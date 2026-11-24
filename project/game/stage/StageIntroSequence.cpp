#include "game/stage/StageIntroSequence.h"
#include "engine/utils/JsonFile.h"
#include "engine/math/MathUtil.h"

#include "engine/3d/obj/Obj3d.h"
#include "engine/camera/Camera.h"
#include "engine/particle/ParticleManager.h"
#include "engine/utils/Easing.h"
#include "externals/nlohmann/json.hpp"
#include "game/camera/PlayCameraController.h"
#include "game/egg/EggSystem.h"
#include "game/player/Player.h"
#include "game/player/PlayerAvatar.h"
#include "game/player/PlayerFacing.h"
#include "game/rail/RailField.h"

#include <cmath>

using namespace KazumimiEngine;

namespace {
    constexpr float kReturnTime = 0.6f;       // カメラが後ろへ戻る時間(秒)
    constexpr float kFollowLookHeight = PlayCameraController::kLookHeight; // 通常カメラが見る高さ
    constexpr float kIntroTurnRate = 8.0f;    // 演技の間の向きを変える速さ（約0.35秒。通常は 18 で一瞬に見える）
    // IntroJump クリップの中の時刻（tools/stage_intro/make_intro_clips.py と同じ）
    constexpr float kTakeOffTime = 0.26f;     // 跳び上がる
    constexpr float kLandTime = 0.78f;        // 着地する
    // 上から落ちてくる（ワールドマップから飛び込んだ時）
    constexpr float kDropHeight = 7.0f;       // 落ち始めの高さ(m)
    constexpr float kDropFallTime = 0.35f;    // 落ちる時間(秒)
    constexpr float kDropLandTime = 0.25f;    // 着地でつぶれて戻る時間(秒)
    // ロールの転がり方
    constexpr float kPushSpeed = 0.025f;      // 押している間(m/秒)。手が当たったまま、ほんの少しずつ前へ（速いと手から離れていく）
    constexpr float kRollAccel = 9.0f;        // 転がり出してからの加速（約1秒で 4.5m）
    constexpr float kRollMaxSpeed = 9.0f;     // その後の一定の速さ(m/秒)
    constexpr float kRollShrink = 0.3f;       // 転がるほど細くなる割合（布を敷いていく感じ）
    constexpr float kRollShrinkDistance = 12.0f;
    constexpr float kRollHideDistance = 40.0f; // ここまで転がったら消す（画面の外）
    constexpr const char* kRollModel = "introRoll";
    constexpr const char* kSparkleYellow = "IntroSparkleY";
    constexpr const char* kSparklePink = "IntroSparkleP";
}

StageIntroSequence::StageIntroSequence() = default;
StageIntroSequence::~StageIntroSequence() = default;

void StageIntroSequence::Initialize(){
    roll_ = Obj3d::Create(kRollModel);
}

void StageIntroSequence::LoadSettings(const std::string& stage){
    nlohmann::json content;
    if ( !JsonFile::Load("resources/stage_intro/" + stage + ".json", content) ) { return; }
    const nlohmann::json camera = content.value("introCamera", nlohmann::json::object());
    distance_ = camera.value("distance", distance_);
    side_ = camera.value("side", side_);
    height_ = camera.value("height", height_);
    lookHeight_ = camera.value("lookHeight", lookHeight_);
    const nlohmann::json roll = content.value("introRoll", nlohmann::json::object());
    rollAhead_ = roll.value("ahead", rollAhead_);
    rollRadius_ = roll.value("radius", rollRadius_);
}

void StageIntroSequence::Reset(const Context& context){
    if ( context.player ) {
        context.player->SetAutoPilot(false);
        context.player->SetFaceOverride(false, 0.0f);
        context.player->SetTurnRate(PlayerFacing::kDefaultTurnRate);
    }
    if ( context.avatar ) {
        context.avatar->ClearIntroClip();
        context.avatar->SetDropPose(0.0f, 1.0f);
    }
    state_ = State::None;
    drop_ = Drop::None;
    rollState_ = Roll::Hidden;
    unrolling_ = false;
}

