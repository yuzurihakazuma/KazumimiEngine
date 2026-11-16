#include "game/player/SwallowAbility.h"
#include "game/ui/UiScale.h"
#include "engine/math/MathUtil.h"

#include "game/player/Player.h"
#include "game/enemy/EnemyManager.h"
#include "game/egg/EggSystem.h"
#include "game/combat/HitFeel.h"
#include "engine/audio/AudioManager.h"
#include "engine/base/Input.h"
#include "engine/math/VectorMath.h"
#include "engine/3d/model/Model.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/3d/obj/Obj3d.h"
#include "engine/graphics/PipelineManager.h"
#include "engine/graphics/DebugDraw.h"
#include "engine/graphics/TextureManager.h"
#include "engine/2d/Sprite.h"
#include "engine/base/WindowProc.h"
#include "engine/math/Matrix4x4.h"
#include "engine/camera/Camera.h"

#include <cmath>
#include <algorithm>

using namespace VectorMath;

namespace {
    // ※ベロの見た目はプレイヤーモデルの TongueOut アニメ（予備動作→射出→保持→収納）。
    //   ここの秒数はそのアニメの進行に合わせてある（変える時はアニメ再生速度も合わせること）
    const float kRetractDuration   = 0.25f; // 捕獲時の戻り秒数（回収自体は即時。これは見た目の戻り）
    const float kMissRetractDuration = 0.25f; // 空振り時の引っ込み秒数
    // 舌の太さ(半幅)は SwallowAbility::kTongueHalfWidth（当たり判定も同じ太さ）
    // モデルのベロの色（player_palette.png の sRGB 247,172,183 をリニアにした値）。しなったベロの絵に使う
    const Vector4 kTongueColor     = { 0.930f, 0.412f, 0.473f, 1.0f };
    const float kAutoLayDelay      = 0.3f;  // 食べてから自動で産むまで(秒)。その間に F を押せば吐き出せる
    const Vector4 kCatchableColor  = { 1.0f, 0.55f, 0.7f, 0.9f };  // 「今なら捕まえられる」ハイライト（舌と同系色）
}

SwallowAbility::SwallowAbility() = default;
SwallowAbility::~SwallowAbility() = default;

void SwallowAbility::Reset(){
    state_       = State::Idle;
    limp_        = false;
    target_      = nullptr;
    phaseT_      = 0.0f;
    lastBelly_   = 0;
    autoLayTimer_ = -1.0f;
}

