#include "game/player/AimThrowController.h"

#include "game/player/Player.h"
#include "game/enemy/EnemyManager.h"
#include "game/egg/EggSystem.h"
#include "game/egg/EggCombo.h"
#include "engine/2d/Sprite.h"
#include "engine/audio/AudioManager.h"
#include "engine/base/Input.h"
#include "engine/base/WindowProc.h"
#include "engine/camera/Camera.h"
#include "engine/graphics/DebugDraw.h"
#include "engine/graphics/TextureManager.h"
#include "game/enemy/Enemy.h"
#include "engine/math/VectorMath.h"
#include "engine/math/Matrix4x4.h"

#include <algorithm>
#include <cmath>

using namespace VectorMath;
using namespace MatrixMath;

AimThrowController::AimThrowController() = default;
AimThrowController::~AimThrowController() = default;

namespace {
    // 照準の色（紙の切り抜き。白地の絵にこの色を塗る）。背景も敵も同じ色（色で意味を分けない）
    const Vector4 kReticleColor { 0.97f, 0.50f, 0.38f, 1.0f };
    const Vector4 kDotColor     { 0.92f, 0.24f, 0.20f, 1.0f };
    const Vector4 kTrailColor   { 1.00f, 0.97f, 0.90f, 1.0f };
}

void AimThrowController::Initialize(){
    TextureManager* textures = TextureManager::GetInstance();
    const uint32_t ring = textures->Load("resources/ui/aim_ring.png").srvIndex;
    const uint32_t notch = textures->Load("resources/ui/aim_notch.png").srvIndex;
    const uint32_t dot = textures->Load("resources/ui/aim_dot.png").srvIndex;
    ringSprite_ = Sprite::Create(ring, { 0.0f, 0.0f });
    for ( auto& sprite : notchSprites_ ) { sprite = Sprite::Create(notch, { 0.0f, 0.0f }); }
    dotSprite_ = Sprite::Create(dot, { 0.0f, 0.0f });
    for ( auto& sprite : trailSprites_ ) { sprite = Sprite::Create(dot, { 0.0f, 0.0f }); }
    for ( auto& sprite : bounceSprites_ ) { sprite = Sprite::Create(dot, { 0.0f, 0.0f }); }
    state_ = State::Idle;
    aimLocked_ = false;
    burstT_ = -1.0f;
    reticleShown_ = false;
}

// 照準を目標へ寄せる：位置は少し遅れて追い、ロックで物の大きさに伸び縮みし、切れ込みがすぼまる
void AimThrowController::UpdateReticle(float targetX, float targetY, float targetSize, bool locked, float dt, float uiScale, float dotX, float dotY){
    if ( !reticleShown_ ) { reticleX_ = targetX; reticleY_ = targetY; reticleSize_ = targetSize; lockAmount_ = 0.0f; reticleShown_ = true; dotX_ = dotX; dotY_ = dotY; }
    const float dotFollow = ( std::min )( 24.0f * dt, 1.0f );
    dotX_ += ( dotX - dotX_ ) * dotFollow;
    dotY_ += ( dotY - dotY_ ) * dotFollow;
    const float follow = ( std::min )( ( locked ? 22.0f : 14.0f ) * dt, 1.0f );
    reticleX_ += ( targetX - reticleX_ ) * follow;
    reticleY_ += ( targetY - reticleY_ ) * follow;
    reticleSize_ += ( targetSize - reticleSize_ ) * ( std::min )( 12.0f * dt, 1.0f );
    lockAmount_ += ( ( locked ? 1.0f : 0.0f ) - lockAmount_ ) * ( std::min )( 14.0f * dt, 1.0f );
    uiScale_ = uiScale;
    PlaceReticle(uiScale, 0.0f, 1.0f);
}

