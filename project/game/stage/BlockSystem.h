#pragma once
#include <vector>
#include <cstdint>
#include "engine/math/struct.h"
#include "engine/utils/Level/LevelData.h" // BlockData
#include "game/stage/BlockShape.h"
#include "game/stage/BlockGrid.h"
#include "game/stage/BlockRenderer.h"
#include "game/stage/BlockWorldShapes.h"

class SplineRail;
class Camera;

// =====================================================================
//  BlockSystem（乗れる/ぶつかる1m角ブロック）
//   エディタでレール上に置いたブロック（距離・段数・横ずれ）を実体化する。
//
//   ・座標系：レール空間（レール上の距離 × 上方向の段 × 道幅方向の横ずれ）。
//     プレイヤーも距離＋高さで動くので、当たり判定が変換なしでそのまま噛み合い、
//     動くレール（リフト等）に乗せたブロックも一緒に動く。
//   ・中身は役割ごとに分けてある：
//       BlockShape（形・置き方）/ BlockGrid（セルの索引）/
//       BlockRenderer（一括描画・継ぎ目の花）/ BlockWorldShapes（ワールドの箱：卵・レイ用）
//     ここに残るのは、ブロックの一覧・プレイヤー用のレール空間の当たり判定・？ブロック
// =====================================================================
class BlockSystem {
public:
    static constexpr float kSize         = BlockShape::kSize;
    static constexpr float kSurfaceY     = BlockShape::kSurfaceY;
    static constexpr int   kTypeCount    = BlockShape::kTypeCount;
    static constexpr int   kTypeSpring   = BlockShape::kTypeSpring;
    static constexpr int   kTypeHatena   = BlockShape::kTypeHatena;
    static constexpr int   kTypeCloud    = BlockShape::kTypeCloud;
    static constexpr int   kTypeWide     = BlockShape::kTypeWide;
    static constexpr int   kTypePedestal = BlockShape::kTypePedestal;

    BlockSystem() = default;
    // grid_ が blocks_ を指しているので、コピー・移動はしない
    BlockSystem(const BlockSystem&) = delete;
    BlockSystem& operator=(const BlockSystem&) = delete;

    // 一括描画の準備（モデル生成後に1回だけ呼ぶ）。noiseSrv はディゾルブ用のダミーテクスチャ
    void Initialize(uint32_t noiseSrvIndex){ renderer_.Initialize(noiseSrvIndex); }

    // エディタのブロック配置とレールから実体を作り直す
    void Sync(const std::vector<BlockData>& blockDatas, const std::vector<SplineRail>* rails);
    void Update();                 // 位置・向きの更新（動くレール追従＋インスタンス行列の収集＋ワールドの箱）
    void Draw(const Camera* camera){ renderer_.Draw(camera); }

    // --- レール空間の当たり判定（Player・敵が使う）---
    //   いずれも「道の中心線に重なるブロック」だけを対象にする（横へずらした飾りは当たらない）
    // 足元の支持面：dist に重なるブロックのうち、上面が footY+0.25 以下で一番高いもの（無ければ 0=レール面）
    float GroundHeightAt(int rail, float dist, float footY) const;
    // 体の高さ帯 [bodyBottom, bodyTop]（レール面からの相対高さ）が dist のブロックに横から重なるか。
    //   重なったブロックの占有区間 [outMin, outMax]（体の半径ぶん拡張済み）と
    //   上面の高さ outTop（埋まった時の押し出し先）を返す。
    //   bodyRadius は体の横半径（負=プレイヤーの半径。敵は自分の半径を渡す）
    bool  BlockedAt(int rail, float dist, float bodyBottom, float bodyTop,
                    float* outMin, float* outMax, float* outTop = nullptr,
                    float bodyRadius = -1.0f) const;
    // 頭上の天井：dist に重なるブロックの底面のうち、footY より上で一番低いもの（無ければ大きな値）
    float CeilingHeightAt(int rail, float dist, float footY) const;
    // 今の足元を支えているブロックの種類（-1=レール面/ブロックなし）。ジャンプ台の判定に使う
    int   SupportTypeAt(int rail, float dist, float footY) const;

