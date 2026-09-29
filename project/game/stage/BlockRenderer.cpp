#include "game/stage/BlockRenderer.h"
#include "game/stage/BlockGrid.h"
#include "engine/3d/obj/Obj3d.h"
#include "engine/graphics/InstancedGroup.h"

#include <algorithm>
#include <cmath>

namespace {
    const uint32_t kMaxPerLook = 4000; // 見た目グループごとの最大インスタンス数

    // 種類 → モデル名（GamePlayScene::LoadResources が先に読み込む）
    const char* kTypeModelNames[BlockShape::kTypeCount] = {
        "craftSponge",   // 0: 1m角スポンジ
        "craftLayer",    // 1: 1m角の段ボール層
        "craftSlope45",  // 2: 斜面45°（巻き段ボール筒）
        "craftSlope26",  // 3: ゆるい斜面26.6°
        "craftSpring",   // 4: ジャンプ台（緑のスポンジ。上に飛び乗ると跳ねる）
        "craftHatena",   // 5: ？ブロック（金色。下から頭突きでコイン）
        "craftCloud",    // 6: すり抜け床（白。上にだけ乗れる薄い板）
        "craftWide",     // 7: 横長ブロック（進行方向2m×1m。スポンジ側面）
        "craftPedestal", // 8: 台座ブロック（2×2m×高さ1m。段ボール層側面）
    };
    const char* kFlowerModelNames[2] = { "flowerOrange", "flowerWhite" };

    // セル座標から決まる擬似乱数（花の有無・向きに使う。毎回同じ結果＝チラつかない）
    uint32_t CellHash(int rail, int cell, int level, int side){
        uint32_t h = ( uint32_t ) ( rail * 73856093 ) ^ ( uint32_t ) ( cell * 19349663 )
                   ^ ( uint32_t ) ( level * 83492791 ) ^ ( uint32_t ) ( ( side + 8 ) * 2971215073u );
        h ^= h >> 13; h *= 0x85ebca6bu; h ^= h >> 16;
        return h;
    }
}

BlockRenderer::BlockRenderer() = default;
BlockRenderer::~BlockRenderer() = default;

void BlockRenderer::Initialize(uint32_t noiseSrvIndex){
    auto initLook = [&](LookGroup& look, const char* modelName){
        look.batch = std::make_unique<InstancedGroup>();
        look.batch->Initialize(modelName, kMaxPerLook);
        look.batch->SetNoiseTexture(noiseSrvIndex);
    };
    for ( int t = 0; t < BlockShape::kTypeCount; ++t ) { initLook(typeLooks_[t], kTypeModelNames[t]); }
    initLook(usedHatenaLook_, "craftHatenaUsed");
    for ( int f = 0; f < 2; ++f ) { initLook(flowerLooks_[f], kFlowerModelNames[f]); }
}

void BlockRenderer::EnsurePool(size_t count){
    // Obj3d は行列計算にしか使わない（描画は InstancedGroup）ので、モデルは何でもよい
    while ( objPool_.size() < count ) { objPool_.push_back(Obj3d::Create("craftSponge")); }
}

// 並んだブロックのつなぎ目へ紙花を自動配置する。
//   本家（ヨッシークラフトワールド）の「継ぎ目を花で隠す」演出：2個以上並べると勝手に咲く。
//   右隣（dist+1）に同じ段・同じ横位置のブロックがあり、両方とも上が空いている継ぎ目に、
//   セルハッシュで約3/4だけ咲かせる
void BlockRenderer::PlaceFlowers(const std::vector<PlacedBlock>& blocks, const BlockGrid& grid){
    flowers_.clear();
    for ( const PlacedBlock& block : blocks ) {
        if ( BlockShape::IsSlope(block.type) ) continue; // 斜面の上面は斜めなので花は咲かせない
        int cell = BlockGrid::CellOf(block.dist);
        int rightIdx = grid.Find(block.rail, cell + 1, block.level, block.side);
        if ( rightIdx < 0 || BlockShape::IsSlope(blocks[rightIdx].type) ) continue; // 右隣なし/斜面
        if ( grid.Has(block.rail, cell, block.level + 1, block.side) ) continue;     // 自分の上が塞がり
        if ( grid.Has(block.rail, cell + 1, block.level + 1, block.side) ) continue; // 隣の上が塞がり
        uint32_t hash = CellHash(block.rail, cell, block.level, ( int ) std::lround(block.side));
        if ( ( hash & 3 ) == 0 ) continue; // 1/4は咲かせない（並びすぎ防止のゆらぎ）
        FlowerSpot flower;
        flower.rail = block.rail;
        flower.dist = block.dist + 0.5f; // 継ぎ目（セルの中間）
        flower.side = block.side;
        flower.topY = ( float ) block.level * BlockShape::kSize + BlockShape::kSize;
        flower.yawOffset = ( ( float ) ( hash % 628 ) ) * 0.01f; // 0〜2π
        flower.kind = ( hash >> 4 ) & 1;
        flowers_.push_back(flower);
    }
}

