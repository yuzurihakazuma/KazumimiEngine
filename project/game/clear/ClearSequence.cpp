#include "game/clear/ClearSequence.h"

#include "engine/audio/AudioManager.h"
#include "engine/camera/Camera.h"
#include "engine/math/VectorMath.h"
#include "engine/rail/SplineRail.h"
#include "engine/utils/Easing.h"
#include "game/clear/ClearCamera.h"
#include "game/clear/GoalProps.h"
#include "game/clear/GoalRing.h"
#include "game/egg/EggSystem.h"
#include "game/player/Player.h"
#include "game/player/PlayerAvatar.h"
#include "game/rail/RailField.h"
#include "game/transition/KamishibaiTransition.h"
#include "game/effect/PaperFx.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace {
    using Sequence = ClearSequence::Sequence;

    constexpr const char* kJumpClip = "GoalJump";   // 大ジャンプ（tools/stage_intro/make_intro_clips.py）
    constexpr const char* kPoseClip = "GoalPose";   // 着地からの決めポーズ（最後の形で止まる）
    constexpr const char* kDanceClip = "GoalDance"; // 喜びの踊り（くり返し）
    constexpr float kArriveDistance = 0.03f;        // 着地する所にこれより近ければ、着いた扱い(m)
    constexpr float kRingVolume = 0.9f;
    constexpr float kConfettiSpeed = 4.2f;          // 輪をくぐった瞬間の紙吹雪が舞い上がる速さ(m/秒)
    constexpr float kConfettiSize = 0.11f;          // 紙吹雪 1 枚の大きさ(m)
    constexpr float kRainSpeed = 0.5f;              // 降らせる紙吹雪の、最初の上向きの速さ(m/秒)
    constexpr float kRainLife = 2.6f;               // 降らせる紙吹雪の、消えるまでの長さ（ふつうの紙吹雪に対する倍）
    constexpr float kDustSpread = 1.6f;             // 着地の土ぼこりが広がる速さ(m/秒)
    constexpr int   kDustCount = 6;
    const Vector4 kDustColor { 0.86f, 0.76f, 0.6f, 1.0f };

    // 恐竜の向き：position から target の方（水平）
    float YawToward(const Vector3& position, const Vector3& target){
        return std::atan2(target.x - position.x, target.z - position.z);
    }

    // 着地：土ぼこり・カメラの方を向く・決めポーズ
    void Land(ClearContext& context){
        if ( context.sinceLanding >= 0.0f ) { return; } // もう着地した
        context.sinceLanding = 0.0f;
        if ( !context.player ) { return; }
        const Vector3 position = context.player->GetPosition();
        if ( context.eggSystem ) {
            for ( int i = 0; i < kDustCount; ++i ) {
                const float angle = 2.0f * Easing::kPi * static_cast<float>( i ) / static_cast<float>( kDustCount );
                const Vector3 velocity { std::cos(angle) * kDustSpread, 0.9f, std::sin(angle) * kDustSpread };
                context.eggSystem->SpawnPuff({ position.x, position.y + 0.1f, position.z }, velocity, kDustColor, 0.2f, 0.5f);
            }
        }
        if ( context.clearCamera ) {
            context.player->SetFaceOverride(true, YawToward(position, context.clearCamera->EndPosition()));
        }
        if ( context.avatar ) { context.avatar->PlayIntroClip(kPoseClip); }
    }

    // --- 輪をくぐる：殻が割れて開く。恐竜は高く跳び、カメラが横から正面へ回り込む ---
    class RingStep : public Sequence::TimedStep {
    public:
        explicit RingStep(float duration) : TimedStep(duration) {}

        void Enter(ClearContext& context) override{
            const ClearSettings& settings = *context.settings;
            AudioManager::GetInstance()->PlayWave(settings.sounds.ring, false, kRingVolume);
            if ( context.ring ) {
                context.ring->Open();
                // 割れ目から殻のかけら、輪の真ん中から紙吹雪
                const Vector3 ringCenter = context.ring->Position() + Vector3 { 0.0f, settings.ring.hinge.y, 0.0f };
                if ( context.eggSystem ) {
                    const Vector3 side { context.ring->Forward().z, 0.0f, -context.ring->Forward().x };
                    for ( const float direction : { -1.0f, 1.0f } ) {
                        context.eggSystem->SpawnBreakFx(ringCenter + side * ( settings.ring.hinge.x * direction ), context.toCamera * -1.0f);
                    }
                }
                if ( context.paperFx ) {
                    context.paperFx->Confetti(ringCenter, settings.confetti.burst, kConfettiSpeed, kConfettiSize);
                }
            }
            if ( context.avatar ) { context.avatar->PlayIntroClip(kJumpClip); }
            if ( context.player ) { // 跳んでいる間は、進む向きのまま
                context.player->SetFaceOverride(true, std::atan2(context.toCamera.x, context.toCamera.z));
            }
        }
        void Exit(ClearContext& context) override{
            Land(context); // 短縮版などで、着地の時刻より先に終わった時
            if ( context.clearCamera ) { context.clearCamera->ApplyEnd(context.camera); }
        }

    protected:
        void OnUpdate(ClearContext& context, float /*deltaTime*/) override{
            if ( context.clearCamera ) { context.clearCamera->Apply(context.camera, Progress()); }
            if ( Elapsed() >= context.settings->jump.land ) { Land(context); }
        }
    };

    // --- 「ゴール！」：切り文字・星・紙吹雪。恐竜は決めポーズから喜びの踊り ---
    class GoalStep : public Sequence::TimedStep {
    public:
        explicit GoalStep(float duration) : TimedStep(duration) {}

        void Enter(ClearContext& context) override{
            context.propsTime = 0.0f;
            if ( context.props ) { context.props->Begin(context.landPoint, context.toCamera); }
            rainTimer_ = 0.0f;
        }

    protected:
        void OnUpdate(ClearContext& context, float deltaTime) override{
            if ( context.clearCamera ) { context.clearCamera->ApplyEnd(context.camera); }
            // 紙吹雪：舞台の上から、少しずつ降らせる
            const ClearConfetti& confetti = context.settings->confetti;
            rainTimer_ -= deltaTime;
            if ( rainTimer_ <= 0.0f && context.paperFx ) {
                rainTimer_ = confetti.rainInterval;
                const Vector3 right = VectorMath::Cross({ 0.0f, 1.0f, 0.0f }, context.toCamera * -1.0f);
                std::uniform_real_distribution<float> across(-confetti.spread, confetti.spread);
                std::uniform_real_distribution<float> depth(-1.5f, 0.6f);
                std::uniform_real_distribution<float> height(confetti.height * 0.55f, confetti.height);
                const Vector3 at = context.landPoint + right * across(random_) + context.toCamera * depth(random_)
                                 + Vector3 { 0.0f, height(random_), 0.0f };
                context.paperFx->Confetti(at, confetti.rain, kRainSpeed, kConfettiSize, kRainLife);
            }
        }

    private:
        float rainTimer_ = 0.0f;
        std::mt19937 random_ { 11u };
    };

    // --- 幕：読み込み中のカード（段ボールの台紙に、恐竜・卵・敵の切り絵）で画面を覆って、次のシーンへ ---
    class CurtainStep : public Sequence::IStep {
    public:
        void Enter(ClearContext& context) override{
            context.leaving = true;
            // 画面遷移の仕組みと共通（カードが覆っている間にシーンが切り替わる）
            KamishibaiTransition::ChangeSceneWithCard(context.nextScene);
        }
        // シーンが切り替わるまで、このまま（リザルトを出したまま、カードに覆われる）
        bool Update(ClearContext& /*context*/, float /*deltaTime*/) override{ return false; }
    };
}

