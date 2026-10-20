#include "game/stage/StageIntroSequence.h"

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
#include <fstream>

namespace {
    constexpr float kReturnTime = 0.6f;       // カメラが後ろへ戻る時間(秒)
    constexpr float kFollowLookHeight = PlayCameraController::kLookHeight; // 通常カメラが見る高さ
    constexpr float kIntroTurnRate = 8.0f;    // 演技の間の向きを変える速さ（約0.35秒。通常は 18 で一瞬に見える）
    // IntroJump クリップの中の時刻（tools/stage_intro/make_intro_clips.py と同じ）
    constexpr float kTakeOffTime = 0.26f;     // 跳び上がる
    constexpr float kLandTime = 0.78f;        // 着地する
    // ロールの転がり方
    constexpr float kPushSpeed = 0.3f;        // 押している間(m/秒)
    constexpr float kRollAccel = 9.0f;        // 転がり出してからの加速（約1秒で 4.5m）
    constexpr float kRollMaxSpeed = 9.0f;     // その後の一定の速さ(m/秒)
    constexpr float kRollShrink = 0.3f;       // 転がるほど細くなる割合（布を敷いていく感じ）
    constexpr float kRollShrinkDistance = 12.0f;
    constexpr float kRollHideDistance = 40.0f; // ここまで転がったら消す（画面の外）
    constexpr const char* kRollModel = "introRoll";
    constexpr const char* kSparkleYellow = "IntroSparkleY";
    constexpr const char* kSparklePink = "IntroSparkleP";

    // カメラを position から target を見る向きへ（PlayCameraController と同じ規約）
    void LookAt(Camera* camera, const Vector3& position, const Vector3& target){
        camera->SetTranslation(position);
        const Vector3 look { target.x - position.x, target.y - position.y, target.z - position.z };
        const float horizontal = std::sqrt(look.x * look.x + look.z * look.z);
        if ( horizontal < 1e-4f && std::abs(look.y) < 1e-4f ) { return; }
        camera->SetRotation({ std::atan2(-look.y, horizontal), std::atan2(look.x, look.z), 0.0f });
    }
}

StageIntroSequence::StageIntroSequence() = default;
StageIntroSequence::~StageIntroSequence() = default;

void StageIntroSequence::Initialize(){
    roll_ = Obj3d::Create(kRollModel);
}

void StageIntroSequence::LoadSettings(const std::string& stage){
    std::ifstream file("resources/stage_intro/" + stage + ".json");
    if ( !file.is_open() ) { return; }
    nlohmann::json content;
    try { file >> content; } catch ( ... ) { return; }
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
    if ( context.avatar ) { context.avatar->ClearIntroClip(); }
    state_ = State::None;
    rollState_ = Roll::Hidden;
}

float StageIntroSequence::YawToCamera(const Vector3& position) const{
    return std::atan2(introCamera_.x - position.x, introCamera_.z - position.z);
}

void StageIntroSequence::OnCue(const std::string& cue, const Context& context){
    if ( !context.player ) { return; }
    Player& player = *context.player;
    const Vector3 position = player.GetPosition();
    if ( cue == "introStart" ) {
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
        state_ = State::Waiting;
        return;
    }
    if ( !IsActive() ) { return; } // 準備の合図が来ていない（エディタから始めた時など）
    if ( !cameraPlaced_ && context.camCtrl ) { PlaceCamera(position, *context.camCtrl); }

    if ( cue == "introTurn" ) {
        // こっちを見て、手を振る
        player.SetFaceOverride(true, YawToCamera(position));
        if ( context.avatar ) { context.avatar->PlayIntroClip("IntroWave"); }
        state_ = State::Wave;
    } else if ( cue == "introPush" ) {
        // 進む方向（ロール）を向いて、押す。ロールは少しずつ前へ
        player.SetFaceOverride(true, forwardYaw_);
        if ( context.avatar ) { context.avatar->PlayIntroClip("IntroPush"); }
        if ( rollState_ == Roll::Placed ) { rollState_ = Roll::Pushed; }
        state_ = State::Push;
    } else if ( cue == "introRollGo" ) {
        // ロールが転がり出す。足元に土ぼこり
        rollState_ = Roll::Rolling;
        const Vector3 forward { std::sin(forwardYaw_), 0.0f, std::cos(forwardYaw_) };
        const float ahead = rollAhead_ + rollDistance_;
        SpawnDust(context.eggSystem, { rollOrigin_.x + forward.x * ahead, rollOrigin_.y, rollOrigin_.z + forward.z * ahead }, 0.5f);
    } else if ( cue == "introAction" ) {
        // その場でジャンプ（高さはクリップが出す。物理のジャンプは押さない）。向きは跳んだらカメラへ
        player.SetFaceOverride(true, forwardYaw_);
        if ( context.avatar ) { context.avatar->PlayIntroClip("IntroJump"); }
        actionTime_ = 0.0f;
        tookOff_ = landed_ = faceForwardOnLand_ = false;
        state_ = State::Action;
    } else if ( cue == "introSparkle" ) {
        // 頭の上にキラキラ（黄色とピンク）
        Vector3 head { position.x, position.y + 1.4f, position.z };
        if ( context.avatar ) { context.avatar->GetJointWorldPosition("Head", head); }
        head.y += 0.4f;
        ParticleManager::GetInstance()->Emit(kSparkleYellow, head, 2);
        ParticleManager::GetInstance()->Emit(kSparklePink, head, 1);
    } else if ( cue == "introReturn" ) {
        // カメラが後ろへ戻る。まだ空中なら、着地してから進む方向へ向き直る
        if ( state_ == State::Action && !landed_ ) {
            faceForwardOnLand_ = true;
        } else {
            player.SetFaceOverride(true, forwardYaw_);
        }
        returnTime_ = 0.0f;
        state_ = State::Returning;
    } else if ( cue == "introEnd" ) {
        player.SetFaceOverride(false, 0.0f);
        player.SetAutoPilot(false);
        player.SetTurnRate(PlayerFacing::kDefaultTurnRate);
        if ( context.avatar ) { context.avatar->ClearIntroClip(); }
        state_ = State::None;
    }
}