// E=舌を伸ばして捕まえる / 左Ctrl=産卵 / F=吐き出す。
//   敵を知るのは EnemyManager、卵を知るのは EggSystem。ここは入力・対象選択・舌の演出を行う。
void SwallowAbility::Update(Player& player, EnemyManager& enemies, EggSystem& eggs, HitFeel& hitFeel,
                            Camera* camera, float dt){
    Input* input = Input::GetInstance();
    Vector3 playerPos = player.GetPosition();

    // クールタイムの経過（成功した時だけ充填される）
    if ( cooldownTimer_ > 0.0f ) { cooldownTimer_ -= dt; }

    // 現在の水平向き（毎フレーム取り直す。ターゲット探索と空振り中の狙い方向の両方で使う）
    float yaw = player.GetRotation().y;
    Vector3 facing = { std::sin(yaw), 0.0f, std::cos(yaw) };

    // ベロ：付け根（鼻先の口）から、太さ kTongueHalfWidth・長さ swallowReach_ の棒が、いつも正面へまっすぐ伸びる。
    //   敵の方へは向けない（届かない敵・高さの合わない敵がいても、まっすぐ出て空振りする）。坂の上では坂に沿った正面。
    //   見た目のベロの骨も同じ向き・同じ長さにする（PlayerAvatar）。
    //   当たり判定はこの棒と敵の体（縦カプセル）が触れたか。捕まえた敵は、ベロの先にくっついたまま口へ戻る
    const Vector3 root = { playerPos.x + facing.x * kTongueRootForward, playerPos.y + kTongueRootHeight, playerPos.z + facing.z * kTongueRootForward };
    const Vector3 straight = Normalize(player.GetFacingAlongGround());
    aimDir_ = straight; // 伸ばしている間・戻している間も、今の正面（振り向けば一緒に向く）
    // 付け根から dir へ length までの棒に、敵の体が触れるか。触れる時は、付け根から触れる所までの長さ（-1＝触れない）
    auto touchAlong = [&](const Enemy& enemy, const Vector3& dir, float length) -> float {
        const float step = kTongueHalfWidth;
        for ( float s = 0.0f; s <= length + 1e-4f; s += step ) {
            const float along = ( std::min )( s, length );
            if ( enemy.HitsSphere(Add(root, Multiply(along, dir)), kTongueHalfWidth) ) { return along; }
        }
        return -1.0f;
    };
    // 付け根から dir へ length までに壁（ブロック）があるか（壁の向こうの敵は食べられない）
    auto blockedBefore = [&](const Vector3& dir, float length, Vector3* hitOut) -> bool {
        if ( !wallQuery_ || length <= 1e-3f ) { return false; }
        Vector3 hit;
        if ( !wallQuery_(root, Add(root, Multiply(length, dir)), hit) ) { return false; }
        if ( hitOut ) { *hitOut = hit; }
        return true;
    };

    // 正面へまっすぐ伸ばしたベロが触れる敵のうち、一番近い敵（Idle中の「食べられる印」に使う）
    auto findCatchable = [&]() -> Enemy* {
        Enemy* found = nullptr;
        float best = 1e9f;
        for ( auto& enemy : enemies.GetEnemies() ) {
            if ( !enemy->IsAlive() || enemy->IsSwallowing() ) continue;
            const float along = touchAlong(*enemy, straight, swallowReach_);
            if ( along < 0.0f || along >= best ) continue;
            if ( blockedBefore(straight, along, nullptr) ) continue; // 間に壁
            best = along; found = enemy.get();
        }
        return found;
        };
    // 敵を捕まえる：敵はベロの先にくっついたまま口へ引き込まれ、縮みながら溶ける（Enemy::TickSwallow）。
    //   敵のポインタは持ち続けない（EnemyManager が食べ終わった時に消すため）
    auto grab = [&](Enemy* caught){
        grabPos_ = caught->GetPosition();                                // ベロの先にくっつける（敵の真ん中）
        caught->SetSwallowMouthOffset(Subtract(root, playerPos));        // ベロの付け根（口）へ
        caught->StartSwallow();
        AudioManager::GetInstance()->PlayWave("resources/se/eggHit.wav", false, 0.5f); // 捕獲音（流用）
        hitFeel.Trigger(0.03f, 0.12f); // 捕まえた手応え
        retractDuration_ = kRetractDuration;
        retractCaught_   = true;
        limp_ = false;   // 捕まえたら、ぴんと張って引き戻す
    };
    // 今の長さ length のまっすぐなベロの棒に触れている敵のうち、付け根に一番近い敵を捕まえる（捕まえたら true）
    auto tryCatch = [&](float length) -> bool {
        Enemy* caught = nullptr;
        float bestAlong = 1e9f;
        for ( auto& enemy : enemies.GetEnemies() ) {
            if ( !enemy->IsAlive() || enemy->IsSwallowing() ) continue;
            const float along = touchAlong(*enemy, aimDir_, length);
            if ( along >= 0.0f && along < bestAlong ) { bestAlong = along; caught = enemy.get(); }
        }
        if ( !caught || blockedBefore(aimDir_, bestAlong, nullptr) ) { return false; } // 触れていない・間に壁
        grab(caught);
        return true;
    };
    // 波打つ時に床になる高さ（恐竜が立っている面。空中なら床なし）
    const float limpFloor = player.IsGrounded() ? playerPos.y + 0.05f : -1e9f;
    if ( debugVisible_ && state_ == State::Idle ) {
        // 当たり判定を表示：正面へ伸ばした時の棒（付け根 → 先）
        const Vector3 end = Add(root, Multiply(swallowReach_, straight));
        DebugDraw::GetInstance()->Line(root, end, kCatchableColor);
        DebugDraw::GetInstance()->Sphere(end, kTongueHalfWidth, kCatchableColor, 8);
    }

    // --- E：舌を伸ばす（Idle中のみ受け付ける。どこでも出せる＝対象がいなくても必ず伸びる）---
    //   届く範囲＆前方に敵がいれば、その位置へホーミングして捕まえる。
    //   いなければ正面へ届く距離ぶん伸びて、そのまま空振りで引っ込む。
    //   （以前は全方位の最近傍だったため、背後の敵まで吸えて違和感があった）
    if ( state_ != State::Idle ) { markVisible_ = false; } // ベロを出している間は、食べられる印を出さない（前の場所に残っていた）
    if ( state_ == State::Idle ) {
        // 「今Eを押したら捕まえられる敵」を毎フレーム可視化する（脈動するリング。分かりづらさ対策）。
        //   クールタイム中は実際には捕まえられないので出さない
        markVisible_ = false;
        if ( cooldownTimer_ <= 0.0f ) {
            Enemy* preview = findCatchable();
            if ( preview ) {
                highlightPulseT_ += dt;
                float pulse = 1.0f + 0.12f * std::sin(highlightPulseT_ * 8.0f);
                if ( debugVisible_ ) {
                    DebugDraw::GetInstance()->Sphere(preview->GetPosition(),
                        ( preview->GetRadius() + 0.18f ) * pulse, kCatchableColor, 16);
                }
                // 頭の上に、小さなベロの印がぴょこぴょこ揺れる
                if ( markSprite_ && camera ) {
                    const float bob = std::abs(std::sin(highlightPulseT_ * 6.0f));
                    const Vector3 head { preview->GetPosition().x, preview->GetPosition().y + preview->GetRadius() + 0.55f + 0.18f * bob, preview->GetPosition().z };
                    Vector2 ndc;
                    if ( MatrixMath::WorldToNdc(head, camera->GetViewProjectionMatrix(), ndc) ) {
                        const float width = ( float ) WindowProc::GetInstance()->GetClientWidth();
                        const float height = ( float ) WindowProc::GetInstance()->GetClientHeight();
                        const float size = 64.0f * height / UiScale::kBaseHeight * ( 1.0f + 0.08f * bob );
                        markSprite_->SetPosition({ ( ndc.x * 0.5f + 0.5f ) * width, ( 1.0f - ( ndc.y * 0.5f + 0.5f ) ) * height });
                        markSprite_->SetSize({ size, size });
                        markSprite_->SetRotation(0.25f * std::sin(highlightPulseT_ * 6.0f));
                        markSprite_->SetColor({ 1.0f, 0.55f, 0.66f, 1.0f });
                        markSprite_->Update();
                        markVisible_ = true;
                    }
                }
            } else {
                highlightPulseT_ = 0.0f; // 次に出た時パッと大きくならないようリセット
            }
        }

        if ( input->TriggerKey(DIK_E) && cooldownTimer_ <= 0.0f && !actionBlocked_ ) {
            target_ = nullptr;         // 狙いは付けない：正面へまっすぐ伸ばして、触れた敵を捕まえる
            state_  = State::Shooting;
            phaseT_ = 0.0f;
            shootLength_ = kTongueRestLength;
            limp_ = false;
            limpT_ = 0.0f;
            holdT_ = 0.0f;
            // 移動はロックしない：ヨッシー同様、走りながらでも舌を出せる
            // （口の位置は毎フレーム player.GetPosition() から取り直すので動いていても破綻しない）
            cooldownTimer_ = swallowCooldown_;
            AudioManager::GetInstance()->PlayWave("resources/se/eggThrow.wav", false, 0.35f); // 舌を伸ばす音（流用）
        }
    }
    // --- Shooting：狙い位置（敵 or 空振りの正面）まで舌が一直線に伸びる ---
    //   空振り時は「現在の口位置＋現在の向き」から毎フレーム再計算する。
    //   固定の到達点へ向かわせないことで、ジャンプで上下しても斜めにねじれず、
    //   途中で振り向けば舌もそのまま一緒に振り向く。
    else if ( state_ == State::Shooting ) {
        if ( target_ && !target_->IsAlive() ) {
            // 敵が別要因で消えた（踏まれた等）→ 以後は空振り扱いで続行
            target_ = nullptr;
        }
        // ベロは正面へまっすぐ速く伸びる。
        //   ・何にも当たらず伸びきった：少しだけ止まって、まっすぐ戻す（しならない）
        //   ・食べられない物（壁・ブロック）にぶつかった：そこで止まり、ぶつかった勢いで上下に波打ってから戻す
        phaseT_ += dt;
        shootLength_ = ( std::min )( shootLength_ + kTongueSpeed * dt, swallowReach_ );
        float length = shootLength_;
        Vector3 wallHit;
        bool blocked = false;
        if ( blockedBefore(aimDir_, length, &wallHit) ) {
            length = Length(Subtract(wallHit, root));
            blocked = true;
        }
        // 今のベロの棒に触れた敵がいたら、その瞬間に捕まえる（伸びきりを待たない＝当たったら即ベロを戻す）。
        //   ベロは体と一緒に動くので、出している途中でジャンプして敵の高さに重なった時も捕まえる
        const bool hitNow = tryCatch(length);
        if ( !hitNow && blocked && !limp_ ) { limp_ = true; limpT_ = 0.0f; } // ぶつかった：ここから波打つ
        if ( limp_ ) {
            limpT_ += dt;
            BuildLimpCurve(root, aimDir_, length, limpFloor);
        } else if ( !hitNow && length >= swallowReach_ - 1e-4f ) {
            holdT_ += dt; // 伸びきった所で少し止まる
        }
        UpdateTongueMesh(root, Add(root, Multiply(length, aimDir_)), camera);
        const bool giveUp = limp_ ? ( limpT_ >= kLimpHangTime ) : ( holdT_ >= kTongueHoldTime );
        if ( hitNow || giveUp ) {
            if ( !hitNow ) {
                // 空振り：引っ込める（伸びた長さから戻す。戻している間も、触れた敵は捕まえる）
                retractDuration_ = kMissRetractDuration;
                retractCaught_   = false;
                missLength_      = length;
            }
            state_  = State::Retracting;
            phaseT_ = 0.0f;
            target_ = nullptr;
        }
    }
    // --- Retracting：舌の先端が口元へ戻る ---
    //   捕獲時：Enemy::TickSwallow と同じ式（掴んだ位置→口）で、縮む敵にぴったり追従。
    //   空振り時：捕獲と違って追う対象が無いので、Shootingと同じく現在の向きへ毎フレーム
    //   再計算する（＝ジャンプでねじれない／振り向けば一緒に振り向く）。
    else {
        phaseT_ += dt;
        float t = ( std::min )( phaseT_ / retractDuration_, 1.0f );
        const float missLength = kTongueRestLength + ( missLength_ - kTongueRestLength ) * ( 1.0f - t );
        if ( !retractCaught_ ) {
            // 空振りで戻している間も、ベロの棒に敵が重なったら捕まえる（ジャンプして高さが合った時など）。
            //   壁にぶつかって波打っている時は、波打ったまま（だんだんおさまりながら）巻き取る
            if ( limp_ ) {
                limpT_ += dt;
                BuildLimpCurve(root, aimDir_, missLength, limpFloor);
            }
            if ( tryCatch(missLength) ) { phaseT_ = 0.0f; t = 0.0f; } // そこから捕まえた時の戻し方へ
        }
        Vector3 tip = retractCaught_ ? grabPos_ + ( root - grabPos_ ) * t : Add(root, Multiply(missLength, aimDir_));
        UpdateTongueMesh(root, tip, camera);

        if ( t >= 1.0f ) {
            state_ = State::Idle;
            limp_ = false;
            UpdateTongueMesh(root, root, camera); // しなったベロの絵を消す
        }
    }

    // --- 食べたら自動で産む：お腹が増えたら少し待ってから（その間に F で吐き出せる）。構え中は構え終わるまで待つ ---
    const int belly = eggs.BellyCount();
    if ( belly > lastBelly_ ) { autoLayTimer_ = kAutoLayDelay; }
    lastBelly_ = belly;
    bool autoLay = false;
    if ( autoLayTimer_ >= 0.0f ) {
        autoLayTimer_ -= dt;
        if ( autoLayTimer_ < 0.0f ) {
            if ( actionBlocked_ ) { autoLayTimer_ = 0.0f; } // 卵の構え中：終わったら産む
            else { autoLay = belly > 0; }
        }
    }

    // --- 左Ctrl（しゃがみ）・食べた後の自動：お腹の敵を1匹、後ろに卵として産む ---
    if ( ( input->TriggerKey(DIK_LCONTROL) || autoLay ) && !actionBlocked_ ) {
        const Vector3 behind = eggs.FollowSlot(eggs.HeldCount()); // 後ろの卵の列の最後（通って来た所。壁の中にならない）
        if ( eggs.LayEgg(behind) ) {
            eggs.SpawnLayFx(behind);        // 産まれた合図（白＆緑がぽわっと）
            hitFeel.Trigger(0.03f, 0.08f);
        }
    }

    // --- F：お腹の敵を吐き出す（プレイヤーの向いている方向へ発射。敵にぶつけて倒せる）---
    if ( input->TriggerKey(DIK_F) ) {
        // 坂道では坂に沿って前へ吐く（水平に吐くと、上り坂では吐いた直後に道へぶつかって消える）
        Vector3 spitFacing = player.GetFacingAlongGround();
        Vector3 spitMouth = { playerPos.x + spitFacing.x * 0.6f, playerPos.y + 0.5f + spitFacing.y * 0.6f, playerPos.z + spitFacing.z * 0.6f };
        if ( eggs.SpitOut(spitMouth, spitFacing) ) {
            hitFeel.Trigger(0.03f, 0.1f);   // 吐き出しの軽い手応え
            AudioManager::GetInstance()->PlayWave("resources/se/eggThrow.wav", false, 0.5f); // 発射音
        }
    }
}