float StageIntroSequence::YawToCamera(const Vector3& position) const{
    return std::atan2(introCamera_.x - position.x, introCamera_.z - position.z);
}

// =====================================================================
//  上から落ちてくる（ワールドマップから飛び込んで来た時）の状態クラス。見た目だけ。足元はずっとスタートの上
// =====================================================================
class StageIntroSequence::DropState {
public:
    virtual ~DropState() = default;
    // 演技が始まった時に、体を置く高さ（既定＝地面の上）
    virtual float StartHeight() const { return 0.0f; }
    // 合図「落ちてくる」（既定＝何もしない）
    virtual void OnDropCue(StageIntroSequence& /*sequence*/) const {}
    // 毎フレーム（次の状態へ進むかも、ここで決める）
    virtual void Update(StageIntroSequence& /*sequence*/, const Context& /*context*/, float /*dt*/) const {}
};

// 落下の状態クラスの入れ物
struct StageIntroSequence::Drops {
    // Drop の番号と同じ並びの表
    static const DropState& Of(Drop drop);

    // 落ちてこない（ふつうに立っている）
    class None final : public DropState {};

    // カードが分かれるまで、画面の上の外で待つ。合図で落ち始める
    class Waiting final : public DropState {
    public:
        float StartHeight() const override { return kDropHeight; }
        void OnDropCue(StageIntroSequence& sequence) const override {
            sequence.drop_ = Drop::Falling;
            sequence.dropTime_ = 0.0f;
        }
    };

    // 重力のように加速して落ちる。着いたら、つぶれる状態へ
    class Falling final : public DropState {
    public:
        void Update(StageIntroSequence& sequence, const Context& context, float dt) const override {
            sequence.dropTime_ += dt;
            const float t = ( std::min )( sequence.dropTime_ / kDropFallTime, 1.0f );
            context.avatar->SetDropPose(kDropHeight * ( 1.0f - t * t ), 1.0f + 0.12f * t); // 落ちるほど縦に伸びる
            if ( t >= 1.0f ) {
                sequence.drop_ = Drop::Landing;
                sequence.dropTime_ = 0.0f;
                sequence.SpawnDust(context.eggSystem, context.player->GetPosition(), 0.5f);
            }
        }
    };

    // 着地でつぶれて戻る。戻ったら終わり
    class Landing final : public DropState {
    public:
        void Update(StageIntroSequence& sequence, const Context& context, float dt) const override {
            sequence.dropTime_ += dt;
            const float u = ( std::min )( sequence.dropTime_ / kDropLandTime, 1.0f );
            const float squash = 1.0f - 0.3f * std::sin(u * Math::kPi) * ( 1.0f - 0.4f * u );
            context.avatar->SetDropPose(0.0f, squash);
            if ( u >= 1.0f ) {
                sequence.drop_ = Drop::None;
                context.avatar->SetDropPose(0.0f, 1.0f);
            }
        }
    };
};

const StageIntroSequence::DropState& StageIntroSequence::Drops::Of(Drop drop){
    static const None none {};
    static const Waiting waiting {};
    static const Falling falling {};
    static const Landing landing {};
    static const DropState* const kTable[] = { &none, &waiting, &falling, &landing };
    return *kTable[static_cast<size_t>( drop )];
}

// =====================================================================
//  ロール（巻いた布）の状態クラス：置いてある・押されている・転がっている・隠れている
// =====================================================================
class StageIntroSequence::RollState {
public:
    virtual ~RollState() = default;
    // 描くか（動かすか）
    virtual bool IsVisible() const { return true; }
    // 今の速さ(m/秒)（既定＝止まっている）
    virtual float Speed(const StageIntroSequence& /*sequence*/, float /*dt*/) const { return 0.0f; }
    // 恐竜が押し始めた（既定＝変わらない）
    virtual void OnPushed(StageIntroSequence& /*sequence*/) const {}
};

// ロールの状態クラスの入れ物
struct StageIntroSequence::Rolls {
    // Roll の番号と同じ並びの表
    static const RollState& Of(Roll roll);

    // 画面の外へ出た・まだ置いていない
    class Hidden final : public RollState {
    public:
        bool IsVisible() const override { return false; }
    };

