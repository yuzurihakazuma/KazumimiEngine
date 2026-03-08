#pragma once
#include "engine/math/VectorMath.h"
#include "game/card/HandManager.h"

class Enemy {
public:
    // 敵の行動状態
    enum class State {
        Patrol,       // 巡回
        MoveToCard,   // カードへ移動
        ChasePlayer,  // プレイヤー追跡
        AttackPlayer, // 近接攻撃
        UseCard       // カード使用
    };

    void Initialize(); // 初期化
    void Update();     // 更新

    // 各Transform取得
    const Vector3& GetPosition() const { return pos_; }      // 位置取得
    const Vector3& GetScale() const { return scale_; }       // スケール取得
    const Vector3& GetRotation() const { return rot_; }      // 回転取得

    // 各Transform設定
    void SetPosition(const Vector3& pos) { pos_ = pos; startX_ = pos.x; } // 位置設定
    void SetScale(const Vector3& scale) { scale_ = scale; }                // スケール設定

    // 各種ターゲット設定
    void SetPlayerPosition(const Vector3& pos) { playerPos_ = pos; }          // プレイヤー位置設定
    void SetTargetCardPosition(const Vector3& pos) { targetCardPos_ = pos; }  // 目標カード位置設定

    // 状態設定・取得
    void SetState(State state) { state_ = state; } // 状態設定
    State GetState() const { return state_; }      // 状態取得

    // カード所持関連
    bool HasCard() const { return hasCard_; }                    // カード所持判定
    void SetHeldCard(const Card& card) { heldCard_ = card; hasCard_ = true; } // カード所持
    const Card& GetHeldCard() const { return heldCard_; }        // 所持カード取得
    void ClearHeldCard() { hasCard_ = false; }                   // 所持カード解除

    // HP関連
    void TakeDamage(int damage);             // ダメージを受ける
    bool IsDead() const { return isDead_; } // 死亡判定
    int GetHP() const { return hp_; }       // HP取得

    // 行動ロック関連
    void SetActionLock(int frame);                            // 一定時間行動ロック
    bool IsActionLocked() const { return isActionLocked_; }   // 行動ロック中か

    // ヒット演出関連
    bool IsHit() const { return isHit_; } // ヒット中か
    bool IsVisible() const;               // 描画するか

    // -----------------------------
    // 攻撃・カード使用リクエスト
    // -----------------------------
    bool GetAttackRequest() const { return attackRequest_; } // 近接攻撃発生取得
    bool GetCardUseRequest() const { return cardUseRequest_; } // カード使用発生取得

    void ClearAttackRequest() { attackRequest_ = false; } // 近接攻撃発生クリア
    void ClearCardUseRequest() { cardUseRequest_ = false; } // カード使用発生クリア

private:
    void UpdatePatrol();        // 巡回処理
    void UpdateMoveToCard();    // カードへ向かう処理
    void UpdateChasePlayer();   // プレイヤー追跡処理
    void UpdateAttackPlayer();  // 近接攻撃処理
    void UpdateUseCard();       // カード使用処理

private:
    Vector3 pos_{ 5.0f, 0.0f, 5.0f };       // 位置
    Vector3 rot_{ 0.0f, 0.0f, 0.0f };       // 回転
    Vector3 scale_{ 1.0f, 1.0f, 1.0f };     // スケール

    float speed_ = 0.05f;                   // 巡回速度
    float chaseSpeed_ = 0.08f;              // 追跡速度
    float moveRange_ = 3.0f;                // 巡回範囲

    float attackRange_ = 2.0f;              // 近接攻撃距離
    float cardUseRange_ = 6.0f;             // カード使用距離

    float startX_ = 5.0f;                   // 巡回開始X
    int direction_ = 1;                     // 巡回方向

    State state_ = State::Patrol;           // 現在の状態

    Vector3 playerPos_{ 0.0f, 0.0f, 0.0f };     // プレイヤー位置
    Vector3 targetCardPos_{ 0.0f, 0.0f, 0.0f }; // 目標カード位置

    bool hasCard_ = false;                  // カード所持中か
    Card heldCard_{ -1, "", 0 };            // 所持カード

    int hp_ = 3;                            // 敵HP
    bool isDead_ = false;                   // 死亡フラグ

    bool isActionLocked_ = false;           // 行動ロック中
    int actionLockTimer_ = 0;               // ロック残り時間

    int thinkTimer_ = 0;                    // 思考更新までの時間

    bool isHit_ = false;                    // ヒット中フラグ
    int hitTimer_ = 0;                      // ヒット残り時間
    const int hitDuration_ = 10;            // ヒット演出時間
    Vector3 knockbackVelocity_{ 0.0f, 0.0f, 0.0f }; // ノックバック速度

    // -----------------------------
    // 攻撃・カード使用発生フラグ
    // GamePlayScene側で受け取って使う
    // -----------------------------
    bool attackRequest_ = false;            // 近接攻撃発生フラグ
    bool cardUseRequest_ = false;           // カード使用発生フラグ

    // -----------------------------
    // クールダウン
    // 連続攻撃・連続カード使用を防ぐ
    // -----------------------------
    int attackCooldownTimer_ = 0;           // 近接攻撃クールダウン
    int cardCooldownTimer_ = 0;             // カード使用クールダウン

    const int attackCooldown_ = 30;         // 近接攻撃クールダウン時間
    const int cardCooldown_ = 60;           // カード使用クールダウン時間
};