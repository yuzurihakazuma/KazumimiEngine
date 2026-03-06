#pragma once
#include "engine/math/VectorMath.h"
#include "game/card/HandManager.h"

class Enemy {
public:
    enum class State {
        Patrol,
        MoveToCard,
        ChasePlayer
    };

    void Initialize();
    void Update();

    const Vector3& GetPosition() const { return pos_; }
    const Vector3& GetScale() const { return scale_; }
    const Vector3& GetRotation() const { return rot_; }

    void SetPosition(const Vector3& pos) { pos_ = pos; startX_ = pos.x; }
    void SetScale(const Vector3& scale) { scale_ = scale; }

    void SetPlayerPosition(const Vector3& pos) { playerPos_ = pos; }

    void SetTargetCardPosition(const Vector3& pos) { targetCardPos_ = pos; }
    void SetState(State state) { state_ = state; }
    State GetState() const { return state_; }

    bool HasCard() const { return hasCard_; }
    void SetHeldCard(const Card& card) { heldCard_ = card; hasCard_ = true; }
    const Card& GetHeldCard() const { return heldCard_; }
    void ClearHeldCard() { hasCard_ = false; }

private:
    void UpdatePatrol();
    void UpdateMoveToCard();
    void UpdateChasePlayer();

private:
    Vector3 pos_{ 5.0f, 0.0f, 5.0f };
    Vector3 rot_{ 0.0f, 0.0f, 0.0f };
    Vector3 scale_{ 1.0f, 1.0f, 1.0f };

    float speed_ = 0.05f;
    float chaseSpeed_ = 0.08f;
    float moveRange_ = 3.0f;

    float startX_ = 5.0f;
    int direction_ = 1;

    State state_ = State::Patrol;

    Vector3 playerPos_{ 0.0f, 0.0f, 0.0f };
    Vector3 targetCardPos_{ 0.0f, 0.0f, 0.0f };

    bool hasCard_ = false;
    Card heldCard_{ -1, "", 0 };
};