    // スタートの少し先に置いてある。押されたら、少しずつ前へ
    class Placed final : public RollState {
    public:
        void OnPushed(StageIntroSequence& sequence) const override { sequence.rollState_ = Roll::Pushed; }
    };

    // 恐竜に押されて、ゆっくり進む
    class Pushed final : public RollState {
    public:
        float Speed(const StageIntroSequence& /*sequence*/, float /*dt*/) const override { return kPushSpeed; }
    };

    // 転がり出した：加速して画面の外へ
    class Rolling final : public RollState {
    public:
        float Speed(const StageIntroSequence& sequence, float dt) const override {
            return ( std::min )( sequence.rollSpeed_ + kRollAccel * dt, kRollMaxSpeed );
        }
    };
};

const StageIntroSequence::RollState& StageIntroSequence::Rolls::Of(Roll roll){
    static const Hidden hidden {};
    static const Placed placed {};
    static const Pushed pushed {};
    static const Rolling rolling {};
    static const RollState* const kTable[] = { &hidden, &placed, &pushed, &rolling };
    return *kTable[static_cast<size_t>( roll )];
}

// =====================================================================
//  演技の段階ごとの状態クラス。
//   StageIntroSequence は、今の段階を見て分岐しない（Acts::Of(state_) で ActState を引いて、仮想関数を呼ぶだけ）。
//   どの段階へ移るかは、遷移の台本から来る合図（OnCue）が決める。段階ごとの違い（入った時の向きとクリップ・
//   ジャンプの進み・カメラ・「カメラが戻る」合図を受けた時の向き）は、各状態が持つ
// =====================================================================
class StageIntroSequence::ActState {
public:
    virtual ~ActState() = default;
    // 演技の最中か（最中はゲームの時間を止め、プレイヤーの入力の代わりをする）
    virtual bool IsActive() const { return true; }
    // HUD を隠すか
    virtual bool HidesHud() const { return true; }
    // この段階に入った時（向き・演技のクリップ）
    virtual void Enter(StageIntroSequence& /*sequence*/, Player& /*player*/, const Context& /*context*/) const {}
    // プレイヤーの更新の前（ジャンプの進みなど）
    virtual void UpdateInput(StageIntroSequence& /*sequence*/, Player& /*player*/, const Context& /*context*/, float /*dt*/) const {}
    // カメラ（既定＝イントロカメラから、プレイヤーを見る）
    virtual void AimCamera(StageIntroSequence& sequence, Camera& camera, const Context& /*context*/, const Vector3& position, float /*dt*/) const {
        camera.LookAt(sequence.introCamera_, { position.x, position.y + sequence.lookHeight_ + sequence.lookLift_, position.z });
    }
    // 合図「カメラが後ろへ戻る」を受けた時の向き（既定＝すぐ進む方向へ向き直る）
    virtual void OnReturnCue(StageIntroSequence& sequence, Player& player) const { player.SetFaceOverride(true, sequence.forwardYaw_); }
};

// 演技の段階の状態クラスの入れ物（StageIntroSequence の中の物に触れるよう、StageIntroSequence の中に置く）
struct StageIntroSequence::Acts {
    // State の番号と同じ並びの表
    static const ActState& Of(State state);

    // 演技していない
    class None final : public ActState {
    public:
        bool IsActive() const override { return false; }
        bool HidesHud() const override { return false; }
    };

    // 準備ができて、次の合図を待っている（進む向きを向いて立っている）
    class Waiting final : public ActState {};

    // こっちを見て、手を振る
    class Wave final : public ActState {
    public:
        void Enter(StageIntroSequence& sequence, Player& player, const Context& context) const override {
            player.SetFaceOverride(true, sequence.YawToCamera(player.GetPosition()));
            if ( context.avatar ) { context.avatar->PlayIntroClip("IntroWave"); }
        }
    };