void BlockRenderer::Rebuild(const std::vector<PlacedBlock>& blocks, const BlockGrid& grid){
    for ( auto& look : typeLooks_ )   { look.objs.clear(); look.blockIndices.clear(); }
    usedHatenaLook_.objs.clear(); usedHatenaLook_.blockIndices.clear();
    for ( auto& look : flowerLooks_ ) { look.objs.clear(); look.blockIndices.clear(); }

    PlaceFlowers(blocks, grid);
    EnsurePool(blocks.size() + flowers_.size());

    // ブロック本体を種類ごとに振り分け（使用済みの？ブロックは灰色グループへ）
    for ( int i = 0; i < ( int ) blocks.size(); ++i ) {
        int type = std::clamp(blocks[i].type, 0, BlockShape::kTypeCount - 1);
        LookGroup& look = ( type == BlockShape::kTypeHatena && blocks[i].used ) ? usedHatenaLook_ : typeLooks_[type];
        look.objs.push_back(objPool_[i].get());
        look.blockIndices.push_back(i);
    }
    // 花はプールの後半を使う（blockIndices は -1-花番号 で区別）
    for ( int f = 0; f < ( int ) flowers_.size(); ++f ) {
        LookGroup& look = flowerLooks_[flowers_[f].kind];
        look.objs.push_back(objPool_[blocks.size() + f].get());
        look.blockIndices.push_back(-1 - f);
    }
}

void BlockRenderer::UpdateLook(LookGroup& look, const std::vector<PlacedBlock>& blocks,
                               const std::vector<SplineRail>& rails){
    for ( size_t i = 0; i < look.objs.size(); ++i ) {
        Obj3d* obj = look.objs[i];
        if ( !obj ) continue;
        int idx = look.blockIndices[i];
        if ( idx >= 0 ) {
            // ブロック本体
            const PlacedBlock& block = blocks[idx];
            Vector3 pos {}; float yaw = 0.0f; float pitch = 0.0f;
            if ( !BlockShape::PoseOnRail(rails, block, pos, yaw, &pitch) ) continue;
            if ( block.type == BlockShape::kTypeCloud ) {
                // すり抜け床：セル上端に薄い板として置く（当たりの支持面＝セル上端と一致）
                pos.y += 0.7f;
                obj->SetScale({ 1.0f, 0.3f, 1.0f });
            } else {
                obj->SetScale({ 1.0f, 1.0f, 1.0f }); // プール使い回しなので毎回戻す
            }
            if ( BlockShape::IsSlope(block.type) ) {
                // 斜面モデルは「高い側の縁が原点」なので、走行方向へ半分ずらしてセル中央に合わせ、
                // 登り方向が -側 なら180°回して高い側を隣のブロックへ向ける
                const float kPi = 3.14159265f;
                float run = BlockShape::FootprintHalf(block.type) * 2.0f;
                float forward = ( float ) block.ascend * run * 0.5f;
                pos.x += std::sin(yaw) * forward;
                pos.z += std::cos(yaw) * forward;
                pos.y += -std::tan(pitch) * forward; // 原点も勾配に沿って上下（レール相対の高さを保つ）
                if ( block.ascend < 0 ) { yaw += kPi; pitch = -pitch; } // 180°回すと勾配の向きも反転
            }
            obj->SetTranslation(pos);
            // ピッチ＝道の勾配。回転は Rx→Ry の順に掛かるので、モデルの左右軸で傾けてから向きを合わせる
            obj->SetRotation({ pitch, yaw, 0.0f });
        } else {
            // 花。継ぎ目のブロック上面に立てる
            const FlowerSpot& flower = flowers_[-1 - idx];
            PlacedBlock asBlock { flower.rail, flower.dist, 0, flower.side, 0 };
            Vector3 pos {}; float yaw = 0.0f;
            if ( !BlockShape::PoseOnRail(rails, asBlock, pos, yaw) ) continue;
            pos.y += flower.topY;
            obj->SetTranslation(pos);
            obj->SetRotation({ 0.0f, yaw + flower.yawOffset, 0.0f });
        }
        obj->Update();
    }
    // 計算済みの行列をまとめてGPU送信用配列へ写す
    if ( look.batch ) { look.batch->Update(look.objs); }
}

void BlockRenderer::Update(const std::vector<PlacedBlock>& blocks, const std::vector<SplineRail>& rails){
    for ( auto& look : typeLooks_ )   { UpdateLook(look, blocks, rails); }
    UpdateLook(usedHatenaLook_, blocks, rails);
    for ( auto& look : flowerLooks_ ) { UpdateLook(look, blocks, rails); }
}

void BlockRenderer::Draw(const Camera* camera){
    for ( auto& look : typeLooks_ ) { if ( look.batch ) { look.batch->Draw(camera); } }
    if ( usedHatenaLook_.batch ) { usedHatenaLook_.batch->Draw(camera); }
    for ( auto& look : flowerLooks_ ) { if ( look.batch ) { look.batch->Draw(camera); } }
}
