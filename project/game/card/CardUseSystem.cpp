#include "game/card/CardUseSystem.h"

#include <cmath>

#include "Engine/Camera/Camera.h"
#include "Engine/3D/Obj/Obj3d.h"
#include "game/player/Player.h"
#include "game/enemy/Enemy.h"
#include "engine/math/VectorMath.h"

using namespace VectorMath;

// 初期化
void CardUseSystem::Initialize(Camera* camera) {

    // パンチ用オブジェクト生成
    punchObj_ = Obj3d::Create("sphere");
    if (punchObj_) {
        punchObj_->SetCamera(camera);          // カメラ設定
        punchObj_->SetScale(punchScale_);      // 初期サイズ設定
    }

    // 火球用オブジェクト生成
    fireballObj_ = Obj3d::Create("sphere");
    if (fireballObj_) {
        fireballObj_->SetCamera(camera);       // カメラ設定
        fireballObj_->SetScale(fireballScale_);// 初期サイズ設定
    }

    // 演出状態をリセット
    Reset();
}

// 更新
void CardUseSystem::Update(Player* player, Enemy* enemy, const Vector3& playerPos, const Vector3& enemyPos) {

    // パンチ更新
    UpdatePunch(player, enemy, playerPos, enemyPos);

    // 火球更新
    UpdateFireball(player, enemy, playerPos, enemyPos);
}

// 描画
void CardUseSystem::Draw() {

    // パンチ描画
    if (isPunchActive_ && punchObj_) {
        punchObj_->Draw();
    }

    // 火球描画
    if (isFireballActive_ && fireballObj_) {
        fireballObj_->Draw();
    }
}

// カード使用
void CardUseSystem::UseCard(const Card& card, const Vector3& casterPos, float casterYaw, bool isPlayerCaster, Player* player) {

    // 使用者の前方向を計算
    Vector3 forward = {
        std::sinf(casterYaw),
        0.0f,
        std::cosf(casterYaw)
    };

    switch (card.id) {
    case 1: // パンチ
    {
        // プレイヤーが使った場合のみ行動ロック
        if (isPlayerCaster && player) {
            player->LockAction(10); // パンチ中は少し硬直
        }

        isPunchActive_ = true;                  // パンチ演出開始
        isPunchPlayerCaster_ = isPlayerCaster;  // 使用者情報を保存
        punchTimer_ = 10;                       // 表示時間設定

        // 使用者前方にパンチを配置
        punchPos_ = {
            casterPos.x + forward.x * 1.5f,
            casterPos.y,
            casterPos.z + forward.z * 1.5f
        };

        if (punchObj_) {
            punchObj_->SetTranslation(punchPos_); // パンチ位置更新
            punchObj_->SetScale(punchScale_);     // パンチサイズ更新
            punchObj_->Update();                  // 行列更新
        }
    }
    break;

    case 2: // 火球
    {
        // プレイヤーが使った場合のみ行動ロック
        if (isPlayerCaster && player) {
            player->LockAction(20); // 火球は少し長めに硬直
        }

        isFireballActive_ = true;                    // 火球演出開始
        isFireballPlayerCaster_ = isPlayerCaster;    // 使用者情報を保存

        // 使用者前方に火球を配置
        fireballPos_ = {
            casterPos.x + forward.x * 1.5f,
            casterPos.y,
            casterPos.z + forward.z * 1.5f
        };

        // 前方向へ飛ばす速度
        fireballVelocity_ = {
            forward.x * 0.3f,
            0.0f,
            forward.z * 0.3f
        };

        if (fireballObj_) {
            fireballObj_->SetTranslation(fireballPos_); // 火球位置更新
            fireballObj_->SetScale(fireballScale_);     // 火球サイズ更新
            fireballObj_->Update();                     // 行列更新
        }
    }
    break;

    default:
        // 未対応カードは何もしない
        break;
    }
}

