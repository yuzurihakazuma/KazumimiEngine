#include "game/gameover/GameOverFlow.h"

#include "engine/base/Input.h"
#include "engine/base/TimeManager.h"
#include "engine/base/WindowProc.h"
#include "engine/camera/Camera.h"
#include "engine/graphics/RenderTexture.h"
#include "engine/utils/Easing.h"
#include "game/camera/PlayCameraController.h"
#include "game/effect/PlayScreenEffects.h"
#include "game/player/Player.h"
#include "game/player/PlayerAvatar.h"
#include "game/rail/RailField.h"
#include "game/stage/StageBackdrop.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
    constexpr float kHeartHeight = 1.4f; // ♡の輪の高さ（恐竜の背の高さ）
    // とどめ：仰け反る間のスロー
    constexpr float kSlowScale = 0.2f;   // ゲームの時間の速さ
    constexpr float kSlowTime = 0.55f;   // スローの長さ(実時間の秒)
    constexpr float kSlowRecover = 0.2f; // そこから元の速さへ戻す長さ(実時間の秒)
    constexpr float kBendIn = 0.18f;     // 仰け反り切るまで(実時間の秒)
    constexpr float kLandTimeout = 4.0f; // 着地を待つ限り（どこにも着かない時の保険）

    // 確かめる用のキー：環境変数 CG2_DEBUG_KEYS を付けて起動した時だけ
    bool DebugKeysEnabled(){
        static const bool enabled = []{
            char* value = nullptr;
            size_t length = 0;
            const bool found = _dupenv_s(&value, &length, "CG2_DEBUG_KEYS") == 0 && value;
            free(value);
            return found;
        }();
        return enabled;
    }
}

GameOverFlow::GameOverFlow() = default;
GameOverFlow::~GameOverFlow() = default;

void GameOverFlow::Initialize(const Context& context, const StageBackdrop& backdrop){
    context_ = context;
    sequence_.Initialize();
    hearts_.Initialize(sequence_.HeartCount());
    resumeCamera_ = Camera::Create();
    defeatPoseApplied_ = false;
    if ( context_.player ) { context_.player->SetHoldOnFall(true); } // 落ちたらその場で止める（♡を見て、スタートへ戻すかゲームオーバーか決める）

    // 種明かしの箱：ステージの地面の範囲から作る
    Vector3 lo { 1e9f, 1e9f, 1e9f }, hi { -1e9f, -1e9f, -1e9f };
    for ( int i = 0; i < ( int ) backdrop.Items().size(); ++i ) {
        Vector3 boxMin, boxMax;
        if ( backdrop.Items()[i].removed || !backdrop.IsGround(i) || !backdrop.WorldBounds(i, boxMin, boxMax) ) { continue; }
        lo = { ( std::min )( lo.x, boxMin.x ), ( std::min )( lo.y, boxMin.y ), ( std::min )( lo.z, boxMin.z ) };
        hi = { ( std::max )( hi.x, boxMax.x ), ( std::max )( hi.y, boxMax.y ), ( std::max )( hi.z, boxMax.z ) };
    }
    float groundTop = 0.0f;
    if ( context_.railField ) {
        const auto& rails = context_.railField->GetRails();
        const int startRail = context_.railField->GetStartRail();
        if ( startRail >= 0 && startRail < ( int ) rails.size() && rails[startRail].nodes.size() >= 2 ) {
            groundTop = rails[startRail].GetPositionByDistance(context_.railField->GetStartDistance()).y;
        }
    }
    if ( lo.x < hi.x ) { sequence_.SetStageBounds(lo, hi, groundTop); }
}

void GameOverFlow::Finalize(){
    sequence_.Finalize();
    hearts_.Finalize();
    resumeCamera_.reset();
}

void GameOverFlow::OnPlayStart(){
    if ( dying_ && context_.player ) { context_.player->SetMovementLocked(false); }
    EndDying();
    hearts_.Refill();
    hearts_.HideNow();
}

void GameOverFlow::OnEditStart(){
    if ( sequence_.IsActive() ) { sequence_.Abort(); }
    if ( dying_ && context_.player ) { context_.player->SetMovementLocked(false); }
    EndDying();
    hearts_.Refill();
    hearts_.HideNow();
}

bool GameOverFlow::UpdateBeforeGame(bool introActive){
    // 演出の間：ゲームの時間は止め、演出だけを止まらない時間で進める
    if ( sequence_.IsActive() ) {
        sequence_.Update(Time::GetInstance()->GetUnscaledDeltaTime());
        if ( sequence_.ConsumeRestoreRequest() ) { RestoreCheckpoint(); } // 紙に貼り替えて見えていない間に
        return true;
    }
    if ( sequence_.ConsumeFinished() ) { hearts_.Show(); } // 再開した：満タンの♡を見せる
    if ( dying_ ) {                                        // とどめのスロー・着地待ち：ゲームは進める
        UpdateDying();
        return sequence_.IsActive();
    }
    if ( DebugKeysEnabled() && !introActive ) {
        Input* input = Input::GetInstance();
        if ( input->Triggerkey(DIK_F7) ) { BeginDying(); return false; }
        if ( input->Triggerkey(DIK_F6) ) { Damage(); if ( sequence_.IsActive() ) { return true; } }
    }
    return false;
}

