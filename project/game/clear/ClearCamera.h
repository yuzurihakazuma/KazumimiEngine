#pragma once
// =====================================================================
//  ClearCamera：クリア演出のカメラ（クリア演出 企画書）。横からの追従の位置から、ゴール正面の決めた位置へ回り込む。
//   ・位置は、回り込む中心（恐竜が着地する所）のまわりを「角度・距離・高さ」で補間する
//     （位置をまっすぐ補間すると、中心をかすめて近づいてから離れる。角度で補間すると、弧を描いて回り込みに見える）
//   ・見る点と画角も一緒に補間する
//   どこへ回り込むかは、呼ぶ側（ClearSequence）がステージごとの値から決めて渡す
// =====================================================================
#include "engine/math/struct.h"

class Camera;

class ClearCamera {
public:
    // 回り込みを始める：今のカメラ（位置・向き・画角）を始まりにして、pivot のまわりを回る。この後 SetEnd で終点を決める
    void Begin(const Camera& camera, const Vector3& pivot);
    // 終点を決める（途中で決め直してもよい。インスペクターで直した値を、その場で確かめられる）。
    //   endLook＝終わりに見る点、endFovY＝終わりの画角
    void SetEnd(const Vector3& endPosition, const Vector3& endLook, float endFovY);
    // 進み具合 progress（0〜1）の位置へカメラを置く（始めと終わりはゆっくり）
    void Apply(Camera* camera, float progress) const;
    // 終わりの位置へ置く（リザルトの間など、動かさない時）
    void ApplyEnd(Camera* camera) const{ Apply(camera, 1.0f); }

    const Vector3& EndPosition() const{ return endPosition_; }

private:
    // 中心のまわりの位置（水平の角度・水平の距離・高さ）
    struct Orbit {
        float angle = 0.0f;
        float radius = 0.0f;
        float height = 0.0f;
    };
    static Orbit ToOrbit(const Vector3& position, const Vector3& pivot);

    Vector3 pivot_ {};
    Orbit   start_ {}, end_ {};
    Vector3 startLook_ {}, endLook_ {};
    Vector3 endPosition_ {};
    float   startFovY_ = 0.78f, endFovY_ = 0.78f;
};