// 照準のスプライトを置く。open＝投げた時に開く量（0〜1）、alpha＝濃さ
void AimThrowController::PlaceReticle(float uiScale, float open, float alpha){
    const float size = reticleSize_ * ( 1.0f + 0.6f * open );
    const float breathe = 1.0f + 0.04f * std::sin(pulseT_ * 6.0f) * ( 1.0f - lockAmount_ );
    // ロックしたら輪は出さず、切れ込みだけが物のふちに吸い付く（光っている物そのものが見えるように）。
    //   スプライトは α で薄くしない（α が画面の α に書かれて黒っぽく抜けるため。出す/出さないで切り替える）
    ( void ) alpha;
    ringVisible_ = lockAmount_ < 0.5f && open <= 0.0f;
    if ( ringSprite_ ) {
        const float ring = size * breathe;
        ringSprite_->SetPosition({ reticleX_, reticleY_ });
        ringSprite_->SetSize({ ring, ring });
        ringSprite_->SetColor(kReticleColor);
        ringSprite_->Update();
    }
    // 4つの切れ込み：斜め 4 方向。ロックで内側へすぼまり、投げると外へ開く
    const float spread = size * ( 0.56f - 0.12f * lockAmount_ + 0.5f * open ) * breathe;
    const float notch = ( 34.0f + 10.0f * lockAmount_ ) * uiScale;
    const float kQuarter = 1.5707963f;
    for ( int i = 0; i < 4; ++i ) {
        const float angle = 0.7853982f + kQuarter * i + 0.35f * open; // 右上から時計回り
        Sprite* sprite = notchSprites_[i].get();
        if ( !sprite ) { continue; }
        sprite->SetPosition({ reticleX_ + std::sin(angle) * spread, reticleY_ - std::cos(angle) * spread });
        sprite->SetSize({ notch, notch });
        sprite->SetRotation(angle); // 絵は下（真ん中）を向いている
        sprite->SetColor(kReticleColor);
        sprite->Update();
    }
    if ( dotSprite_ ) {
        const float dot = ( 16.0f + 4.0f * lockAmount_ ) * uiScale;
        dotSprite_->SetPosition({ dotX_, dotY_ });
        dotSprite_->SetSize({ dot, dot });
        dotSprite_->SetColor(kDotColor);
        dotSprite_->Update();
    }
}

