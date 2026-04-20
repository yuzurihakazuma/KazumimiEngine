#pragma once
#include <memory>
#include "engine/math/VectorMath.h"
#include "engine/3d/obj/SkinnedObj3d.h"

class Camera;

class Player {
public:
    void Initialize();  // 初期化
    void Update();      // 更新
    void Draw();        // 描画

    void SetCamera(const Camera* camera); // カメラ設定

    // 各Transform取得
    const Vector3& GetPosition() const { return pos_; }      // 位置取得
    const Vector3& GetScale() const { return scale_; }       // スケール取得
    const Vector3& GetRotation() const { return rot_; }      // 回転取得

    // 各Transform設定
    void SetPosition(const Vector3& pos) { pos_ = pos; }     // 位置設定
    void SetScale(const Vector3& scale) { scale_ = scale; }  // スケール設定
    void SetRotation(const Vector3& rot) { rot_ = rot; }     // 回転設定

    void LockAction(int frame); // 指定フレームの間プレイヤー操作をロック

    // レベル・経験値取得
    int GetLevel() const { return level_; }
    int GetExp() const { return exp_; }
    int GetNextLevelExp() const { return nextLevelExp_; }

    // コスト取得
    int GetCost() const { return cost_; }
    int GetMaxCost() const { return maxCost_; }

    void AddExp(int value);      // 経験値加算
    bool CanUseCost(int value) const; // コスト使用可能判定
    void UseCost(int value);     // コスト消費

    int GetHP() const { return hp_; }         // 現在HP取得
    int GetMaxHP() const { return maxHp_; }   // 最大HP取得
    bool IsDead() const { return isDead_; }   // 死亡判定
    bool IsInvincible() const { return isInvincible_; } // 無敵状態判定
    bool IsDodging() const { return isDodging_; }       // 回避中判定

    bool IsHit() const { return isHit_; }         // 被弾中か
    bool IsVisible() const;                       // 描画するか

    bool IsKnockback() const { return isKnockback_; } // ノックバック中か

    void TakeDamage(int damage, const Vector3& attackFrom); // ダメージ処理

    void SetInputEnable(bool enable) { isInputEnabled_ = enable; } // 入力有効/無効切り替え

    void ApplySpeedBuff(float multiplier, int durationFrames); // スピードバフ適用の関数

    float GetSpeedMultiplier() const { return speedMultiplier_; } // 現在の速度倍率取得

    void Heal(int amount); // HP回復処理

    bool IsShieldActive() const { return isShieldActive_; }  // シールド状態の取得

    void AddShieldHits(int hits) { shieldHitCount_ = hits; }  // シールドの回数をセットする関数

    int GetShieldHits() const { return shieldHitCount_; }  // 今のシールドの残り回数を取得する関数

    bool IsActionLocked() const { return isActionLocked_; }

    void SetMaxCost(int cost) { maxCost_ = cost; }
    void SetCost(int cost) { cost_ = cost; }
    void SetHP(int hp) { hp_ = hp; }

    void SetEnemyAtkDebuffed(bool isDebuffed) { isEnemyAtkDebuffed_ = isDebuffed; }
    bool IsEnemyAtkDebuffed() const { return isEnemyAtkDebuffed_; }

private:
    void LevelUp();      // レベルアップ処理
    void UpdateCost();   // コスト自然回復

private:
    Vector3 pos_{ 0.0f, 0.0f, 0.0f };     // 位置
    Vector3 rot_{ 0.0f, 0.0f, 0.0f };     // 回転
    Vector3 scale_{ 1.0f, 1.0f, 1.0f };   // スケール

    std::unique_ptr<SkinnedObj3d> model_ = nullptr; // プレイヤーのアニメーションモデル
    const Camera* camera_ = nullptr;                // 使用カメラ

    float moveSpeed_ = 0.2f;              // 通常移動速度

    // 回避
    bool isDodging_ = false;              // 回避中フラグ
    int dodgeTimer_ = 0;                  // 回避残り時間
    const int dodgeDuration_ = 15;        // 回避継続フレーム
    float dodgeSpeed_ = 0.6f;             // 回避速度
    Vector3 dodgeDirection_{ 0.0f, 0.0f, 0.0f }; // 回避方向

    // 行動ロック
    bool isActionLocked_ = false;         // 行動ロック中
    int actionLockTimer_ = 0;             // ロック残り時間

    // レベル
    int level_ = 1;                       // 現在レベル
    int exp_ = 0;                         // 現在経験値
    int nextLevelExp_ = 3;                // 次レベル必要経験値

    // コスト
    int cost_ = 3;                        // 現在コスト
    int maxCost_ = 3;                     // 最大コスト

    int costRecoveryTimer_ = 0;           // コスト回復タイマー
    int costRecoveryInterval_ = 180;      // コスト回復間隔

    int hp_ = 8;                          // 現在HP
    int maxHp_ = 8;                       // 最大HP

    bool isDead_ = false;                 // 死亡フラグ

    bool isInvincible_ = false;           // 被弾後無敵
    int invincibleTimer_ = 0;             // 無敵残り時間
    const int invincibleDuration_ = 30;   // 無敵時間

    // 被弾演出
    bool isHit_ = false;              // 被弾中フラグ
    int hitTimer_ = 0;                // 被弾演出残り時間
    const int hitDuration_ = 20;      // 被弾演出時間

    // ノックバック
    bool isKnockback_ = false;                    // ノックバック中フラグ
    int knockbackTimer_ = 0;                      // ノックバック残り時間
    const int knockbackDuration_ = 10;            // ノックバック時間
    Vector3 knockbackVelocity_{ 0.0f, 0.0f, 0.0f }; // ノックバック速度

    bool isInputEnabled_ = true; // 入力有効フラグ

    // 速度
    float speedMultiplier_ = 1.0f; // 移動速度倍率
    int speedBuffTimer_ = 0; // スピードアップの残り時間

    // シールド管理
    bool isShieldActive_ = false; // シールド展開中か
    int shieldHitCount_ = 0;

    bool isEnemyAtkDebuffed_ = false;
};