// 食べられない物にぶつかったベロの形（付け根から先までの折れ線 curve_）を作る：上下に波打つ。
//   ぶつかった先から付け根へ向かって波が走り、だんだんおさまる。付け根（口）は動かさない。
//   床（恐竜が立っている面）より下へは行かない
void SwallowAbility::BuildLimpCurve(const Vector3& root, const Vector3& dir, float length, float floorY){
    const Vector3 side = Normalize(Cross({ 0.0f, 1.0f, 0.0f }, dir));   // 横（波打つ面に垂直）
    const Vector3 up = Normalize(Cross(dir, side));                     // 正面に垂直な上向き（波の向き）
    limpSide_ = side;
    const float kTwoPi = Math::kTwoPi;
    // 波の大きさ：ぶつかった瞬間にぱっと出て、だんだんおさまる。短く巻き取られるほど小さく
    const float amplitude = kWaveAmplitude * ( std::min )( limpT_ / 0.03f, 1.0f ) * std::exp(-kWaveDecay * limpT_)
                          * ( std::min )( length / 1.0f, 1.0f );
    curve_.clear();
    for ( int i = 0; i <= kLimpSegments; ++i ) {
        const float s = ( float ) i / ( float ) kLimpSegments;         // 0＝付け根 → 1＝先
        const float envelope = ( std::min )( s * 3.0f, 1.0f );          // 付け根は動かさない
        const float wave = std::sin(kTwoPi * ( kWaveCount * s + kWaveSpeed * limpT_ )); // 先から付け根へ走る
        Vector3 point = Add(Add(root, Multiply(length * s, dir)), Multiply(amplitude * envelope * wave, up));
        if ( point.y < floorY ) { point.y = floorY; }
        curve_.push_back(point);
    }
}

