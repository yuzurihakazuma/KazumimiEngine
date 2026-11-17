#pragma once
// =====================================================================
//  GoalGate：ゴールの門（クリア演出 企画書の「輪」を、工作の門にした物）。
//   ・見た目：段ボールの柱・紙の虹のアーチ・てっぺんの大きな卵・ゴールテープ・足元の花や木（モデルは tools/clear/make_goal_set.py）
//   ・置き場所：マップのゴールの点から、レールに沿って gateOffset だけずらした所。入口の面（恐竜が近づいてくる側）を、
//     遊んでいる時のカメラの方へ gateTurn 度だけ回す（真横から見ても、門の面が見える）
//   ・くぐった判定：恐竜の足元が、門の面（レールに垂直）を「くぐる所の幅・高さの中」で横切った時
//   ・開く：テープが左右に切れて根元から垂れ、卵のふたがぽんと跳ねて後ろへ倒れる
//   見た目と判定だけを持つ。くぐった後に何をするかは ClearFlow が決める
// =====================================================================
#include "engine/math/struct.h"
#include "game/clear/ClearSettings.h"

#include <memory>

class Obj3d;
class RailField;

class GoalGate {
public:
    GoalGate();
    ~GoalGate();

    // モデルから見た目を作る（モデルは読み込み済みであること）
    void Initialize();

    // レールの上に置く。ゴールが無いマップでは置かない（IsPlaced が false）。
    //   turnDegrees＝入口の面を、遊んでいる時のカメラ（世界の -Z の側）へ回す角度
    void Place(const RailField& railField, const ClearGate& shape, float gateOffset, float turnDegrees);
    bool IsPlaced() const{ return placed_; }

    // 閉じた形へ戻す（プレイ開始・やりなおし）
    void Close();
    // テープが切れて、卵が割れ始める
    void Open();
    bool IsOpen() const{ return opening_; }

    // 前のフレームの足元 previous から今の足元 current へ動いた時に、門をくぐったか。
    //   くぐったなら true と、くぐった向き（レールの進む向きなら +1、逆なら -1）
    bool CheckPass(const Vector3& previous, const Vector3& current, float& outDirection) const;

    void Update(float deltaTime);
    void Draw();

    // 門の真ん中の足元（レールの上）・レールの進む向き（水平・長さ 1）・門の表の向き（恐竜が抜けていく側。水平・長さ 1）
    const Vector3& Position() const{ return position_; }
    const Vector3& Forward() const{ return forward_; }
    const Vector3& Front() const{ return front_; }
    // 門の横の向き（表から見て右。水平・長さ 1）
    Vector3 Right() const;
    // 卵の真ん中・テープの真ん中（世界）
    Vector3 EggCenter() const;
    Vector3 TapeCenter() const;
    int   Rail() const{ return rail_; }
    float RailDistance() const{ return railDistance_; }

private:
    // 門の中の点（モデルの座標：x＝横・y＝上・z＝表の向き）→ 世界
    Vector3 ToWorld(const Vector3& local) const;
    // 開き具合から、卵のふたとテープの位置と向きを決める
    void ApplyPose();
    // piece を、モデルの中の pivot を軸に local だけ回して置く（lift＝上へ持ち上げる量）
    void PlacePiece(Obj3d* piece, const Vector3& pivot, const Matrix4x4& local, float lift);

    std::unique_ptr<Obj3d> gate_;      // 動かない部分（柱・アーチ・飾り・卵の下半分の巣）
    std::unique_ptr<Obj3d> eggBottom_; // 卵の下半分
    std::unique_ptr<Obj3d> eggTop_;    // 卵のふた
    std::unique_ptr<Obj3d> tapeLeft_;  // ゴールテープ（モデルの +x 側の柱から真ん中まで）
    std::unique_ptr<Obj3d> tapeRight_; // ゴールテープ（-x 側）
    ClearGate shape_;
    bool    placed_ = false;
    bool    opening_ = false;
    float   openTime_ = 0.0f;          // 開き始めてからの秒数
    Vector3 position_ {};
    Vector3 forward_ { 1.0f, 0.0f, 0.0f };
    Vector3 front_ { 1.0f, 0.0f, 0.0f };
    float   yaw_ = 0.0f;               // 門の向き（モデルの +z を、門の表の向きへ）
    int     rail_ = -1;
    float   railDistance_ = 0.0f;
};
