#include "game/enemy/Boss.h"
#include "engine/math/VectorMath.h"
#include "game/card/CardDatabase.h"
#include <algorithm>
#include <cmath>
#include <random>

using namespace VectorMath;

namespace {
constexpr float kBossAppearRiseHeight = 5.5f;
}

void Boss::Initialize() {
    rot_ = { 0.0f, 0.0f, 0.0f };
    scale_ = { 2.0f, 2.0f, 2.0f };

    state_ = State::Appear;

    maxHP_ = 40;
    hp_ = maxHP_;
    isDead_ = false;

    thinkTimer_ = 0;

    isActionLocked_ = false;
    actionLockTimer_ = 0;

    isHit_ = false;
    hitTimer_ = 0;
    knockbackVelocity_ = { 0.0f, 0.0f, 0.0f };

    cardUseRequest_ = false;
    selectedCard_ = { -1, "", 0 };
    lastUsedCardId_ = -1;

    isAttackDebuffed_ = false;
    attackDebuffTimer_ = 0;

    appearTimer_ = appearDuration_;

    cardCooldownTimers_.clear();
    cardCooldownTimer_ = 0;
    castDurationCurrent_ = castTime_;

    SetSpawnPosition({ 10.0f, 2.0f, 10.0f });
    InitializeBossCards();
}

void Boss::InitializeBossCards() {
    heldCards_.clear();
    heldCards_.push_back(CardDatabase::GetCardData(101));
    heldCards_.push_back(CardDatabase::GetCardData(102));
    heldCards_.push_back(CardDatabase::GetCardData(103));
}

void Boss::Update() {
    if (isDead_) {
        state_ = State::Dead;
        return;
    }

    cardUseRequest_ = false;

    if (state_ == State::Appear) {
        UpdateAppear();
        return;
    }

    if (cardCooldownTimer_ > 0) {
        cardCooldownTimer_--;
    }

    for (auto& [cardId, timer] : cardCooldownTimers_) {
        if (timer > 0) {
            timer--;
        }
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

    case State::UseCard:
        UpdateUseCard();
        break;

    case State::Appear:
    case State::Dead:
        break;
    }

    if (isAttackDebuffed_) {
        attackDebuffTimer_--;
        if (attackDebuffTimer_ <= 0) {
            isAttackDebuffed_ = false;
        }
    }
}

void Boss::UpdateAppear() {
    if (appearTimer_ > 0) {
        appearTimer_--;
    }

    float t = 1.0f - (static_cast<float>(appearTimer_) / static_cast<float>(appearDuration_));
    float eased = t * t * (3.0f - 2.0f * t);

    pos_.y = appearStartY_ + (appearTargetY_ - appearStartY_) * eased;
    rot_.y += 0.03f;

    if (appearTimer_ <= 0) {
        pos_.y = appearTargetY_;
        rot_.y = 0.0f;
        state_ = State::Idle;
        thinkTimer_ = 20;
    }
}