void ClearSequence::Build(Sequence& sequence, const ClearContext& context){
    const ClearTiming& timing = context.timing;
    sequence.Clear();
    sequence.Add<RingStep>("輪をくぐる", timing.ring);
    sequence.Add<GoalStep>("「ゴール！」", timing.goal);
    sequence.AddCall("リザルトを出す", [](ClearContext& c){
        c.resultOpen = true;
        if ( c.result ) { c.result->Open(c.resultData); }
    });
    // 景色が沈んでマットが出る間は、決定ボタンを受け付けない（「ゴール！」で押していたボタンで飛ばさない）
    sequence.AddUntil("リザルトへ", [](ClearContext& c, float elapsed, float deltaTime){
        if ( c.result ) { c.result->Update(deltaTime, false); }
        return elapsed >= c.timing.toResult;
    });
    // リザルト：帯・3 行・花丸 → 決定ボタン → 所持数へ加算（中の進行は ResultScreen。決定ボタンで早送り）
    sequence.AddUntil("リザルト・加算", [](ClearContext& c, float /*elapsed*/, float deltaTime){
        if ( !c.result ) { return true; }
        c.result->Update(deltaTime, c.decide);
        return c.result->IsFinished();
    });
    sequence.AddTimed("幕の前", timing.leave);
    sequence.Add<CurtainStep>("幕");
}