void GameOverFlow::Damage(bool fellOut){
    if ( !hearts_.Damage(sequence_.DamagePerHit()) ) { return; }
    if ( fellOut ) { StartGameOver(); } // 穴に落ちた：着地は無いのですぐ
    else { BeginDying(); }
}

void GameOverFlow::BeginDying(){
    dying_ = true;
    dyingTime_ = 0.0f;
    if ( context_.player ) { context_.player->SetMovementLocked(true); } // 操作は止める（弾かれた勢いでは動く）
    Time::GetInstance()->SetTimeScale(kSlowScale);
}

void GameOverFlow::UpdateDying(){
    dyingTime_ += Time::GetInstance()->GetUnscaledDeltaTime();
    // スロー → 元の速さへ。ヒットストップが速さを戻しても、ここで毎フレーム決め直す
    const float recover = Easing::Clamp01(( dyingTime_ - kSlowTime ) / kSlowRecover);
    Time::GetInstance()->SetTimeScale(kSlowScale + ( 1.0f - kSlowScale ) * recover);
    // 仰け反り切って、地面に着いたらゲームオーバー（空中でやられたら、着くまで落ちる）
    Player* player = context_.player;
    const bool landed = !player || player->IsGrounded();
    if ( ( dyingTime_ >= kSlowTime && landed ) || dyingTime_ >= kLandTimeout ) {
        EndDying();
        StartGameOver();
    }
}

void GameOverFlow::EndDying(){
    if ( !dying_ ) { return; }
    dying_ = false;
    Time::GetInstance()->SetTimeScale(1.0f);
}

// 敵にぶつかった・穴に落ちた → ♡が減る。最後の♡が無くなったらゲームオーバー
bool GameOverFlow::UpdateAfterCombat(){
    Player* player = context_.player;
    if ( !player ) { return false; }
    if ( dying_ ) {
        player->ConsumeDamaged(); // とどめの後は、もう減らさない
        if ( player->IsFellOut() ) { EndDying(); StartGameOver(); } // 着く前に穴へ落ちた
        return sequence_.IsActive();
    }
    if ( player->ConsumeDamaged() ) {
        Damage();
        if ( sequence_.IsActive() ) { return true; }
    }
    if ( player->IsFellOut() ) {
        Damage(true);
        if ( sequence_.IsActive() ) { return true; }
        // まだ♡が残っている：スタートへ戻す（円を閉じて開く）。動くレールも基準へ
        player->Respawn();
        if ( context_.screenFx ) { context_.screenFx->StartMissIris(); }
        if ( context_.railField ) { context_.railField->ResetMotion(); }
    }
    return false;
}

void GameOverFlow::StartGameOver(){
    EndDying();
    ++gameOverCount_;
    hearts_.HideNow();
    sequence_.Start(context_.camera, gameOverCount_ >= 2); // 2回目からは短縮版
}

// 紙に貼り替えて見えていない間に、チェックポイント（今はスタート）の状態へ戻し、再開のカメラを求める
void GameOverFlow::RestoreCheckpoint(){
    Player* player = context_.player;
    RailField* railField = context_.railField;
    if ( !player || !railField ) { return; }
    player->Respawn();
    player->SetMovementLocked(false);
    railField->ResetMotion();
    // 位置はレールの更新で決まるので、キーを押していない扱いで1回だけ動かす（再開の絵に恐竜が写るように）
    player->SetAutoPilot(true);
    player->Update(railField->GetRails());
    player->SetAutoPilot(false);
    hearts_.Refill();
    hearts_.HideNow();
    // ステージもプレイ開始の時と同じに戻す（シーンが中身を知っている）
    if ( context_.resetStage ) { context_.resetStage(); }

    // 再開のカメラ：プレイ中のカメラと同じ追い方を、画面に出さないカメラで落ち着くまで回す
    if ( !context_.camCtrl ) { return; }
    context_.camCtrl->Reset();
    // スタート地点（Respawn 直後はまだ位置が入っていないこともあるので、レールから求める）
    Vector3 start = player->GetPosition();
    const auto& rails = railField->GetRails();
    const int startRail = railField->GetStartRail();
    if ( startRail >= 0 && startRail < ( int ) rails.size() && rails[startRail].nodes.size() >= 2 ) {
        start = rails[startRail].GetPositionByDistance(railField->GetStartDistance());
    }
    resumeCamera_->SetAspectRatio(( float ) WindowProc::GetInstance()->GetClientWidth() / ( float ) ( std::max )( 1, WindowProc::GetInstance()->GetClientHeight() ));
    resumeCamera_->SetFarClip(context_.farClip);
    for ( int i = 0; i < 120; ++i ) {
        context_.camCtrl->Update(resumeCamera_.get(), start, true, rails, false, 1.0f / 60.0f);
    }
    resumeCamera_->Update();
    sequence_.SetResumeCamera(resumeCamera_->GetWorldPosition(), resumeCamera_->GetRotation(), resumeCamera_->GetProjectionMatrix());
}