    // 進む方向（ロール）を向いて、押す。ロールは少しずつ前へ
    class Push final : public ActState {
    public:
        void Enter(StageIntroSequence& sequence, Player& player, const Context& context) const override {
            player.SetFaceOverride(true, sequence.forwardYaw_);
            if ( context.avatar ) { context.avatar->PlayIntroClip("IntroPush"); }
            Rolls::Of(sequence.rollState_).OnPushed(sequence);
        }
    };

    // その場でジャンプ（高さはクリップが出す。物理のジャンプは押さない）。跳んだらカメラを向き、着地で土ぼこり
    class Action final : public ActState {
    public:
        void Enter(StageIntroSequence& sequence, Player& player, const Context& context) const override {
            player.SetFaceOverride(true, sequence.forwardYaw_);
            if ( context.avatar ) { context.avatar->PlayIntroClip("IntroJump"); }
            sequence.actionTime_ = 0.0f;
            sequence.tookOff_ = sequence.landed_ = sequence.faceForwardOnLand_ = false;
        }
        void UpdateInput(StageIntroSequence& sequence, Player& player, const Context& context, float dt) const override {
            sequence.TrackJump(player, context, dt);
        }
        // まだ空中なら、着地してから進む方向へ向き直る
        void OnReturnCue(StageIntroSequence& sequence, Player& player) const override {
            if ( !sequence.landed_ ) { sequence.faceForwardOnLand_ = true; return; }
            ActState::OnReturnCue(sequence, player);
        }
    };

    // カメラが後ろへ戻る（HUD を出す）。ジャンプの途中で始まった時は、着地まで続ける
    class Returning final : public ActState {
    public:
        bool HidesHud() const override { return false; }
        void Enter(StageIntroSequence& sequence, Player& /*player*/, const Context& /*context*/) const override { sequence.returnTime_ = 0.0f; }
        void UpdateInput(StageIntroSequence& sequence, Player& player, const Context& context, float dt) const override {
            if ( !sequence.landed_ ) { sequence.TrackJump(player, context, dt); }
        }
        // イントロカメラ → 通常カメラへ（始めと終わりがゆっくり）。向きはどちらもプレイヤーを見るので、見る点を補間する
        void AimCamera(StageIntroSequence& sequence, Camera& camera, const Context& context, const Vector3& position, float dt) const override {
            sequence.returnTime_ += dt;
            const float e = Easing::EaseInOutCubic(sequence.returnTime_ / kReturnTime);
            const Vector3 follow = context.camCtrl->FollowTargetPosition(position);
            const Vector3& from = sequence.introCamera_;
            const Vector3 at { from.x + ( follow.x - from.x ) * e, from.y + ( follow.y - from.y ) * e, from.z + ( follow.z - from.z ) * e };
            camera.LookAt(at, { position.x, position.y + sequence.lookHeight_ + sequence.lookLift_ * ( 1.0f - e ) + ( kFollowLookHeight - sequence.lookHeight_ ) * e, position.z });
        }
    };
};

const StageIntroSequence::ActState& StageIntroSequence::Acts::Of(State state){
    static const None none {};
    static const Waiting waiting {};
    static const Wave wave {};
    static const Push push {};
    static const Action action {};
    static const Returning returning {};
    static const ActState* const kTable[] = { &none, &waiting, &wave, &push, &action, &returning };
    return *kTable[static_cast<size_t>( state )];
}

bool StageIntroSequence::IsActive() const{ return Acts::Of(state_).IsActive(); }
bool StageIntroSequence::IsHudHidden() const{ return Acts::Of(state_).HidesHud(); }

// 次の段階へ（入った時の向き・クリップは、状態クラスが決める）
void StageIntroSequence::ChangeState(State next, Player& player, const Context& context){
    state_ = next;
    Acts::Of(next).Enter(*this, player, context);
}