// 恐竜：着地する所へ歩かせ（レールの向きから、押すキーの向き＝世界の ±X / ±Z を決める）、決めポーズの後は踊らせる
void ClearSequence::UpdateActor(ClearContext& context, float deltaTime){
    if ( !context.player ) { return; }
    // 着地してからの時間：決めポーズが終わったら、喜びの踊りへ
    if ( context.sinceLanding >= 0.0f ) {
        context.sinceLanding += deltaTime;
        if ( !context.dancing && context.sinceLanding >= context.settings->jump.pose ) {
            context.dancing = true;
            if ( context.avatar ) { context.avatar->PlayIntroClip(kDanceClip, true); }
        }
    }
    Player& player = *context.player;
    PlayerInput::AutoPilot pilot; // 何も押さない
    const auto* rails = context.railField ? &context.railField->GetRails() : nullptr;
    const int rail = player.GetCurrentRail();
    if ( rails && rail == context.landRail && rail >= 0 && rail < static_cast<int>( rails->size() ) && deltaTime > 0.0f ) {
        const float remaining = context.landDistance - player.GetRailDistance();
        if ( std::abs(remaining) > kArriveDistance ) {
            // 着地までの時間で、ちょうど着く速さ（歩く速さより速くはしない。行き過ぎないよう、残りの距離でも抑える）
            const ClearJump& jump = context.settings->jump;
            const float timeLeft = ( std::max )( jump.land - jump.takeOff, 0.2f );
            const float walkSpeed = ( std::max )( player.GetMoveSpeed(), 0.1f );
            const float wanted = ( std::min )( context.stage->landAhead / timeLeft, walkSpeed );
            const float speed = ( std::min )( wanted, std::abs(remaining) / deltaTime );
            const float amount = std::clamp(speed / walkSpeed, 0.0f, 1.0f);
            // レールの進む向き（距離が増える向き）が、世界のどちら向きか
            const SplineRail& railData = ( *rails )[rail];
            const Vector3 tangent = railData.GetTangentByDistance(player.GetRailDistance());
            const bool horizontal = ( railData.type == SplineRail::RailType::Horizontal );
            const float axis = horizontal ? tangent.x : tangent.z;
            const float alongRail = ( remaining > 0.0f ) ? 1.0f : -1.0f;
            const float world = alongRail * ( axis >= 0.0f ? 1.0f : -1.0f ) * amount;
            ( horizontal ? pilot.moveX : pilot.moveZ ) = world;
        }
    }
    player.SetAutoPilot(true, pilot);
}