    // --- ワールド空間の当たり判定（卵・吐き出し弾など、レールに乗らずに飛ぶ物が使う）---
    // 球が from→to へ動いた間にブロックへ触れたか。触れたら outHitPos に当たる直前の位置を返す
    bool  SweepSphere(const Vector3& from, const Vector3& to, float radius, Vector3* outHitPos = nullptr) const{
        return shapes_.SweepSphere(from, to, radius, outHitPos);
    }

    // --- エディタ用：マウスのレイが最初に当たるブロックと、その面 ---
    struct RayHit {
        BlockData cell;          // 当たったブロック（rail/dist/level/side/type）
        float distance = 0.0f;   // レイの始点からの距離
        int   faceAxis = 1;      // 当たった面の軸（0=道幅方向 / 1=上下 / 2=進行方向）
        int   faceSign = 1;      // 面の向き（+1/-1）
        float halfAlong = 0.5f;  // 当たったブロックの進行方向の半幅（隣のセルを求める用）
        float halfSide  = 0.5f;  // 当たったブロックの道幅方向の半幅
    };
    bool  Raycast(const Vector3& origin, const Vector3& direction, RayHit& outHit) const;

    // 当たり判定の箱をワイヤーで描く（デバッグ表示用）
    void  DrawHitShapes(const Vector4& color) const{ shapes_.DrawAll(color); }

    // エディタ用：指定セルに type のブロックを置いた時の形を、道に沿った向きのワイヤーで描く
    //   （配置のゴースト表示。実際のブロックと同じ位置・向き・大きさになる）。inflate で少し膨らませる
    void  DrawCellGhost(int rail, float dist, int level, float side, int type,
                        const Vector4& color, float inflate = 0.0f) const;
    // セルの中心のワールド座標（レールが無効なら false）
    bool  CellCenter(int rail, float dist, int level, float side, Vector3& outCenter) const;

    // --- エディタの2D表示用：ブロックの形の決まりを公開する ---
    // 見た目の進行方向の半分の長さ（ゆるい斜面・横長・台座は1m、他は0.5m）
    static float VisualHalfAlong(int type){ return BlockShape::FootprintHalf(type); }
    static bool  IsSlope(int type){ return BlockShape::IsSlope(type); }
    // そのセルにある斜面の登り方向（+1=距離が増える側が高い / -1=逆）。ブロックが無ければ +1
    int   AscendAt(int rail, float dist, int level, float side) const;

    // --- ？ブロック（頭突きでコイン）---
    // 頭をぶつけた時に Player が呼ぶ：ぶつけた先が未使用の？ブロックならコインを出す
    void  NotifyHeadBump(int rail, float dist, float headY);
    // 頭突きで出たコインのワールド位置（1フレーム分。シーンが取り出して演出とカウントに使う）
    bool  ConsumeBumpCoin(Vector3& outPos);
    // Play開始時：？ブロックを全部未使用に戻す
    void  ResetPlay();

private:
    // 斜面の登り方向を隣のブロックから決める（高い側が隣のブロックへ向く）
    int   SlopeAscendAt(int rail, float dist, int level, float side) const;
    // 足元の支持面（一番高い上面）とその持ち主の種類
    void  FindSupport(int rail, float dist, float footY, float& outTop, int& outType) const;
    // 見た目を作り直す（配置・？ブロックの使用状態が変わった時）
    void  RebuildLooks(){ renderer_.Rebuild(blocks_, grid_); }

    std::vector<PlacedBlock> blocks_;
    const std::vector<SplineRail>* rails_ = nullptr; // シーンの RailField が所有（借り物）
    BlockGrid        grid_;
    BlockRenderer    renderer_;
    BlockWorldShapes shapes_;
    std::vector<Vector3> bumpCoinQueue_; // 頭突きで出たコインの位置（シーンが ConsumeBumpCoin で取り出す）
};