// Q長押しで構え、矢印で「画面上のカーソル」を直感的に動かし、離して投げる。
//   ・ロック中はその敵へ。未ロックはカーソルの先(奥)へ投げる（クラフトワールド風）。
void AimThrowController::Update(Player& player, EnemyManager& enemies, EggSystem& eggs, Camera* camera, float dt){
    if ( !camera ) return;
    Input* input = Input::GetInstance();
    Vector3 playerPos = player.GetPosition();

    const float screenWidth = ( float ) WindowProc::GetInstance()->GetClientWidth();
    const float screenHeight = ( float ) WindowProc::GetInstance()->GetClientHeight();

    // ワールド点 → スクリーン画素。NDC計算は MatrixMath::WorldToNdc に一本化（カメラ後方なら false）
    auto project = [&](const Vector3& worldPos, float& screenX, float& screenY) -> bool {
        Vector2 ndc;
        if ( !WorldToNdc(worldPos, camera->GetViewProjectionMatrix(), ndc) ) return false;
        screenX = ( ndc.x * 0.5f + 0.5f ) * screenWidth;
        screenY = ( 1.0f - ( ndc.y * 0.5f + 0.5f ) ) * screenHeight;
        return true;
        };

    if ( state_ == State::Idle ) {
        // Q を押し始めた＆卵を持っている → 構えに入る（地上/ジャンプ/踏ん張り中どこでもOK）
        if ( input->Pushkey(DIK_Q) && eggs.HeldCount() > 0 ) {
            state_ = State::Aiming;
            aimAxisYaw_ = player.GetRotation().y; // レール軸の基準は構え開始時の向きで固定
                                                  // （狙いでプレイヤーが振り向いても軸はブレない）
            // 構え中も移動・ジャンプ・踏ん張りは受け付ける（狙いは矢印キーで別操作なので競合しない）
            // カーソルの初期位置：プレイヤーの少し前方上をスクリーン投影（無理なら画面中央）
            float projectedX, projectedY;
            // 坂道では坂に沿った前方を初めの狙いにする（水平だと上り坂では道へ向けて投げることになる）
            Vector3 facing = player.GetFacingAlongGround();
            Vector3 ahead = { playerPos.x + facing.x * 5.0f, playerPos.y + 1.0f + facing.y * 5.0f, playerPos.z + facing.z * 5.0f };
            if ( project(ahead, projectedX, projectedY) ) { cursorX_ = projectedX; cursorY_ = projectedY; }
            else { cursorX_ = screenWidth * 0.5f; cursorY_ = screenHeight * 0.45f; }
            // 照準は今のカーソルの所から出す（遠くから寄ってこない）
            reticleShown_ = false;
            burstT_ = -1.0f;
        } else {
            // 構えていない：光らせていた物を戻す。投げた直後なら、照準をぱっと開いて消す
            if ( combo_ ) { combo_->SetAimHighlight(-1); }
            enemies.SetHighlight(nullptr, 0.0f);
            if ( burstT_ >= 0.0f ) {
                burstT_ += dt;
                const float t = burstT_ / 0.16f;
                if ( t >= 1.0f ) { burstT_ = -1.0f; } else { PlaceReticle(uiScale_, t, 1.0f - t); }
            }
            trailCount_ = 0;
            return;
        }
    }

    // --- 構え中（Aiming）---
    // 矢印キーで「画面上のカーソル」を動かす（直感的：上=上 / 右=右 / 等速）。
    const float kCursorSpeed = 620.0f; // px/s
    if ( input->Pushkey(DIK_UP) )    cursorY_ -= kCursorSpeed * dt;
    if ( input->Pushkey(DIK_DOWN) )  cursorY_ += kCursorSpeed * dt;
    if ( input->Pushkey(DIK_LEFT) )  cursorX_ -= kCursorSpeed * dt;
    if ( input->Pushkey(DIK_RIGHT) ) cursorX_ += kCursorSpeed * dt;
    cursorX_ = std::clamp(cursorX_, 0.0f, screenWidth);
    cursorY_ = std::clamp(cursorY_, 0.0f, screenHeight);

    // --- ロックオン対象を探す：カーソル(自由位置)に画面上で一番近い敵（吸いつき無し）---
    Enemy* lockEnemy = nullptr;
    float  bestPixelDist = 1e9f, bestEnemyX = 0.0f, bestEnemyY = 0.0f;
    for ( auto& enemy : enemies.GetEnemies() ) {
        if ( !enemy->IsAlive() ) continue;
        float enemyScreenX, enemyScreenY;
        if ( !project(enemy->GetPosition(), enemyScreenX, enemyScreenY) ) continue; // 後方は対象外
        float pixelDist = std::sqrt(( enemyScreenX - cursorX_ ) * ( enemyScreenX - cursorX_ ) + ( enemyScreenY - cursorY_ ) * ( enemyScreenY - cursorY_ ));
        if ( pixelDist < bestPixelDist ) { bestPixelDist = pixelDist; lockEnemy = enemy.get(); bestEnemyX = enemyScreenX; bestEnemyY = enemyScreenY; }
    }
    float lockThreshold = aimLocked_ ? 120.0f : 80.0f; // 粘り（付く<外れる）
    aimLocked_ = ( lockEnemy != nullptr && bestPixelDist < lockThreshold );

    // 敵にロックしていない時：カーソルの先の背景（木・柵・岩・看板・花）をロック＝奥へ投げる
    EggCombo::Pick depthPick;
    depthLocked_ = !aimLocked_ && combo_
        && combo_->PickForAim(*camera, cursorX_, cursorY_, screenWidth, screenHeight, playerPos, depthPick);
    depthTarget_ = depthLocked_ ? depthPick.target : -1;

    Vector3 origin = { playerPos.x, playerPos.y + 0.5f, playerPos.z };
    Vector3 throwDir = { 0.0f, 0.0f, 1.0f };
    float   throwSpeed = throwSpeedNormal_;  // 通常の投げ速度（ノードエディタから調整可）
    float   displayX = cursorX_, displayY = cursorY_;
    Vector3 cursorWorld;         // 狙い線の先端（奥に追従させる）

    if ( depthLocked_ ) {
        // 背景の物にロック：照準は物に吸い付き、卵はカーソルが指す所（当てる所）へ。どこに当てるかで、はじける向きが変わる
        displayX = depthPick.screenX; displayY = depthPick.screenY;
        cursorWorld = depthPick.aimPoint;
        Vector3 toTarget = cursorWorld - origin;
        float targetDist = Length(toTarget);
        if ( targetDist > 1e-4f ) throwDir = { toTarget.x / targetDist, toTarget.y / targetDist, toTarget.z / targetDist };
        throwSpeed = throwSpeedLock_;
        depthRingSize_ = std::clamp(depthPick.screenRadius * 2.2f, 70.0f, 260.0f);
    } else if ( aimLocked_ ) {
        // ロック中：その敵へ。命中しやすいよう速度を上げる。カーソルは敵にピタッと合わせる。
        displayX = bestEnemyX; displayY = bestEnemyY;
        cursorWorld = lockEnemy->GetPosition();
        Vector3 toTarget = cursorWorld - origin;
        float targetDist = Length(toTarget);
        if ( targetDist > 1e-4f ) throwDir = { toTarget.x / targetDist, toTarget.y / targetDist, toTarget.z / targetDist };
        throwSpeed = throwSpeedLock_; // ★敵ロック時は速く（ノードエディタから調整可）
        if ( debugVisible_ ) { DebugDraw::GetInstance()->Sphere(cursorWorld, lockEnemy->GetRadius() + 0.25f, { 1.0f, 0.2f, 0.2f, 1.0f }, 16); }
    } else {
        // 未ロック：カーソルのレイと「レール軸の縦平面」の交点へ投げる。
        //   ・投げはレール軸の平面内に制限（奥へは飛ばない。奥狙いはロックオン時のみ）
        //   ・ビームが必ずカーソルを通る＝見えている場所と当たる場所が常に一致する
        //   ・交点が体の後ろ側なら、そのまま後ろへ投げられる（振り向きも追従）
        Vector3 axis = { std::sin(aimAxisYaw_), 0.0f, std::cos(aimAxisYaw_) };
        Matrix4x4 invVP = Inverse(camera->GetViewProjectionMatrix());
        float ndcX = cursorX_ / screenWidth * 2.0f - 1.0f;
        float ndcY = 1.0f - cursorY_ / screenHeight * 2.0f;
        Vector3 nearPoint = NdcToWorld(ndcX, ndcY, 0.0f, invVP);
        Vector3 farPoint  = NdcToWorld(ndcX, ndcY, 1.0f, invVP);
        Vector3 ray = { farPoint.x - nearPoint.x, farPoint.y - nearPoint.y, farPoint.z - nearPoint.z };
        Vector3 planeNormal = { axis.z, 0.0f, -axis.x }; // レール軸と直交する水平法線（軸とYが張る縦平面）
        float denom = ray.x * planeNormal.x + ray.z * planeNormal.z;
        Vector3 target = { origin.x + axis.x * 12.0f, origin.y + 1.0f, origin.z + axis.z * 12.0f }; // フォールバック
        if ( std::abs(denom) > 1e-6f ) {
            float tPlane = ( ( origin.x - nearPoint.x ) * planeNormal.x
                           + ( origin.z - nearPoint.z ) * planeNormal.z ) / denom;
            if ( tPlane > 0.0f ) {
                target = { nearPoint.x + ray.x * tPlane,
                           nearPoint.y + ray.y * tPlane,
                           nearPoint.z + ray.z * tPlane };
            }
        }
        Vector3 dir = { target.x - origin.x, target.y - origin.y, target.z - origin.z };
        float dirLength = Length(dir);
        if ( dirLength > 1e-4f ) throwDir = { dir.x / dirLength, dir.y / dirLength, dir.z / dirLength };
        // ビーム先端＝狙い点（遠すぎる時は18mでクランプ。カーソルは線の延長上に乗る）
        if ( dirLength > 18.0f ) {
            cursorWorld = { origin.x + throwDir.x * 18.0f, origin.y + throwDir.y * 18.0f, origin.z + throwDir.z * 18.0f };
        } else {
            cursorWorld = target;
        }
    }

    // プレイヤーの見た目もこの狙い方向を向く（GamePlayScene が毎フレーム反映）
    aimYaw_ = std::atan2(throwDir.x, throwDir.z);

    pulseT_ += dt;
    const float uiScale = screenHeight / 1080.0f;

    // 狙っている物を光らせる（1つだけ：背景の物か敵。同じ光り方）
    if ( combo_ ) { combo_->SetAimHighlight(depthLocked_ ? depthTarget_ : -1); }
    enemies.SetHighlight(aimLocked_ ? lockEnemy : nullptr, 0.5f + 0.5f * std::sin(pulseT_ * 5.0f));

    // 照準：ロックした物の画面上の大きさに合わせる。ロックしていない時はふつうの大きさ
    float reticleTarget = 110.0f * uiScale;
    if ( depthLocked_ ) {
        reticleTarget = std::clamp(depthPick.screenRadius * 2.4f, 90.0f * uiScale, 300.0f * uiScale);
    } else if ( aimLocked_ ) {
        float edgeX, edgeY;
        const Vector3 right = Normalize(Cross({ 0.0f, 1.0f, 0.0f }, Subtract(lockEnemy->GetPosition(), camera->GetWorldPosition())));
        if ( project(Add(lockEnemy->GetPosition(), Multiply(lockEnemy->GetRadius() + 0.2f, right)), edgeX, edgeY) ) {
            reticleTarget = std::clamp(std::abs(edgeX - bestEnemyX) * 2.6f, 80.0f * uiScale, 260.0f * uiScale);
        }
    }
    UpdateReticle(displayX, displayY, reticleTarget, depthLocked_ || aimLocked_, dt, uiScale,
                  depthLocked_ ? depthPick.aimX : displayX, depthLocked_ ? depthPick.aimY : displayY);

    // はじける向き：物の真ん中から、その向きへ小さな紙の点を 3 つ（どこに当てるとどっちへ飛ぶかが分かる）
    bounceCount_ = 0;
    if ( depthLocked_ ) {
        for ( int i = 0; i < ( int ) bounceSprites_.size(); ++i ) {
            const Vector3 point = Add(depthPick.center, Multiply(1.4f + 1.1f * i + 0.15f * std::sin(pulseT_ * 5.0f), depthPick.bounceDir));
            float sx, sy;
            if ( !project(point, sx, sy) || !bounceSprites_[i] ) { continue; }
            const float size = ( 15.0f - 3.0f * i ) * uiScale;
            bounceSprites_[bounceCount_]->SetPosition({ sx, sy });
            bounceSprites_[bounceCount_]->SetSize({ size, size });
            bounceSprites_[bounceCount_]->SetColor(kReticleColor);
            bounceSprites_[bounceCount_]->Update();
            ++bounceCount_;
        }
    }

    // 卵の通る道：口元から狙いまでに、小さな紙の点を 4 つだけ（デバッグの線の代わり）
    Vector3 beamStart = { origin.x, origin.y + 0.3f, origin.z };
    trailCount_ = 0;
    for ( int i = 0; i < ( int ) trailSprites_.size(); ++i ) {
        const float t = 0.2f + 0.18f * i + 0.04f * std::sin(pulseT_ * 4.0f); // ゆっくり流れる
        const Vector3 point = Add(beamStart, Multiply(t, Subtract(cursorWorld, beamStart)));
        float sx, sy;
        if ( !project(point, sx, sy) || !trailSprites_[i] ) { continue; }
        const float size = ( 11.0f + 3.0f * i ) * uiScale;
        trailSprites_[trailCount_]->SetPosition({ sx, sy });
        trailSprites_[trailCount_]->SetSize({ size, size });
        trailSprites_[trailCount_]->SetColor(kTrailColor);
        trailSprites_[trailCount_]->Update();
        ++trailCount_;
    }

    // 調べる時だけ：狙いの線と球（エディタの「当たり判定を表示」）
    if ( debugVisible_ ) {
        DebugDraw::GetInstance()->Line(beamStart, cursorWorld, { 1.0f, 0.18f, 0.12f, 0.95f });
        DebugDraw::GetInstance()->Sphere(cursorWorld, 0.16f, { 1.0f, 0.3f, 0.2f, 1.0f }, 10);
    }

    // Q を離した瞬間 → 投げる（ロック中=敵へ速く / 未ロック=カーソルの奥へ）。
    //   ※「投げずにキャンセル」したい時用のコマンドは別途キーで足せる（今は常に投げる）。
    // Xでキャンセル：投げずに構え解除。上にのせていた卵は消費されず後ろ（ストック）へ戻る
    if ( input->Triggerkey(DIK_X) ) {
        canceled_ = true;
        Reset();
        return;
    }

    if ( !input->Pushkey(DIK_Q) ) {
        // 投げる方向へプレイヤーの向きも合わせる（水平成分のyaw）
        if ( std::abs(throwDir.x) > 1e-4f || std::abs(throwDir.z) > 1e-4f ) {
            float yaw = std::atan2(throwDir.x, throwDir.z);
            Vector3 rotation = player.GetRotation();
            player.SetRotation({ rotation.x, yaw, rotation.z });
        }
        if ( Egg* egg = eggs.ThrowEgg(playerPos, throwDir, throwSpeed) ) {
            AudioManager::GetInstance()->PlayWave("resources/se/eggThrow.wav", false, 0.55f); // 投擲音
            if ( depthLocked_ && combo_ ) { combo_->Launch(egg, depthTarget_, depthPick.aimPoint); } // 奥の物の当てる所へ（当たるとはじけてコンボ）
        }
        state_ = State::Idle;
        aimLocked_ = false;
        depthLocked_ = false;
        burstT_ = 0.0f; // 照準がぱっと開いて消える
        trailCount_ = 0;
        if ( combo_ ) { combo_->SetAimHighlight(-1); }
        enemies.SetHighlight(nullptr, 0.0f);
    }
}

void AimThrowController::DrawSprite(){
    if ( state_ != State::Aiming && burstT_ < 0.0f ) { return; }
    if ( state_ == State::Aiming ) {
        for ( int i = 0; i < trailCount_; ++i ) { trailSprites_[i]->Draw(); }
        for ( int i = 0; i < bounceCount_; ++i ) { bounceSprites_[i]->Draw(); }
    }
    if ( ringSprite_ && ringVisible_ ) { ringSprite_->Draw(); }
    for ( auto& sprite : notchSprites_ ) { if ( sprite ) { sprite->Draw(); } }
    if ( dotSprite_ && burstT_ < 0.0f ) { dotSprite_->Draw(); }
}