// 遷移の台本から来る合図。合図の名前ごとの中身は、下の Cue〜 の関数（名前との対応は表）
void StageIntroSequence::OnCue(const std::string& cue, const Context& context){
    if ( !context.player ) { return; }
    Player& player = *context.player;
    const Vector3 position = player.GetPosition();
    if ( cue == "introStart" ) { CueStart(player, position, context); return; } // 準備の合図だけは、演技の最中でなくても受ける
    if ( !IsActive() ) { return; } // 準備の合図が来ていない（エディタから始めた時など）
    if ( !cameraPlaced_ && context.camCtrl ) { PlaceCamera(position, *context.camCtrl); }

    using CueHandler = void ( StageIntroSequence::* )( Player&, const Vector3&, const Context& );
    static const struct { const char* cue; CueHandler handler; } kCues[] = {
        { "introDrop",    &StageIntroSequence::CueDrop },
        { "introTurn",    &StageIntroSequence::CueTurn },
        { "introPush",    &StageIntroSequence::CuePush },
        { "introRollGo",  &StageIntroSequence::CueRollGo },
        { "introAction",  &StageIntroSequence::CueAction },
        { "introSparkle", &StageIntroSequence::CueSparkle },
        { "introReturn",  &StageIntroSequence::CueReturn },
        { "introEnd",     &StageIntroSequence::CueEnd },
    };
    for ( const auto& row : kCues ) {
        if ( cue == row.cue ) { ( this->*row.handler )( player, position, context ); return; }
    }
}

// introStart：カードの裏でステージを読み終えた。ロールを置いて、進む向きを向いて待つ
void StageIntroSequence::CueStart(Player& player, const Vector3& position, const Context& context){
    // 進む向き＝スタートのレールの向き。ロールはスタートから進む方向へ少し先、道の上に置く
    Vector3 start = position;
    if ( context.railField ) {
        const auto& rails = context.railField->GetRails();
        const int rail = context.railField->GetStartRail();
        if ( rail >= 0 && rail < ( int ) rails.size() ) {
            const float distance = context.railField->GetStartDistance();
            const Vector3 tangent = rails[rail].GetTangentByDistance(distance);
            if ( std::abs(tangent.x) > 1e-4f || std::abs(tangent.z) > 1e-4f ) { forwardYaw_ = std::atan2(tangent.x, tangent.z); }
            start = rails[rail].GetPositionByDistance(distance);
        }
    }
    rollOrigin_ = start;
    rollDistance_ = rollSpeed_ = rollAngle_ = 0.0f;
    rollState_ = Roll::Placed;
    unrolling_ = true; // ロールより先の道は、まだ布が敷かれていない
    // イントロカメラの位置は、プレイヤーがスタートに立った後（次の更新）で決める
    //   （プレイ開始の直後だと、プレイヤーの位置がまだスタートに置かれていないことがある）
    cameraPlaced_ = false;
    headBase_ = -1.0f;
    lookLift_ = 0.0f;
    tookOff_ = landed_ = faceForwardOnLand_ = false;
    player.SetFaceOverride(true, forwardYaw_); // 最初は進む向きを向いて立っている
    player.SetTurnRate(kIntroTurnRate);        // 向きを変えるのが一瞬に見えないよう、ゆっくり
    player.SetAutoPilot(true);                 // キーの代わり：何も押さない
    if ( context.avatar ) { context.avatar->ClearIntroClip(); }
    // ワールドマップから飛び込んで来た：カードが分かれるまで、画面の上の外で待つ
    drop_ = dropRequested_ ? Drop::Waiting : Drop::None;
    dropRequested_ = false;
    if ( context.avatar ) { context.avatar->SetDropPose(Drops::Of(drop_).StartHeight(), 1.0f); }
    ChangeState(State::Waiting, player, context);
}

// introDrop：ワールドマップから飛び込んで来た時だけ、上から落ちてくる
void StageIntroSequence::CueDrop(Player& /*player*/, const Vector3& /*position*/, const Context& /*context*/){
    Drops::Of(drop_).OnDropCue(*this);
}

// introTurn：こっちを見て、手を振る
void StageIntroSequence::CueTurn(Player& player, const Vector3& /*position*/, const Context& context){
    ChangeState(State::Wave, player, context);
}

// introPush：進む方向（ロール）を向いて、押す
void StageIntroSequence::CuePush(Player& player, const Vector3& /*position*/, const Context& context){
    ChangeState(State::Push, player, context);
}

