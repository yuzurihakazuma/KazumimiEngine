#pragma once
// =====================================================================
//  ToyTrain：おもちゃの汽車の形（機関車・客車）。ワールドマップの箱庭の汽車（DioramaTrain）と、
//   ステージの列車（TrainVisuals）で同じ形を使う。フェルトの色の箱・筒・球で組む（モデルの素材は要らない）
//   部品の位置：x＝右・y＝上（0＝線路の上の面＝車輪の下）・z＝前。単位は m
// =====================================================================
#include "engine/math/struct.h"

#include <string>
#include <vector>

namespace ToyTrain {
    struct Part {
        const char* shape;  // "box"＝1m の箱 / "tube"＝直径1m・高さ1m の筒 / "ball"＝球
        Vector3 color;      // sRGB
        Vector3 offset;     // 車両の中の位置
        Vector3 size;       // 見た目の大きさ（球は直径）
        float   pitch;      // 筒を前後に寝かせる時 π/2
    };

    std::vector<Part> Engine();          // 機関車（緑の釜・赤い運転室・黒い煙突）
    std::vector<Part> Coach(int index);  // 客車（0＝黄色に赤い屋根 / 1＝水色に黄色い屋根 をくり返す）

    // 形と色のフェルトのモデルを用意して、その名前を返す（同じ物は1回だけ作る）
    std::string FeltModel(const char* shape, const Vector3& color);
    // モデルに掛ける大きさ（球のモデルは半径1m なので半分）
    Vector3 ModelScale(const Part& part);

    // 寸法
    constexpr float kEngineFront   = 1.75f; // 機関車の真ん中から前のはし（ライト）まで
    constexpr float kEngineBack    = 1.70f; // 機関車の真ん中からうしろのはし（運転室の屋根）まで
    constexpr float kCoachLength   = 3.0f;  // 客車の長さ（屋根は少し長い）
    constexpr float kCoachGap      = 0.3f;  // 車両のすき間
    constexpr float kCoachRoofTop  = 2.07f; // 客車の屋根の上の面（ステージの列車ではここを歩く）
    constexpr float kWheelX        = 0.8f;  // 車輪の横の位置
    const Vector3   kChimneyTop { 0.0f, 2.45f, 1.0f }; // 煙突の口（機関車の中）
}