void Boss::DecideNextState() {
    if (state_ == State::Appear) {
        return;
    }

    if (state_ == State::UseCard && isCasting_) {
        return;
    }

    Vector3 toPlayer = {
        playerPos_.x - pos_.x,
        0.0f,
        playerPos_.z - pos_.z
    };

    float playerDist = Length(toPlayer);

    if (state_ == State::UseCard) {
        return;
    }

    if (playerDist <= chaseRange_) {
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
    if (thinkTimer_ > 0) {
        return;
    }

    if (dist <= chaseRange_) {
        state_ = State::Chase;
        thinkTimer_ = 0;
    }
}

void Boss::UpdateChase() {
    if (isCasting_) {
        isCasting_ = false;
        scale_ = { 2.0f, 2.0f, 2.0f };
    }

    Vector3 dir = {
        playerPos_.x - pos_.x,
        0.0f,
        playerPos_.z - pos_.z
    };

    float dist = Length(dir);
    const bool isEnraged = hp_ <= (maxHP_ / 2);
    const float moveSpeed = isEnraged ? 0.11f : 0.08f;
    const float cardStartRange = isEnraged ? 28.0f : 24.0f;
    const float stopRange = isEnraged ? 2.4f : 3.0f;

    if (dist > 0.01f) {
        Vector3 normDir = Normalize(dir);
        rot_.y = std::atan2f(normDir.x, normDir.z);
    }

    // 中距離から技に入れるようにして、追いかけるだけの時間を短くする。
    if (cardCooldownTimer_ <= 0 && dist <= cardStartRange) {
        state_ = State::UseCard;
        thinkTimer_ = 0;
        return;
    }

    if (dist > stopRange) {
        Vector3 normDir = Normalize(dir);
        pos_ += normDir * moveSpeed;
    }
}

void Boss::UpdateUseCard() {
    const bool isEnraged = hp_ <= (maxHP_ / 2);
    const int castTime = isEnraged ? 40 : castTime_;

    if (!isCasting_) {
        isCasting_ = true;
        castDurationCurrent_ = castTime;
        castTimer_ = castDurationCurrent_;
        return;
    }

    if (castTimer_ > 0) {
        castTimer_--;

        Vector3 dir = {
            playerPos_.x - pos_.x,
            0.0f,
            playerPos_.z - pos_.z
        };
        if (Length(dir) > 0.01f) {
            rot_.y = std::atan2f(dir.x, dir.z);
        }

        float t = 1.0f - static_cast<float>(castTimer_) / static_cast<float>(castDurationCurrent_);
        float eased = t * t * (3.0f - 2.0f * t);
        float scale = 2.0f + (3.0f - 2.0f) * eased;
        scale_ = { scale, scale, scale };
        return;
    }

    scale_ = { 3.0f, 3.0f, 3.0f };

    Vector3 dir = {
        playerPos_.x - pos_.x,
        0.0f,
        playerPos_.z - pos_.z
    };
    if (Length(dir) > 0.01f) {
        rot_.y = std::atan2f(dir.x, dir.z);
    }

    std::vector<Card> candidates;
    auto addWeightedCard = [&](int cardId, int weight) {
        if (weight <= 0) {
            return;
        }

        for (const auto& card : heldCards_) {
            if (card.id != cardId || !IsCardReady(card.id)) {
                continue;
            }

            for (int i = 0; i < weight; ++i) {
                candidates.push_back(card);
            }
            return;
        }
    };

    float dist = Length(dir);
    if (dist < 5.5f) {
        addWeightedCard(101, 5);
        addWeightedCard(102, isEnraged ? 2 : 1);
        addWeightedCard(103, 1);
    } else if (dist < 14.0f) {
        addWeightedCard(102, isEnraged ? 7 : 5);
        addWeightedCard(103, isEnraged ? 3 : 2);
        addWeightedCard(101, 2);
    } else {
        addWeightedCard(102, isEnraged ? 8 : 6);
        addWeightedCard(103, isEnraged ? 5 : 4);
    }

    // 連続で同じカードばかりにならないように、別候補がある時は直前の札を外す。
    bool hasDifferentCard = false;
    for (const auto& card : candidates) {
        if (card.id != lastUsedCardId_) {
            hasDifferentCard = true;
            break;
        }
    }
    if (hasDifferentCard) {
        candidates.erase(
            std::remove_if(
                candidates.begin(),
                candidates.end(),
                [&](const Card& card) { return card.id == lastUsedCardId_; }
            ),
            candidates.end()
        );
    }

    if (candidates.empty()) {
        for (const auto& card : heldCards_) {
            if (IsCardReady(card.id)) {
                candidates.push_back(card);
            }
        }
    }

    if (candidates.empty()) {
        state_ = State::Chase;
        thinkTimer_ = 20;
        isCasting_ = false;
        scale_ = { 2.0f, 2.0f, 2.0f };
        return;
    }

    static std::random_device rd;
    static std::mt19937 mt(rd());
    std::uniform_int_distribution<int> distCard(0, static_cast<int>(candidates.size()) - 1);

    selectedCard_ = candidates[distCard(mt)];
    lastUsedCardId_ = selectedCard_.id;
    cardUseRequest_ = true;

    if (selectedCard_.id == 101) {
        cardCooldownTimer_ = isEnraged ? 28 : 38;
        StartCardCooldown(101, isEnraged ? 45 : 60);
    } else if (selectedCard_.id == 102) {
        cardCooldownTimer_ = isEnraged ? 24 : 34;
        StartCardCooldown(102, isEnraged ? 45 : 65);
    } else if (selectedCard_.id == 103) {
        cardCooldownTimer_ = isEnraged ? 48 : 64;
        StartCardCooldown(103, isEnraged ? 90 : 120);
    }

    state_ = State::Chase;
    thinkTimer_ = isEnraged ? 10 : 18;

    isActionLocked_ = true;
    actionLockTimer_ = isEnraged ? 10 : 16;

    isCasting_ = false;
    scale_ = { 2.0f, 2.0f, 2.0f };
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

    if (state_ == State::UseCard) {
        cardUseRequest_ = false;
        isActionLocked_ = false;
        actionLockTimer_ = 0;
        state_ = State::Chase;
        thinkTimer_ = 15;
        isCasting_ = false;
        scale_ = { 2.0f, 2.0f, 2.0f };
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

bool Boss::IsCardReady(int cardId) const {
    auto it = cardCooldownTimers_.find(cardId);
    if (it == cardCooldownTimers_.end()) {
        return true;
    }
    return it->second <= 0;
}

void Boss::StartCardCooldown(int cardId, int time) {
    cardCooldownTimers_[cardId] = time;
}

int Boss::GetCardCooldownTime(int cardId) const {
    auto it = cardCooldownTimers_.find(cardId);
    if (it == cardCooldownTimers_.end()) {
        return 0;
    }
    return it->second;
}

void Boss::SetSpawnPosition(const Vector3& pos) {
    appearTargetY_ = pos.y;
    appearStartY_ = pos.y - kBossAppearRiseHeight;
    pos_ = { pos.x, appearStartY_, pos.z };
}
