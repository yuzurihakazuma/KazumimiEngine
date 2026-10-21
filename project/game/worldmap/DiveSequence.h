#pragma once
// =====================================================================
//  DiveSequence：箱庭への飛び込み（ステージ選択・飛び込み演出 企画書の5段）。
//    構え(crouch)   … その場でかがむ
//    ジャンプ(jump) … 箱庭の上の頂点へ跳ぶ。後半から前転を始める
//    頂点(apex)     … 前転を終えて、頭を下に向ける
//    飛び込み(dive) … 頭から箱庭の真ん中へ。縮みながら加速
//    吸い込み(absorb)… 恐竜は消え、箱庭が水面のように弾む。円を箱庭の真ん中へ閉じる
//   長さ・高さ・カメラの位置は resources/worldmap/dive.json（DiveTiming）。ここは形だけ持つ
// =====================================================================
#include "engine/math/struct.h"
#include "game/worldmap/WorldMapData.h"

class DiveSequence {
public:
    // from＝恐竜の立っている所、center＝箱庭の真ん中、camera*＝始まった時のカメラ
    void Start(const DiveTiming& timing, const Vector3& from, float yaw, const Vector3& center,
               const Vector3& cameraPosition, const Vector3& cameraLook);
    void Update(float deltaTime);

    bool IsActive() const{ return active_; }
    bool IsFinished() const{ return active_ && time_ >= EndTime(); }

    // 恐竜の見た目（visible=false の間は描かない）
    struct Pose {
        Vector3 position {};
        Vector3 rotation {};
        Vector3 scale { 1.0f, 1.0f, 1.0f };
        bool    visible = true;
    };
    Pose DinoPose() const;
    void CameraPose(Vector3& outPosition, Vector3& outLook) const;
    // 円の半径（2＝全開）
    float IrisRadius() const;

    // 段の始まり（1回だけ true。音・弾み用）
    bool ConsumeJumped();
    bool ConsumeAbsorbed();

private:
    float EndTime() const;

    DiveTiming timing_;
    bool    active_ = false;
    float   time_ = 0.0f;
    Vector3 from_ {}, apex_ {}, center_ {};
    float   yaw_ = 0.0f;
    float   divePitch_ = 3.14159265f; // 頭から向かう角度（頂点→真ん中の向き）
    Vector3 cameraStart_ {}, lookStart_ {};
    bool    jumped_ = false, absorbed_ = false;
    bool    jumpTaken_ = false, absorbTaken_ = false;
};