// リセット
void CardUseSystem::Reset() {

    // パンチ状態リセット
    isPunchActive_ = false;
    isPunchPlayerCaster_ = true;
    punchPos_ = { 0.0f, 0.0f, 0.0f };
    punchTimer_ = 0;

    // 火球状態リセット
    isFireballActive_ = false;
    isFireballPlayerCaster_ = true;
    fireballPos_ = { 0.0f, 0.0f, 0.0f };
    fireballVelocity_ = { 0.0f, 0.0f, 0.0f };
}

// パンチ更新
void CardUseSystem::UpdatePunch(Player* player, Enemy* enemy, const Vector3& playerPos, const Vector3& enemyPos) {

    if (!isPunchActive_) {
        return; // パンチ演出中でなければ何もしない
    }

    // 残り時間更新
    punchTimer_--;

    if (punchTimer_ <= 0) {
        isPunchActive_ = false; // 時間切れで終了
        return;
    }

    // パンチオブジェクト更新
    if (punchObj_) {
        punchObj_->SetTranslation(punchPos_);
        punchObj_->SetScale(punchScale_);
        punchObj_->Update();
    }

    // プレイヤーが使ったパンチ
    if (isPunchPlayerCaster_) {

        if (enemy && !enemy->IsDead()) {
            Vector3 diff = {
                enemyPos.x - punchPos_.x,
                enemyPos.y - punchPos_.y,
                enemyPos.z - punchPos_.z
            }; // 敵との距離

            if (Length(diff) < 2.0f) {
                enemy->TakeDamage(1);   // 敵にダメージ
                isPunchActive_ = false; // ヒットしたら終了
            }
        }
    }
    // 敵が使ったパンチ
    else {

        if (player && !player->IsDead()) {
            Vector3 diff = {
                playerPos.x - punchPos_.x,
                playerPos.y - punchPos_.y,
                playerPos.z - punchPos_.z
            }; // プレイヤーとの距離

            if (Length(diff) < 2.0f) {
                player->TakeDamage(1, punchPos_); // プレイヤーにダメージ
                isPunchActive_ = false;           // ヒットしたら終了
            }
        }
    }
}

// 火球更新
void CardUseSystem::UpdateFireball(Player* player, Enemy* enemy, const Vector3& playerPos, const Vector3& enemyPos) {

    if (!isFireballActive_) {
        return; // 火球演出中でなければ何もしない
    }

    // 火球移動
    fireballPos_ += fireballVelocity_;

    // 火球オブジェクト更新
    if (fireballObj_) {
        fireballObj_->SetTranslation(fireballPos_);
        fireballObj_->SetScale(fireballScale_);
        fireballObj_->Update();
    }

    // プレイヤーが使った火球
    if (isFireballPlayerCaster_) {

        if (enemy && !enemy->IsDead()) {
            Vector3 diff = {
                enemyPos.x - fireballPos_.x,
                enemyPos.y - fireballPos_.y,
                enemyPos.z - fireballPos_.z
            }; // 敵との距離

            if (Length(diff) < 1.5f) {
                enemy->TakeDamage(1);     // 敵にダメージ
                isFireballActive_ = false;// ヒットしたら終了
                return;
            }
        }

        // プレイヤー位置から遠くまで飛んだら終了
        if (Length(fireballPos_ - playerPos) > 20.0f) {
            isFireballActive_ = false;
        }
    }
    // 敵が使った火球
    else {

        if (player && !player->IsDead()) {
            Vector3 diff = {
                playerPos.x - fireballPos_.x,
                playerPos.y - fireballPos_.y,
                playerPos.z - fireballPos_.z
            }; // プレイヤーとの距離

            if (Length(diff) < 1.5f) {
                player->TakeDamage(1, fireballPos_); // プレイヤーにダメージ
                isFireballActive_ = false;           // ヒットしたら終了
                return;
            }
        }

        // 敵位置から遠くまで飛んだら終了
        if (Length(fireballPos_ - enemyPos) > 20.0f) {
            isFireballActive_ = false;
        }
    }
}