// introRollGo：ロールが転がり出す。足元に土ぼこり
void StageIntroSequence::CueRollGo(Player& /*player*/, const Vector3& /*position*/, const Context& context){
    rollState_ = Roll::Rolling;
    const Vector3 forward { std::sin(forwardYaw_), 0.0f, std::cos(forwardYaw_) };
    const float ahead = rollAhead_ + rollDistance_;
    SpawnDust(context.eggSystem, { rollOrigin_.x + forward.x * ahead, rollOrigin_.y, rollOrigin_.z + forward.z * ahead }, 0.5f);
}

// introAction：その場でジャンプ
void StageIntroSequence::CueAction(Player& player, const Vector3& /*position*/, const Context& context){
    ChangeState(State::Action, player, context);
}

// introSparkle：頭の上にキラキラ（黄色とピンク）
void StageIntroSequence::CueSparkle(Player& /*player*/, const Vector3& position, const Context& context){
    Vector3 head { position.x, position.y + 1.4f, position.z };
    if ( context.avatar ) { context.avatar->GetJointWorldPosition("Head", head); }
    head.y += 0.4f;
    ParticleManager::GetInstance()->Emit(kSparkleYellow, head, 2);
    ParticleManager::GetInstance()->Emit(kSparklePink, head, 1);
}

// introReturn：カメラが後ろへ戻る（向きは、今の段階が決める：まだ空中なら、着地してから向き直る）
void StageIntroSequence::CueReturn(Player& player, const Vector3& /*position*/, const Context& context){
    Acts::Of(state_).OnReturnCue(*this, player);
    ChangeState(State::Returning, player, context);
}

// introEnd：操作開始
void StageIntroSequence::CueEnd(Player& player, const Vector3& /*position*/, const Context& context){
    player.SetFaceOverride(false, 0.0f);
    player.SetAutoPilot(false);
    player.SetTurnRate(PlayerFacing::kDefaultTurnRate);
    if ( context.avatar ) {
        context.avatar->ClearIntroClip();
        context.avatar->SetDropPose(0.0f, 1.0f);
    }
    drop_ = Drop::None;
    ChangeState(State::None, player, context);
}

void StageIntroSequence::UpdateInput(const Context& context, float dt){
    if ( !IsActive() || !context.player ) { return; }
    Player& player = *context.player;
    player.SetAutoPilot(true, {}); // 動かない・跳ばない（ジャンプの高さはクリップが出す）
    UpdateDrop(context, dt);
    Acts::Of(state_).UpdateInput(*this, player, context, dt); // 段階ごとの動き（ジャンプの進みなど）
}

// ジャンプの途中：跳び上がったらカメラを向き、着地で土ぼこり（カメラが戻り始めた後も続く）
void StageIntroSequence::TrackJump(Player& player, const Context& context, float dt){
    actionTime_ += dt;
    const Vector3 position = player.GetPosition();
    if ( !tookOff_ && actionTime_ >= kTakeOffTime ) {
        tookOff_ = true;
        player.SetFaceOverride(true, YawToCamera(position));
        SpawnDust(context.eggSystem, position, 0.35f);
    }
    if ( !landed_ && actionTime_ >= kLandTime ) {
        landed_ = true;
        SpawnDust(context.eggSystem, position, 0.4f);
        if ( faceForwardOnLand_ ) { player.SetFaceOverride(true, forwardYaw_); }
    }
}

// 上から落ちてくる：落ち方・つぶれ方は、落下の状態クラスが決める
void StageIntroSequence::UpdateDrop(const Context& context, float dt){
    if ( !context.avatar ) { return; }
    Drops::Of(drop_).Update(*this, context, dt);
}

// イントロカメラ：プレイヤーから通常カメラの方へ寄った、少し低い所（少し見上げる）。進む向きへ少しずらす
void StageIntroSequence::PlaceCamera(const Vector3& position, PlayCameraController& camCtrl){
    const Vector3 follow = camCtrl.FollowTargetPosition(position);
    Vector3 toCamera { follow.x - position.x, 0.0f, follow.z - position.z };
    const float length = std::sqrt(toCamera.x * toCamera.x + toCamera.z * toCamera.z);
    toCamera = ( length > 1e-4f ) ? Vector3 { toCamera.x / length, 0.0f, toCamera.z / length } : Vector3 { 0.0f, 0.0f, -1.0f };
    const Vector3 forward { std::sin(forwardYaw_), 0.0f, std::cos(forwardYaw_) };
    introCamera_ = { position.x + toCamera.x * distance_ + forward.x * side_,
                     position.y + height_,
                     position.z + toCamera.z * distance_ + forward.z * side_ };
    cameraPlaced_ = true;
}

