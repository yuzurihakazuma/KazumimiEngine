#pragma once
// =====================================================================
//  BlockWorldShapes：ワールド空間のブロックの箱（道に沿って傾いた直方体）。
//   卵・吐き出し弾のように、レールに乗らずに飛ぶ物の当たり判定と、エディタのマウスのレイに使う。
//   描画と同じ BlockShape::PoseOnRail（位置・向き・勾配）から箱を作るので、見た目と必ず一致する。
//   道の脇の飾りブロックも対象（見えている物には当たる）。
//   箱は BlockSystem::Update で毎フレーム作り直す＝動くレールにも追従する
// =====================================================================
#include "game/stage/BlockShape.h"

#include <vector>

class BlockWorldShapes {
public:
    struct Box {
        Vector3 center  { 0.0f, 0.0f, 0.0f };
        Vector3 right   { 1.0f, 0.0f, 0.0f }; // 道幅方向
        Vector3 up      { 0.0f, 1.0f, 0.0f }; // 道に垂直な上
        Vector3 forward { 0.0f, 0.0f, 1.0f }; // 進行方向
        Vector3 half    { 0.5f, 0.5f, 0.5f }; // 各軸の半分の長さ（right/up/forward の順）
        int     blockIndex = -1;
        bool    slope = false;                // 斜面：上面が進行方向に沿って傾く
        int     ascend = 1;
    };
    // レイが当たった箱と面
    struct Hit {
        int   blockIndex = -1;
        float distance = 0.0f; // レイの始点からの距離
        int   faceAxis = 1;    // 当たった面の軸（0=道幅方向 / 1=上下 / 2=進行方向）
        int   faceSign = 1;    // 面の向き（+1/-1）
        float halfAlong = 0.5f;
        float halfSide  = 0.5f;
    };

    // ブロック1個ぶんの箱（見た目と同じ位置・向き・実寸）。レールが無効なら false
    static bool MakeBox(const std::vector<SplineRail>& rails, const PlacedBlock& block, Box& outBox);
    // 箱をワイヤーで描く（inflate で少し膨らませる）
    static void DrawWire(const Box& box, const Vector4& color, float inflate);

    // 全ブロックの箱を作り直す
    void Rebuild(const std::vector<PlacedBlock>& blocks, const std::vector<SplineRail>& rails);

    // 球が from→to へ動いた間に箱へ触れたか。触れたら outHitPos に当たる直前の位置を返す
    bool SweepSphere(const Vector3& from, const Vector3& to, float radius, Vector3* outHitPos) const;
    // レイが最初に当たる箱
    bool Raycast(const Vector3& origin, const Vector3& direction, Hit& outHit) const;
    // 全部の箱をワイヤーで描く（当たり判定の表示）
    void DrawAll(const Vector4& color) const;

private:
    // 点と箱の距離の2乗（斜面は上面の傾きを考慮）
    static float DistSqToBox(const Box& box, const Vector3& worldPoint);

    std::vector<Box> boxes_;
};