// ベロの長さ・先の位置を覚える（見た目のベロの骨が使う）。しなっている間は、しなったベロの絵（筒）を作る。
//   筒の太さ・色は、プレイヤーのモデルのベロ（平たい楕円の断面・真ん中が太い）と同じにしてある
void SwallowAbility::UpdateTongueMesh(const Vector3& mouth, const Vector3& tip, Camera* camera){
    tongueLength_ = Length(tip - mouth); // モデルの Tongue ボーンの伸び制御に使う実距離
    tongueTip_ = tip;                    // 見た目のベロの骨をここへ向ける
    if ( !limp_ && !tongueModel_ ) { return; }
    // 遅延生成：初めて必要になった時に動的メッシュを1回だけ確保する
    if ( !tongueModel_ ) {
        tongueModel_ = std::make_unique<Model>();
        tongueModel_->InitializeDynamic(ModelManager::GetInstance()->GetModelCommon(),
                                        ( kLimpSegments + 1 ) * kLimpRing, kLimpSegments * kLimpRing * 6, "resources/block/white1x1.png");
        if ( tongueModel_->GetMaterial() ) {
            tongueModel_->GetMaterial()->color = kTongueColor; // モデルのベロと同じ色
            tongueModel_->GetMaterial()->enableLighting = 1;
        }
        tongueObj_ = std::make_unique<Obj3d>();
        tongueObj_->Initialize(tongueModel_.get());
        tongueObj_->SetPipelineType(PipelineType::Object3D_CullNone); // 裏から見ても消えない
        tongueObj_->SetTranslation({ 0.0f, 0.0f, 0.0f }); // 頂点はワールド座標で焼くので平行移動はしない
    }

    std::vector<Model::VertexData> vertices;
    std::vector<uint32_t> indices;
    if ( limp_ && curve_.size() >= 2 ) {
        float length = 0.0f;
        for ( size_t i = 1; i < curve_.size(); ++i ) { length += Length(Subtract(curve_[i], curve_[i - 1])); }
        const float thick = 1.0f + 0.1f * std::clamp(( length - kTongueRestLength ) / 1.2f, 0.0f, 1.0f); // 骨のベロと同じ太り方
        const int count = ( int ) curve_.size();
        for ( int i = 0; i < count; ++i ) {
            const Vector3 ahead = curve_[( std::min )( i + 1, count - 1 )];
            const Vector3 behind = curve_[( std::max )( i - 1, 0 )];
            Vector3 tangent = Subtract(ahead, behind);
            tangent = Length(tangent) > 1e-5f ? Normalize(tangent) : Vector3 { 0.0f, 0.0f, 1.0f };
            const Vector3 up = Normalize(Cross(tangent, limpSide_));
            // 断面の大きさ：モデルのベロは細長い楕円体（真ん中が一番太い）。付け根は口の中から始まるので、少し太い所から
            const float u = 0.097f + 0.903f * ( float ) i / ( float ) ( count - 1 );
            const float profile = std::sqrt(( std::max )( 1.0f - ( 2.0f * u - 1.0f ) * ( 2.0f * u - 1.0f ), 0.0f ));
            const float halfWide = kTongueModelHalfWide * thick * profile;
            const float halfThin = kTongueModelHalfThin * thick * profile;
            for ( int r = 0; r < kLimpRing; ++r ) {
                const float a = Math::kTwoPi * ( float ) r / ( float ) kLimpRing;
                const float cosA = std::cos(a), sinA = std::sin(a);
                const Vector3 offset = Add(Multiply(cosA * halfWide, limpSide_), Multiply(sinA * halfThin, up));
                Vector3 normal = Add(Multiply(cosA * kTongueModelHalfThin, limpSide_), Multiply(sinA * kTongueModelHalfWide, up)); // 楕円の法線
                normal = Normalize(normal);
                Model::VertexData vertex {};
                const Vector3 position = Add(curve_[i], offset);
                vertex.position = { position.x, position.y, position.z, 1.0f };
                vertex.normal = normal;
                vertex.texcoord = { 0.5f, 0.5f };
                vertices.push_back(vertex);
            }
        }
        for ( int i = 0; i + 1 < count; ++i ) {
            for ( int r = 0; r < kLimpRing; ++r ) {
                const uint32_t a = ( uint32_t ) ( i * kLimpRing + r );
                const uint32_t b = ( uint32_t ) ( i * kLimpRing + ( r + 1 ) % kLimpRing );
                const uint32_t c = a + ( uint32_t ) kLimpRing, d = b + ( uint32_t ) kLimpRing;
                indices.push_back(a); indices.push_back(c); indices.push_back(b);
                indices.push_back(b); indices.push_back(c); indices.push_back(d);
            }
        }
    }
    tongueModel_->UpdateMesh(vertices, indices); // 空なら何も描かれない
    tongueObj_->SetCamera(camera);
    tongueObj_->Update();
}

void SwallowAbility::Draw() const{
    // ふだんのベロの見た目は、プレイヤーモデルの TongueOut アニメ（骨のベロ）。
    //   食べられない物にぶつかって波打っている間だけ、曲がるベロの絵（筒）を描く（その間、骨のベロは口の中へ隠す。PlayerAvatar）
    if ( limp_ && tongueObj_ ) { tongueObj_->Draw(); }
}

void SwallowAbility::InitializeMark(){
    markSprite_ = Sprite::Create(TextureManager::GetInstance()->Load("resources/ui/tongue_mark.png").srvIndex, { 0.0f, 0.0f });
}

void SwallowAbility::DrawSprite(){
    if ( markVisible_ && markSprite_ ) { markSprite_->Draw(); }
}
