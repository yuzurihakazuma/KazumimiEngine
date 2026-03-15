#include "game/enemy/Boss.h"
#include "engine/math/VectorMath.h"
#include "game/card/CardDatabase.h"
#include <cmath>
#include <random>

using namespace VectorMath;

void Boss::Initialize() {
    pos_ = { 10.0f, 0.0f, 10.0f };
    rot_ = { 0.0f, 0.0f, 0.0f };
    scale_ = { 2.0f, 2.0f, 2.0f };

    state_ = State::Idle;

    maxHP_ = 30;
    hp_ = maxHP_;
    isDead_ = false;

    thinkTimer_ = 0;

    isActionLocked_ = false;
    actionLockTimer_ = 0;

    isHit_ = false;
    hitTimer_ = 0;
    knockbackVelocity_ = { 0.0f, 0.0f, 0.0f };

    attackRequest_ = false;
    cardUseRequest_ = false;

    attackCooldownTimer_ = 0;
    skillCooldownTimer_ = 0;

    selectedCard_ = { -1, "", 0 };

    InitializeBossCards();
}

void Boss::InitializeBossCards() {
    heldCards_.clear();

    heldCards_.push_back(CardDatabase::GetCardData(2)); // Fireball
    heldCards_.push_back(CardDatabase::GetCardData(6)); // IceBullet
    heldCards_.push_back(CardDatabase::GetCardData(2)); // Fireball
}

void Boss::Update() {
    if (isDead_) {
        state_ = State::Dead;
        return;
    }

    attackRequest_ = false;
    cardUseRequest_ = false;

    if (attackCooldownTimer_ > 0) {
        attackCooldownTimer_--;
    }

    if (skillCooldownTimer_ > 0) {
        skillCooldownTimer_--;
    }

    if (isHit_) {
        pos_ += knockbackVelocity_;
        knockbackVelocity_ *= 0.85f;

        hitTimer_--;
        if (hitTimer_ <= 0) {
            isHit_ = false;
            knockbackVelocity_ = { 0.0f, 0.0f, 0.0f };
        }
        return;
    }

    if (isActionLocked_) {
        actionLockTimer_--;
        if (actionLockTimer_ <= 0) {
            isActionLocked_ = false;
        }
        return;
    }

    if (thinkTimer_ > 0) {
        thinkTimer_--;
    }

    if (thinkTimer_ <= 0) {
        DecideNextState();
    }

    switch (state_) {
    case State::Idle:
        UpdateIdle();
        break;

    case State::Chase:
        UpdateChase();
        break;

    case State::Attack:
        UpdateAttack();
        break;

    case State::UseSkill:
        UpdateUseSkill();
        break;

    case State::Dead:
        break;
    }
}

void Boss::DecideNextState() {
    Vector3 toPlayer = {
        playerPos_.x - pos_.x,
        0.0f,
        playerPos_.z - pos_.z
    };

    float playerDist = Length(toPlayer);

    if (state_ == State::Attack) {
        if (playerDist > attackExitRange_) {
            state_ = State::Chase;
        }
        return;
    }

    if (state_ == State::UseSkill) {
        if (playerDist > skillExitRange_) {
            state_ = State::Chase;
        }
        return;
    }

    if (playerDist <= attackEnterRange_) {
        state_ = State::Attack;
    } else if (playerDist <= skillEnterRange_ && !heldCards_.empty()) {
        state_ = State::UseSkill;
    } else if (playerDist <= chaseRange_) {
        state_ = State::Chase;
    } else {
        state_ = State::Idle;
    }
}

void Boss::UpdateIdle() {
    Vector3 dir = {
        playerPos_.x - pos_.x,
        0.0f,
        playerPos_.z - pos_.z
    };

    float dist = Length(dir);

    if (dist <= chaseRange_) {
        state_ = State::Chase;
        thinkTimer_ = 0;
    }
}

