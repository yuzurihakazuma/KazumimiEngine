#pragma once
// =====================================================================
//  GoalRing：ゴールの輪（クリア演出 企画書）。卵の殻をかたどった段ボールの輪で、くぐると殻が割れて開く。
//   ・置き場所：マップのゴールの点から、レールに沿って ringOffset だけずらした所（レールの向きに垂直に立てる）
//   ・くぐった判定：恐竜の足元が、輪の面を「くぐる所の幅・高さの中」で横切った時
//   ・開く：上のふたが、ちょうつがい（割れ目の片側）を軸に、ぽんと跳ねて横へ開く。行き過ぎて戻る
//   モデルは tools/clear/make_clear_assets.py が作る（goal_ring_top.obj / goal_ring_bottom.obj。寸法は clear.json の ring）
//   見た目と判定だけを持つ。くぐった後に何をするかは ClearFlow が決める
// =====================================================================
#include "engine/math/struct.h"
#include "game/clear/ClearSettings.h"

#include <memory>

class Obj3d;
class RailField;

class GoalRing {
public:
    GoalRing();
    ~GoalRing();

    // モデルから見た目を作る（モデルは読み込み済みであること）
    void Initialize();

    // レールの上に置く。ゴールが無いマップでは置かない（IsPlaced が false）
    void Place(const RailField& railField, const ClearRing& shape, float ringOffset);
    bool IsPlaced() const{ return placed_; }

    // 閉じた形へ戻す（プレイ開始・やりなおし）
    void Close();
    // ふたを開き始める
    void Open();
    bool IsOpen() const{ return opening_; }

    // 前のフレームの足元 previous から今の足元 current へ動いた時に、輪をくぐったか。
    //   くぐったなら true と、くぐった向き（レールの進む向きなら +1、逆なら -1）
    bool CheckPass(const Vector3& previous, const Vector3& current, float& outDirection) const;

    void Update(float deltaTime);
    void Draw();

    // 輪の真ん中の足元（レールの上）と、レールの進む向き（水平・長さ 1）
    const Vector3& Position() const{ return position_; }
    const Vector3& Forward() const{ return forward_; }
    int   Rail() const{ return rail_; }
    float RailDistance() const{ return railDistance_; }

private:
    void ApplyLid(); // 開き具合から、ふたの位置と向きを決める

    std::unique_ptr<Obj3d> top_;     // 上のふた
    std::unique_ptr<Obj3d> bottom_;  // 下のうつわ
    ClearRing shape_;
    bool    placed_ = false;
    bool    opening_ = false;
    float   openTime_ = 0.0f;        // 開き始めてからの秒数
    Vector3 position_ {};
    Vector3 forward_ { 1.0f, 0.0f, 0.0f };
    float   yaw_ = 0.0f;             // 輪の向き（モデルの厚みの向きを、レールの向きへ）
    int     rail_ = -1;
    float   railDistance_ = 0.0f;
};
