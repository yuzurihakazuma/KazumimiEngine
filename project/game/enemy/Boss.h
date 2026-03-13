#pragma once
#include "engine/math/VectorMath.h"

class Boss {
public:
    enum class State {
        Idle,       // 待機
        Chase,      // プレイヤー追跡
        Attack,     // 通常攻撃
        UseSkill,   // スキル使用
        Dead        // 死亡
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

    // 攻撃リクエスト
    bool GetAttackRequest() const { return attackRequest_; }
    void ClearAttackRequest() { attackRequest_ = false; }

    // スキルリクエスト
    bool GetSkillRequest() const { return skillRequest_; }
    void ClearSkillRequest() { skillRequest_ = false; }

private:
    void DecideNextState();

    void UpdateIdle();
    void UpdateChase();
    void UpdateAttack();
    void UpdateUseSkill();

private:
    Vector3 pos_{ 10.0f, 0.0f, 10.0f };
    Vector3 rot_{ 0.0f, 0.0f, 0.0f };
    Vector3 scale_{ 2.0f, 2.0f, 2.0f };

    Vector3 playerPos_{ 0.0f, 0.0f, 0.0f };

    State state_ = State::Idle;

    int hp_ = 20;
    int maxHP_ = 20;
    bool isDead_ = false;

    // 移動
    float chaseSpeed_ = 0.06f;

    // 距離
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

    // ヒット演出
    bool isHit_ = false;
    int hitTimer_ = 0;
    const int hitDuration_ = 10;
    Vector3 knockbackVelocity_{ 0.0f, 0.0f, 0.0f };

    // 攻撃リクエスト
    bool attackRequest_ = false;
    bool skillRequest_ = false;

    // クールダウン
    int attackCooldownTimer_ = 0;
    int skillCooldownTimer_ = 0;

    const int attackCooldown_ = 45;
    const int skillCooldown_ = 120;
};