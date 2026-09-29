#include "BlockSystem.h"
#include "engine/rail/SplineRail.h"
#include <algorithm>
#include <cmath>

namespace {
    const float kHalf = BlockShape::kHalf;
    const float kPlayerRadius  = 0.30f; // プレイヤーの横当たり半径
    const float kStepTolerance = 0.25f; // これ以下の段差は支持面として拾う（小さなガタつき吸収）
    // 支持面（乗れる範囲）はブロックの実寸＋わずかな縁の許容だけ。
    //   壁当たり半径をそのまま使うと1マス空きの落とし穴を歩いて渡れてしまう
    const float kSupportEdge = 0.10f;
    // 当たり判定で見るセルの範囲（±2セル）：斜面26°・横長・台座（半幅1m）は、レール編集後に
    //   距離が半端な値へ引き直されると ±1セルの窓から漏れて「見えているのに当たらない」ことがあった
    const int kQueryCells = 2;

    // プレイヤーは道の中心線上を進むので、横へずらしたブロック（壁の飾り等）は当たらない。
    //   side=0 のみ当たる（1mずらすと 1.0 > 0.8 で判定から外れる）
    bool OverlapsCenterLine(float side){ return std::abs(side) < kHalf + kPlayerRadius; }
}

void BlockSystem::Sync(const std::vector<BlockData>& blockDatas, const std::vector<SplineRail>* rails){
    rails_ = rails;
    blocks_.clear();
    if ( rails_ ) {
        for ( const auto& data : blockDatas ) {
            if ( data.rail < 0 || data.rail >= ( int ) rails_->size() ) continue;
            const SplineRail& rail = ( *rails_ )[data.rail];
            if ( rail.nodes.size() < 2 ) continue;
            // レールが短くなって範囲外に残ったブロックは末尾セルへ寄せる。
            //   見た目と当たり判定が同じ dist を使う（食い違うと「見えるのに当たらない」が起きる）
            float dist = data.dist;
            if ( dist > rail.GetLength() ) { dist = std::floor(rail.GetLength()); }
            if ( dist < 0.0f ) { dist = 0.0f; }
            blocks_.push_back({ data.rail, dist, data.level, data.side, data.type });
        }
    }
    grid_.Rebuild(blocks_);
    // 斜面の登り方向を隣のブロックから自動決定（例：ブロックの手前に斜面を置くと、そのブロックへ登るスロープになる）
    for ( PlacedBlock& block : blocks_ ) {
        if ( BlockShape::IsSlope(block.type) ) { block.ascend = SlopeAscendAt(block.rail, block.dist, block.level, block.side); }
    }
    RebuildLooks();
    Update(); // 生成直後に位置を確定（原点に一瞬出るのを防ぐ）
}

int BlockSystem::SlopeAscendAt(int rail, float dist, int level, float side) const{
    const int cell = BlockGrid::CellOf(dist);
    if ( grid_.Has(rail, cell + 1, level, side) ) return +1;
    if ( grid_.Has(rail, cell - 1, level, side) ) return -1;
    return +1; // 隣がいなければ進行方向へ登る向き
}

void BlockSystem::Update(){
    if ( !rails_ ) return;
    renderer_.Update(blocks_, *rails_);
    shapes_.Rebuild(blocks_, *rails_); // 卵などが使うワールド空間の箱も、同じ位置・向きで作り直す
}

bool BlockSystem::Raycast(const Vector3& origin, const Vector3& direction, RayHit& outHit) const{
    BlockWorldShapes::Hit hit;
    if ( !shapes_.Raycast(origin, direction, hit) ) return false;
    const PlacedBlock& block = blocks_[hit.blockIndex];
    outHit.cell.rail  = block.rail;
    outHit.cell.dist  = block.dist;
    outHit.cell.level = block.level;
    outHit.cell.side  = block.side;
    outHit.cell.type  = block.type;
    outHit.distance   = hit.distance;
    outHit.faceAxis   = hit.faceAxis;
    outHit.faceSign   = hit.faceSign;
    outHit.halfAlong  = hit.halfAlong;
    outHit.halfSide   = hit.halfSide;
    return true;
}

int BlockSystem::AscendAt(int rail, float dist, int level, float side) const{
    int found = grid_.Find(rail, BlockGrid::CellOf(dist), level, side);
    return ( found >= 0 ) ? blocks_[found].ascend : +1;
}

bool BlockSystem::CellCenter(int rail, float dist, int level, float side, Vector3& outCenter) const{
    if ( !rails_ ) return false;
    BlockWorldShapes::Box box;
    if ( !BlockWorldShapes::MakeBox(*rails_, PlacedBlock { rail, dist, level, side, 0 }, box) ) return false;
    outCenter = box.center;
    return true;
}

void BlockSystem::DrawCellGhost(int rail, float dist, int level, float side, int type,
                                const Vector4& color, float inflate) const{
    if ( !rails_ ) return;
    PlacedBlock cell { rail, dist, level, side, type };
    // 斜面の向きは、置いた後と同じ決め方（隣のブロックの方へ登る）で予告する
    if ( BlockShape::IsSlope(type) ) { cell.ascend = SlopeAscendAt(rail, dist, level, side); }
    BlockWorldShapes::Box box;
    if ( BlockWorldShapes::MakeBox(*rails_, cell, box) ) { BlockWorldShapes::DrawWire(box, color, inflate); }
}

