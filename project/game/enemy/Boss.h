#pragma once
#include "engine/math/VectorMath.h"
#include "game/card/HandManager.h"
#include <vector>

class Boss {
public:
    enum class State {
        Idle,
        Chase,
        Attack,
        UseSkill,
        Dead
    };

public:
    void Initialize();
    void Update();

    // Transform
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

    // 通常攻撃リクエスト
    bool GetAttackRequest() const { return attackRequest_; }
    void ClearAttackRequest() { attackRequest_ = false; }

    // カード使用リクエスト
    bool GetCardUseRequest() const { return cardUseRequest_; }
    void ClearCardUseRequest() { cardUseRequest_ = false; }

    // 今回使うカード
    const Card& GetSelectedCard() const { return selectedCard_; }

    // ドロップ用
    bool HasAnyCard() const { return !heldCards_.empty(); }
    Card GetRandomDropCard() const;

private:
    void DecideNextState();

    void UpdateIdle();
    void UpdateChase();
    void UpdateAttack();
    void UpdateUseSkill();

    void InitializeBossCards();

private:
    Vector3 pos_{ 10.0f, 0.0f, 10.0f };
    Vector3 rot_{ 0.0f, 0.0f, 0.0f };
    Vector3 scale_{ 2.0f, 2.0f, 2.0f };

    Vector3 playerPos_{ 0.0f, 0.0f, 0.0f };

    State state_ = State::Idle;

    int hp_ = 20;
    int maxHP_ = 20;
    bool isDead_ = false;

    float chaseSpeed_ = 0.06f;

    float chaseRange_ = 20.0f;
    float attackEnterRange_ = 2.8f;
    float attackExitRange_ = 4.0f;

    float skillEnterRange_ = 8.0f;
    float skillExitRange_ = 10.0f;

    int thinkTimer_ = 0;

    bool isActionLocked_ = false;
    int actionLockTimer_ = 0;

    bool isHit_ = false;
    int hitTimer_ = 0;
    const int hitDuration_ = 10;
    Vector3 knockbackVelocity_{ 0.0f, 0.0f, 0.0f };

    bool attackRequest_ = false;
    bool cardUseRequest_ = false;

    int attackCooldownTimer_ = 0;
    int skillCooldownTimer_ = 0;

    const int attackCooldown_ = 45;
    const int skillCooldown_ = 120;

    // ボス固有カード
    std::vector<Card> heldCards_;
    Card selectedCard_{ -1, "", 0 };
};