void StageIntroSequence::UpdateInput(const Context& context, float dt){
    if ( !IsActive() || !context.player ) { return; }
    Player& player = *context.player;
    player.SetAutoPilot(true, {}); // 動かない・跳ばない（ジャンプの高さはクリップが出す）

    // ジャンプの途中：跳び上がったらカメラを向き、着地で土ぼこり（カメラが戻り始めた後も続く）
    if ( state_ == State::Action || ( state_ == State::Returning && !landed_ ) ) {
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
    if ( state_ != State::Returning ) {
        LookAt(camera, introCamera_, { position.x, position.y + lookHeight_ + lookLift_, position.z });
        return;
    }
    // イントロカメラ → 通常カメラへ（始めと終わりがゆっくり）。向きはどちらもプレイヤーを見るので、見る点を補間する
    returnTime_ += dt;
    const float e = Easing::EaseInOutCubic(returnTime_ / kReturnTime);
    const Vector3 follow = context.camCtrl->FollowTargetPosition(position);
    const Vector3 at { introCamera_.x + ( follow.x - introCamera_.x ) * e,
                       introCamera_.y + ( follow.y - introCamera_.y ) * e,
                       introCamera_.z + ( follow.z - introCamera_.z ) * e };
    LookAt(camera, at, { position.x, position.y + lookHeight_ + lookLift_ * ( 1.0f - e ) + ( kFollowLookHeight - lookHeight_ ) * e, position.z });
}

// ロール：道の幅の方向を軸に、進む方向へ転がる（回った角度 ＝ 転がった距離 ÷ 半径）。転がるほど少し細くなる
void StageIntroSequence::UpdateRoll(float dt){
    if ( rollState_ == Roll::Hidden || !roll_ ) { return; }
    if ( rollState_ == Roll::Pushed ) {
        rollSpeed_ = kPushSpeed;
    } else if ( rollState_ == Roll::Rolling ) {
        rollSpeed_ = ( std::min )( rollSpeed_ + kRollAccel * dt, kRollMaxSpeed );
    } else {
        rollSpeed_ = 0.0f;
    }
    const float radius = rollRadius_ * ( 1.0f - kRollShrink * ( std::min )( rollDistance_ / kRollShrinkDistance, 1.0f ) );
    const float step = rollSpeed_ * dt;
    rollDistance_ += step;
    rollAngle_ += step / ( std::max )( radius, 0.01f );
    if ( rollDistance_ > kRollHideDistance ) { rollState_ = Roll::Hidden; return; }
    const Vector3 forward { std::sin(forwardYaw_), 0.0f, std::cos(forwardYaw_) };
    const float ahead = rollAhead_ + rollDistance_;
    roll_->SetTranslation({ rollOrigin_.x + forward.x * ahead, rollOrigin_.y + radius, rollOrigin_.z + forward.z * ahead });
    // モデルは X が筒の軸（長さ1・半径1）。進む向きへ回すと、X が道の幅の方向になる
    roll_->SetRotation({ rollAngle_, forwardYaw_, 0.0f });
    roll_->SetScale({ 1.0f, radius, radius });
    roll_->Update();
}

void StageIntroSequence::Draw(){
    if ( rollState_ != Roll::Hidden && roll_ ) { roll_->Draw(); }
}

// 土ぼこり：足元から横と上へ、ふわっと広がる小さな粒（明るい背景でも見えるよう、加算ではない粒で）
void StageIntroSequence::SpawnDust(EggSystem* eggSystem, const Vector3& at, float spread) const{
    if ( !eggSystem ) { return; }
    const Vector4 color { 0.86f, 0.76f, 0.6f, 1.0f };
    for ( int i = 0; i < 3; ++i ) {
        const float angle = forwardYaw_ + 3.14159265f * 0.5f + ( float ) i * 2.0944f;
        const Vector3 velocity { std::sin(angle) * spread * 3.0f, 0.9f + 0.25f * ( float ) i, std::cos(angle) * spread * 3.0f };
        eggSystem->SpawnPuff({ at.x, at.y + 0.1f, at.z }, velocity, color, 0.16f + 0.03f * ( float ) i, 0.45f);
    }
}
