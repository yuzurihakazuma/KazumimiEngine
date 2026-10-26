#pragma once
// =====================================================================
//  DiveSequence：箱庭への飛び込み（ステージ選択・飛び込み演出 企画書の5段）。
//    構え(crouch)   … 箱庭の方を向いて、かがむ
//    ジャンプ(jump) … 箱庭の上へ跳ぶ。後半から前転を始める
//    頂点(apex)     … ふわっと止まって、くるくる回る
//    飛び込み(dive) … 回りながら落ち、箱庭と同じ大きさまで縮む
//    着地(absorb)   … 箱庭の中のスタート地点に、小さくなって着地（ステージに入った）。円がそこへ閉じる
//   回転は体の真ん中を軸にする（足元を軸にすると、体が大きく振り回されて見える）。
//   前転は2回転で、着地でちょうど立った向きに戻る
//   長さ・高さ・カメラの位置は resources/worldmap/dive.json（DiveTiming）
// =====================================================================
#include "engine/math/struct.h"
#include "game/worldmap/WorldMapData.h"

class DiveSequence {
public:
    // from＝恐竜の立っている所、target＝着地する所（箱庭の中のスタート）、endScale＝着地した時の大きさ
    void Start(const DiveTiming& timing, const Vector3& from, float yaw, const Vector3& target, float endScale,
               const Vector3& cameraPosition, const Vector3& cameraLook);
    // 着地する所は箱庭と一緒に動く（正面へ戻る・弾む）ので、毎フレーム渡し直す
    void SetTarget(const Vector3& target, float endScale);
    void Update(float deltaTime);

    bool IsActive() const{ return active_; }
    bool IsFinished() const{ return active_ && time_ >= EndTime(); }

    // 恐竜の見た目
    struct Pose {
        Vector3 position {};
        Vector3 rotation {};
        Vector3 scale { 1.0f, 1.0f, 1.0f };
    };
    Pose DinoPose() const;
    void CameraPose(Vector3& outPosition, Vector3& outLook) const;
    Vector3 Target() const{ return target_; }
    // 円の半径（2＝全開）
    float IrisRadius() const;

    // 段の始まり（1回だけ true。音・弾み用）
    bool ConsumeJumped();
    bool ConsumeLanded();

private:
    float EndTime() const;
    float LandTime() const{ return timing_.crouch + timing_.jump + timing_.apex + timing_.dive; }

    DiveTiming timing_;
    bool    active_ = false;
    float   time_ = 0.0f;
    Vector3 from_ {}, target_ {};
    float   endScale_ = 0.1f;
    float   yaw_ = 0.0f;
    Vector3 cameraStart_ {}, lookStart_ {};
    bool    jumpTaken_ = false, landTaken_ = false;
};