void Boss::UpdateChase() {
    Vector3 dir = {
        playerPos_.x - pos_.x,
        0.0f,
        playerPos_.z - pos_.z
    };

    float dist = Length(dir);

    if (dist <= attackEnterRange_) {
        state_ = State::Attack;
        thinkTimer_ = 0;
        return;
    }

    if (dist <= skillEnterRange_ && skillCooldownTimer_ <= 0 && !heldCards_.empty()) {
        state_ = State::UseSkill;
        thinkTimer_ = 0;
        return;
    }

    if (dist > 0.01f) {
        dir = Normalize(dir);
        pos_ += dir * chaseSpeed_;
        rot_.y = std::atan2f(dir.x, dir.z);
    }
}

void Boss::UpdateAttack() {
    Vector3 dir = {
        playerPos_.x - pos_.x,
        0.0f,
        playerPos_.z - pos_.z
    };

    float dist = Length(dir);

    if (dist > attackExitRange_) {
        state_ = State::Chase;
        thinkTimer_ = 0;
        return;
    }

    if (dist > 0.01f) {
        rot_.y = std::atan2f(dir.x, dir.z);
    }

    if (attackCooldownTimer_ > 0) {
        return;
    }

    attackRequest_ = true;
    attackCooldownTimer_ = attackCooldown_;

    SetActionLock(20);
    thinkTimer_ = 20;
}

void Boss::UpdateUseSkill() {
    Vector3 dir = {
        playerPos_.x - pos_.x,
        0.0f,
        playerPos_.z - pos_.z
    };

    float dist = Length(dir);

    if (dist > skillExitRange_) {
        state_ = State::Chase;
        thinkTimer_ = 0;
        return;
    }

    if (dist > 0.01f) {
        rot_.y = std::atan2f(dir.x, dir.z);
    }

    if (skillCooldownTimer_ > 0 || heldCards_.empty()) {
        state_ = State::Chase;
        thinkTimer_ = 0;
        return;
    }

    static std::random_device rd;
    static std::mt19937 mt(rd());
    std::uniform_int_distribution<int> distCard(0, static_cast<int>(heldCards_.size()) - 1);

    selectedCard_ = heldCards_[distCard(mt)];
    cardUseRequest_ = true;
    skillCooldownTimer_ = skillCooldown_;

    SetActionLock(35);
    thinkTimer_ = 35;
}

Card Boss::GetRandomDropCard() const {
    if (heldCards_.empty()) {
        return Card{ -1, "", 0 };
    }

    static std::random_device rd;
    static std::mt19937 mt(rd());
    std::uniform_int_distribution<int> distCard(0, static_cast<int>(heldCards_.size()) - 1);

    return heldCards_[distCard(mt)];
}

void Boss::TakeDamage(int damage) {
    if (isDead_) {
        return;
    }

    // 被弾でカード詠唱キャンセル
    if (state_ == State::UseSkill) {
        cardUseRequest_ = false;
        isActionLocked_ = false;
        actionLockTimer_ = 0;
        state_ = State::Chase;
        thinkTimer_ = 15; // ボスは少し長めに立て直し
    }

    hp_ -= damage;

    isHit_ = true;
    hitTimer_ = hitDuration_;

    Vector3 hitDir = {
        pos_.x - playerPos_.x,
        0.0f,
        pos_.z - playerPos_.z
    };

    if (Length(hitDir) > 0.01f) {
        hitDir = Normalize(hitDir);
        knockbackVelocity_ = hitDir * 0.15f;
    }

    if (hp_ <= 0) {
        hp_ = 0;
        isDead_ = true;
        state_ = State::Dead;
    }
}

void Boss::SetActionLock(int frame) {
    isActionLocked_ = true;
    actionLockTimer_ = frame;
}

bool Boss::IsVisible() const {
    if (!isHit_) {
        return true;
    }

    return (hitTimer_ % 2) == 0;
}