// 頭をぶつけた時に Player が呼ぶ。ぶつけた先が未使用の？ブロックならコインを出して使用済みにする
void BlockSystem::NotifyHeadBump(int rail, float dist, float headY){
    bool changed = false;
    grid_.ForEachNear(rail, dist, 1, [&](int index){
        PlacedBlock& block = blocks_[index];
        if ( block.type != kTypeHatena || block.used ) return;
        if ( !OverlapsCenterLine(block.side) ) return;
        if ( std::abs(block.dist - dist) > kHalf + kPlayerRadius ) return;
        float bottom = ( float ) block.level * kSize;
        if ( std::abs(bottom - headY) > 0.15f ) return; // 頭が当たった面のブロックだけ
        block.used = true;
        changed = true;
        // コインの飛び出し位置＝ブロックの上面中央
        Vector3 pos {}; float yaw = 0.0f;
        if ( rails_ && BlockShape::PoseOnRail(*rails_, block, pos, yaw) ) {
            bumpCoinQueue_.push_back({ pos.x, pos.y + kSize + 0.3f, pos.z });
        }
    });
    if ( changed ) { RebuildLooks(); } // 使用済みの見た目（灰色）へ差し替え
}

bool BlockSystem::ConsumeBumpCoin(Vector3& outPos){
    if ( bumpCoinQueue_.empty() ) return false;
    outPos = bumpCoinQueue_.back();
    bumpCoinQueue_.pop_back();
    return true;
}

// Play開始時：？ブロックを全部未使用へ戻す
void BlockSystem::ResetPlay(){
    bool changed = false;
    for ( auto& block : blocks_ ) {
        if ( block.used ) { block.used = false; changed = true; }
    }
    bumpCoinQueue_.clear();
    if ( changed ) { RebuildLooks(); }
}

// 足元の支持面（レール空間）。footY より少し上までの上面を「乗れる面」として拾い、一番高いものを選ぶ。
//   斜面は位置に応じた表面高さ（＝そのまま歩いて登り降りできる）
void BlockSystem::FindSupport(int rail, float dist, float footY, float& outTop, int& outType) const{
    outTop = 0.0f;  // レール面
    outType = -1;
    grid_.ForEachNear(rail, dist, kQueryCells, [&](int index){
        const PlacedBlock& block = blocks_[index];
        if ( !OverlapsCenterLine(block.side) ) return;
        float reach = BlockShape::FootprintHalf(block.type) + kSupportEdge; // 乗れるのは実寸＋縁だけ
        if ( std::abs(block.dist - dist) > reach ) return;
        float top = block.SurfaceHeightAt(dist);
        if ( top <= footY + kStepTolerance && top > outTop ) { outTop = top; outType = block.type; }
    });
}

float BlockSystem::GroundHeightAt(int rail, float dist, float footY) const{
    float top = 0.0f; int type = -1;
    FindSupport(rail, dist, footY, top, type);
    return top;
}

int BlockSystem::SupportTypeAt(int rail, float dist, float footY) const{
    float top = 0.0f; int type = -1;
    FindSupport(rail, dist, footY, top, type);
    return type;
}

// 体の高さ帯がブロックへ横から重なるか（支持面として乗っている場合は重ならない）
bool BlockSystem::BlockedAt(int rail, float dist, float bodyBottom, float bodyTop,
                            float* outMin, float* outMax, float* outTop, float bodyRadius) const{
    const float radius = ( bodyRadius >= 0.0f ) ? bodyRadius : kPlayerRadius;
    bool blocked = false;
    grid_.ForEachNear(rail, dist, kQueryCells, [&](int index){
        if ( blocked ) return;
        const PlacedBlock& block = blocks_[index];
        if ( !OverlapsCenterLine(block.side) ) return;
        if ( BlockShape::IsSlope(block.type) ) return;    // 斜面は壁にならない（歩いて登る）
        if ( block.type == kTypeCloud ) return;           // すり抜け床は横から通り抜けられる
        // 型別の footprint（横長2m/台座2×2m は半幅1.0m）で壁の届く範囲を決める
        float reach = BlockShape::FootprintHalf(block.type) + radius;
        if ( std::abs(block.dist - dist) > reach ) return;
        float bottom = ( float ) block.level * kSize;
        float top    = bottom + kSize;
        // 面ぴったり（乗っている/頭が触れているだけ）は重なり扱いしない。
        // 上側の許容はステップ許容と同じ幅にする：横長/台座は壁帯が広い（半幅1.3m）ため、
        // 斜面で登り切る直前（残り0.25m以内）に側面へ引っかかって坂の途中で止まるのを防ぐ
        if ( bodyBottom < top - kStepTolerance && bodyTop > bottom + 0.05f ) {
            if ( outMin ) { *outMin = block.dist - reach; }
            if ( outMax ) { *outMax = block.dist + reach; }
            if ( outTop ) { *outTop = top; }
            blocked = true;
        }
    });
    return blocked;
}

// 頭上の天井（footY より上にあるブロックの底面のうち一番低いもの）
float BlockSystem::CeilingHeightAt(int rail, float dist, float footY) const{
    float best = 1e9f;
    grid_.ForEachNear(rail, dist, kQueryCells, [&](int index){
        const PlacedBlock& block = blocks_[index];
        if ( !OverlapsCenterLine(block.side) ) return;
        if ( BlockShape::IsSlope(block.type) ) return;    // 斜面の下は薄いので頭ぶつけ無し
        if ( block.type == kTypeCloud ) return;           // すり抜け床は下から通り抜けられる
        // 型別の footprint（横長・台座は半幅1.0m）。外縁の下でも頭がぶつかるように
        if ( std::abs(block.dist - dist) > BlockShape::FootprintHalf(block.type) + kPlayerRadius ) return;
        float bottom = ( float ) block.level * kSize;
        if ( bottom > footY + 0.1f && bottom < best ) { best = bottom; }
    });
    return best;
}
