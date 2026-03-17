#pragma once
#include "engine/math/VectorMath.h"
#include "game/card/HandManager.h"
#include <vector>

class Boss {
public:
    // ボスの行動状態
    enum class State {
        Appear,    // 登場演出
        Idle,      // 待機
        Chase,     // プレイヤー追跡
        Attack,    // 近接攻撃
        UseSkill,  // スキル使用
        Dead       // 死亡
    };

public:
    void Initialize(); // 初期化
    void Update();     // 更新

    // Transform取得
    const Vector3& GetPosition() const { return pos_; }
    const Vector3& GetRotation() const { return rot_; }
    const Vector3& GetScale() const { return scale_; }

    void SetPosition(const Vector3& pos) { pos_ = pos; }
    void SetScale(const Vector3& scale) { scale_ = scale; }

    // プレイヤー情報
    void SetPlayerPosition(const Vector3& pos) { playerPos_ = pos; }

    // 状態
    void SetState(State state) { state_ = state; }
    State GetState() const { return state_; }

    // 登場演出中か
    bool IsAppearing() const { return state_ == State::Appear; }

    // HP
    void TakeDamage(int damage);
    int GetHP() const { return hp_; }
    int GetMaxHP() const { return maxHP_; }
    bool IsDead() const { return isDead_; }

    // 表示
    bool IsHit() const { return isHit_; }
    bool IsVisible() const;

    // 行動ロック
    void SetActionLock(int frame);
    bool IsActionLocked() const { return isActionLocked_; }

    // 攻撃リクエスト
    bool GetAttackRequest() const { return attackRequest_; }
    void ClearAttackRequest() { attackRequest_ = false; }

    // スキルリクエスト
    bool GetCardUseRequest() const { return cardUseRequest_; }
    void ClearCardUseRequest() { cardUseRequest_ = false; }

    // 使用カード
    const Card& GetSelectedCard() const { return selectedCard_; }

    // ドロップ用
    bool HasAnyCard() const { return !heldCards_.empty(); }
    Card GetRandomDropCard() const;

    // デバフ
    void ApplyAttackDebuff(int duration) {
        isAttackDebuffed_ = true;
        attackDebuffTimer_ = duration;
    }

    bool IsAttackDebuffed() const { return isAttackDebuffed_; }

private:
    void DecideNextState();

    void UpdateAppear();   // 登場演出
    void UpdateIdle();
    void UpdateChase();
    void UpdateAttack();
    void UpdateUseSkill();

    void InitializeBossCards();

private:
    // Transform
    Vector3 pos_{ 10.0f, 0.0f, 10.0f };
    Vector3 rot_{ 0.0f, 0.0f, 0.0f };
    Vector3 scale_{ 2.0f, 2.0f, 2.0f };

    // プレイヤー位置
    Vector3 playerPos_{ 0.0f, 0.0f, 0.0f };

    // 状態
    State state_ = State::Appear;

    // HP
    int hp_ = 20;
    int maxHP_ = 20;
    bool isDead_ = false;

    // 移動
    float chaseSpeed_ = 0.06f;

    // 距離設定
    float chaseRange_ = 20.0f;
    float attackEnterRange_ = 2.8f;
    float attackExitRange_ = 4.0f;

    float skillEnterRange_ = 8.0f;
    float skillExitRange_ = 10.0f;

    // 思考
    int thinkTimer_ = 0;

    // 行動ロック
    bool isActionLocked_ = false;
    int actionLockTimer_ = 0;

    // ヒット処理
    bool isHit_ = false;
    int hitTimer_ = 0;
    const int hitDuration_ = 10;
    Vector3 knockbackVelocity_{ 0.0f, 0.0f, 0.0f };

    // 攻撃・スキル
    bool attackRequest_ = false;
    bool cardUseRequest_ = false;

    int attackCooldownTimer_ = 0;
    int skillCooldownTimer_ = 0;

    const int attackCooldown_ = 45;
    const int skillCooldown_ = 120;

    // カード
    std::vector<Card> heldCards_;
    Card selectedCard_{ -1, "", 0 };

    // デバフ
    bool isAttackDebuffed_ = false;
    int attackDebuffTimer_ = 0;

    // 登場演出
    float appearStartY_ = -4.0f;     // 開始位置（地面下）
    float appearTargetY_ = 2.0f;     // 最終位置
    int appearTimer_ = 0;            // 残り時間
    const int appearDuration_ = 60;  // 演出フレーム数
};