void GameOverFlow::UpdateVisuals(bool playing, const Vector3& playerPos){
    // とどめ〜ゲームオーバーの最初：やられの演技（スローで仰け反る → 着地 → 起き上がる → くらくら（横向きへ回る）→ しりもち → あおむけにぺたん）
    if ( context_.avatar ) {
        GameOverSequence::DefeatPoseData pose;
        const bool acting = sequence_.DefeatPose(pose);
        if ( dying_ ) {
            pose = GameOverSequence::DefeatPoseData {};
            pose.pitch = GameOverSequence::kBendPitch * Easing::EaseOutBack(Easing::Clamp01(dyingTime_ / kBendIn));
        }
        if ( dying_ || acting ) {
            context_.avatar->SetDropPose(pose.lift, pose.squash);
            // 向き：くらくらの間に、画面から見て横向き（近い方）へ回る。縦向き（奥・手前を向いている）でやられても、倒れる所が横から見える
            float yaw = context_.player ? context_.player->GetRotation().y : 0.0f;
            if ( acting && context_.camera ) {
                const Vector3 cam = context_.camera->GetWorldPosition();
                const float toCamera = std::atan2(cam.x - playerPos.x, cam.z - playerPos.z);
                auto wrap = [](float angle){
                    while ( angle > 3.14159265f ) { angle -= 6.2831853f; }
                    while ( angle < -3.14159265f ) { angle += 6.2831853f; }
                    return angle;
                };
                const float left = wrap(toCamera + 1.5707963f - yaw);
                const float right = wrap(toCamera - 1.5707963f - yaw);
                const float diff = std::fabs(left) < std::fabs(right) ? left : right;
                yaw += diff * pose.spin + pose.wobble;
            }
            context_.avatar->SetDefeatRotation(pose.pitch, pose.roll, true, yaw);
            defeatPoseApplied_ = true;
            // 星は頭のまわり（頭の骨が無ければ足元から背の高さの所）
            Vector3 head { playerPos.x, playerPos.y + kHeartHeight, playerPos.z };
            context_.avatar->GetJointWorldPosition("Head", head);
            sequence_.SetHeadPosition(head);
        } else if ( defeatPoseApplied_ ) {
            context_.avatar->SetDropPose(0.0f, 1.0f);
            context_.avatar->SetDefeatRotation(0.0f, 0.0f, false, 0.0f);
            defeatPoseApplied_ = false;
        }
    }
    // 恐竜のまわりの♡の輪
    if ( playing && context_.camera ) {
        hearts_.Update(Time::GetInstance()->GetUnscaledDeltaTime(), playerPos, kHeartHeight, *context_.camera);
    }
}

bool GameOverFlow::DrawUI(bool playing){
    if ( sequence_.IsActive() ) {
        sequence_.DrawUI();
        return true;
    }
    if ( playing ) { hearts_.Draw(); }
    return false;
}

void GameOverFlow::OnFrameComposed(ID3D12GraphicsCommandList* commandList, RenderTexture* finalTarget){
    if ( finalTarget ) { sequence_.OnFrameComposed(commandList, finalTarget->GetResource().Get()); }
}

void GameOverFlow::DrawInspector(bool playing){
#ifdef USE_IMGUI
    if ( !ImGui::CollapsingHeader("体力・ゲームオーバー") ) { return; }
    ImGui::Text("♡ %d / %d（1回で %d 減る）  ゲームオーバー %d 回", hearts_.Count(), hearts_.Max(), sequence_.DamagePerHit(), gameOverCount_);
    if ( !playing ) { ImGui::BeginDisabled(); }
    if ( ImGui::Button("ダメージ") && !sequence_.IsActive() && !dying_ ) { Damage(); }
    ImGui::SameLine();
    if ( ImGui::Button("ゲームオーバーを試す") && !sequence_.IsActive() && !dying_ ) { BeginDying(); }
    ImGui::SameLine();
    if ( ImGui::Button("満タン") ) { hearts_.Refill(); }
    if ( !playing ) { ImGui::EndDisabled(); }
    if ( ImGui::Button("回数を0に（フル版に戻す）") ) { gameOverCount_ = 0; }
    sequence_.DrawDebugUI();
#else
    ( void ) playing;
#endif
}