void StageIntroSequence::UpdateCamera(Camera* camera, const Context& context, float dt){
    if ( !IsActive() || !camera || !context.player || !context.camCtrl ) { return; }
    const Vector3 position = context.player->GetPosition();
    if ( !cameraPlaced_ ) { PlaceCamera(position, *context.camCtrl); }
    // ジャンプ（クリップが体を持ち上げる）で頭が画面の上へ切れないよう、上がった頭の分だけ見る所も上げる
    Vector3 head {};
    if ( context.avatar && context.avatar->GetJointWorldPosition("Head", head) ) {
        const float headHeight = head.y - position.y;
        if ( headBase_ < 0.0f ) { headBase_ = headHeight; }
        const float target = ( std::max )( headHeight - headBase_, 0.0f ) * 0.7f;
        lookLift_ += ( target - lookLift_ ) * ( std::min )( 10.0f * dt, 1.0f );
    }
    Acts::Of(state_).AimCamera(*this, *camera, context, position, dt); // どこから見るかは、段階ごとの状態が決める
}

// ロール：道の幅の方向を軸に、進む方向へ転がる（回った角度 ＝ 転がった距離 ÷ 半径）。転がるほど少し細くなる
void StageIntroSequence::UpdateRoll(float dt){
    const RollState& rollState = Rolls::Of(rollState_);
    if ( !rollState.IsVisible() || !roll_ ) { return; }
    rollSpeed_ = rollState.Speed(*this, dt); // 押されている・転がっている・止まっている、は状態が決める
    const float radius = rollRadius_ * ( 1.0f - kRollShrink * ( std::min )( rollDistance_ / kRollShrinkDistance, 1.0f ) );
    const float step = rollSpeed_ * dt;
    rollDistance_ += step;
    rollAngle_ += step / ( std::max )( radius, 0.01f );
    if ( rollDistance_ > kRollHideDistance ) { rollState_ = Roll::Hidden; unrolling_ = false; return; } // 画面の外：残りは全部敷く
    const Vector3 forward { std::sin(forwardYaw_), 0.0f, std::cos(forwardYaw_) };
    const float ahead = rollAhead_ + rollDistance_;
    roll_->SetTranslation({ rollOrigin_.x + forward.x * ahead, rollOrigin_.y + radius, rollOrigin_.z + forward.z * ahead });
    // モデルは X が筒の軸（長さ1・半径1）。進む向きへ回すと、X が道の幅の方向になる
    roll_->SetRotation({ rollAngle_, forwardYaw_, 0.0f });
    roll_->SetScale({ 1.0f, radius, radius });
    roll_->Update();
}

void StageIntroSequence::Draw(){
    if ( Rolls::Of(rollState_).IsVisible() && roll_ ) { roll_->Draw(); }
}

// 土ぼこり：足元から横と上へ、ふわっと広がる小さな粒（明るい背景でも見えるよう、加算ではない粒で）
void StageIntroSequence::SpawnDust(EggSystem* eggSystem, const Vector3& at, float spread) const{
    if ( !eggSystem ) { return; }
    const Vector4 color { 0.86f, 0.76f, 0.6f, 1.0f };
    for ( int i = 0; i < 3; ++i ) {
        const float angle = forwardYaw_ + Math::kHalfPi + ( float ) i * 2.0944f;
        const Vector3 velocity { std::sin(angle) * spread * 3.0f, 0.9f + 0.25f * ( float ) i, std::cos(angle) * spread * 3.0f };
        eggSystem->SpawnPuff({ at.x, at.y + 0.1f, at.z }, velocity, color, 0.16f + 0.03f * ( float ) i, 0.45f);
    }
}
