#include "game/enemy/Enemy.h"
#include "engine/math/VectorMath.h"

using namespace VectorMath;

void Enemy::Initialize() {
    pos_ = { 5.0f, 0.0f, 5.0f };
    rot_ = { 0.0f, 0.0f, 0.0f };
    scale_ = { 1.0f, 1.0f, 1.0f };

    startX_ = pos_.x;
    state_ = State::Patrol;
    hasCard_ = false;
}

void Enemy::Update() {
    switch (state_) {
    case State::Patrol:
        UpdatePatrol();
        break;

    case State::MoveToCard:
        UpdateMoveToCard();
        break;

    case State::ChasePlayer:
        UpdateChasePlayer();
        break;
    }
}

void Enemy::UpdatePatrol() {
    pos_.x += speed_ * direction_;

    if (direction_ > 0) {
        rot_.y = 1.57f;
    } else {
        rot_.y = -1.57f;
    }

    if (pos_.x > startX_ + moveRange_) {
        direction_ = -1;
    }

    if (pos_.x < startX_ - moveRange_) {
        direction_ = 1;
    }
}

void Enemy::UpdateMoveToCard() {
    Vector3 dir = {
        targetCardPos_.x - pos_.x,
        0.0f,
        targetCardPos_.z - pos_.z
    };

    if (Length(dir) > 0.01f) {
        dir = Normalize(dir);
        pos_ += dir * chaseSpeed_;
    }

    if (dir.x > 0.0f) {
        rot_.y = 1.57f;
    } else if (dir.x < 0.0f) {
        rot_.y = -1.57f;
    }
}

void Enemy::UpdateChasePlayer() {
    Vector3 dir = {
        playerPos_.x - pos_.x,
        0.0f,
        playerPos_.z - pos_.z
    };

    if (Length(dir) > 0.01f) {
        dir = Normalize(dir);
        pos_ += dir * chaseSpeed_;
    }

    if (dir.x > 0.0f) {
        rot_.y = 1.57f;
    } else if (dir.x < 0.0f) {
        rot_.y = -1.57f;
    }
}