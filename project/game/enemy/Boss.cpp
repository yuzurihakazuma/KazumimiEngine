#include "game/enemy/Boss.h"
#include "engine/math/VectorMath.h"
#include "game/card/CardDatabase.h"
#include <cmath>
#include <random>

using namespace VectorMath;

void Boss::Initialize() {
    pos_ = { 10.0f, -4.0f, 10.0f };
    rot_ = { 0.0f, 0.0f, 0.0f };
    scale_ = { 2.0f, 2.0f, 2.0f };

    state_ = State::Appear;    // 初期状態は登場演出

    maxHP_ = 25;               // 最大HP設定
    hp_ = maxHP_;              // HP全回復
    isDead_ = false;           // 死亡状態リセット

    thinkTimer_ = 0;           // 思考タイマー初期化

    isActionLocked_ = false;   // 行動ロック解除
    actionLockTimer_ = 0;      // ロック時間リセット

    isHit_ = false;                            // ヒット状態リセット
    hitTimer_ = 0;                             // ヒット時間リセット
    knockbackVelocity_ = { 0.0f, 0.0f, 0.0f }; // ノックバック初期化

    cardUseRequest_ = false;   // スキル使用フラグ初期化

    selectedCard_ = { -1, "", 0 }; // 選択カード初期化

    isAttackDebuffed_ = false; // 攻撃デバフ状態リセット
    attackDebuffTimer_ = 0;    // 攻撃デバフ時間リセット

    appearStartY_ = -4.0f;     // 地面下から出現
    appearTargetY_ = 1.5f;     // 最終位置
    appearTimer_ = appearDuration_; // 登場演出時間セット

    cardCooldownTimers_.clear(); // カードごとのクールタイム初期化

    InitializeBossCards(); // ボス専用カードを登録
}

void Boss::InitializeBossCards() {
    heldCards_.clear(); // 既存カードをクリア

    heldCards_.push_back(CardDatabase::GetCardData(101)); // BossClaw
    heldCards_.push_back(CardDatabase::GetCardData(102)); // BossFier
    heldCards_.push_back(CardDatabase::GetCardData(103)); // BossSummon
}

void Boss::Update() {
    if (isDead_) {
        state_ = State::Dead;
        return;
    }

    // 毎フレーム要求を初期化
    cardUseRequest_ = false;

    // 登場演出中は他の思考・攻撃・スキル処理を一切しない
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
        break;
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
        appearTimer_--; // 登場演出時間減少
    }

    // 0.0〜1.0に変換
    float t = 1.0f - (static_cast<float>(appearTimer_) / static_cast<float>(appearDuration_));

    // 少し自然に見える補間
    float eased = t * t * (3.0f - 2.0f * t);

    // 地面下からせり上がる
    pos_.y = appearStartY_ + (appearTargetY_ - appearStartY_) * eased;

    // 少し回して登場感を出す
    rot_.y += 0.03f;

    if (appearTimer_ <= 0) {
        pos_.y = appearTargetY_; // 最終位置に固定
        rot_.y = 0.0f;           // 回転を戻す
        state_ = State::Idle;    // 待機へ移行
        thinkTimer_ = 20;        // すぐ行動しないよう少し待つ
    }
}

void Boss::DecideNextState() {
    if (state_ == State::Appear) {
        return; // 登場演出中は状態遷移しない
    }

    // 詠唱中は状態遷移しない
    if (state_ == State::UseCard && isCasting_) {
        return;
    }

    Vector3 toPlayer = {
        playerPos_.x - pos_.x,
        0.0f,
        playerPos_.z - pos_.z
    };

    float playerDist = Length(toPlayer); // プレイヤーとの距離

    if (state_ == State::UseCard) {
        return;
    }

    if (playerDist <= chaseRange_) {
        state_ = State::Chase; // 範囲内なら常に追跡
    } else {
        state_ = State::Idle; // 範囲外なら待機
    }
}

void Boss::UpdateIdle() {
    Vector3 dir = {
        playerPos_.x - pos_.x,
        0.0f,
        playerPos_.z - pos_.z
    };

    float dist = Length(dir); // プレイヤーとの距離

    // すぐにChaseに入らないように少し待つ
    if (thinkTimer_ > 0) {
        return;
    }

    if (dist <= chaseRange_) {
        state_ = State::Chase; // 追跡範囲に入ったら追跡開始
        thinkTimer_ = 0;       // すぐ再判断できるようにする
    }
}

void Boss::UpdateChase() {
    // 詠唱状態が残っていたら解除
    if (isCasting_) {
        isCasting_ = false;
        scale_ = { 2.0f, 2.0f, 2.0f };
    }

    Vector3 dir = {
        playerPos_.x - pos_.x,
        0.0f,
        playerPos_.z - pos_.z
    };

    float dist = Length(dir); // プレイヤーとの距離

    // まずは常にプレイヤーの方向を向かせる
    if (dist > 0.01f) {
        Vector3 normDir = Normalize(dir);
        rot_.y = std::atan2f(normDir.x, normDir.z);
    }

    // カード全体のクールタイムが終わっていればカード使用へ
    if (cardCooldownTimer_ <= 0) {
        state_ = State::UseCard;
        thinkTimer_ = 0;
        return;
    }

    // クールタイム中は追跡
    if (dist > 2.0f) {
        Vector3 normDir = Normalize(dir);
        pos_ += normDir * chaseSpeed_; // プレイヤーへ移動
    }
}

void Boss::UpdateUseCard() {
    // 詠唱開始
    if (!isCasting_) {
        isCasting_ = true;
        castTimer_ = castTime_;
        return;
    }

    // 詠唱中
    if (castTimer_ > 0) {
        castTimer_--;

        // プレイヤーの方を向く
        Vector3 dir = {
            playerPos_.x - pos_.x,
            0.0f,
            playerPos_.z - pos_.z
        };
        if (Length(dir) > 0.01f) {
            rot_.y = std::atan2f(dir.x, dir.z);
        }

        // 詠唱の進行度
        float t = 1.0f - (float)castTimer_ / (float)castTime_;

        // 少しなめらかに大きくする
        float eased = t * t * (3.0f - 2.0f * t);

        // 2.0 → 3.0 に補間
        float scale = 2.0f + (3.0f - 2.0f) * eased;
        scale_ = { scale, scale, scale };

        return;
    }

    // 発動直前の大きさを固定
    scale_ = { 3.0f, 3.0f, 3.0f };

    // ---------------------------------------------------
    // 1. プレイヤーの方を向く
    // ---------------------------------------------------
    Vector3 dir = {
        playerPos_.x - pos_.x,
        0.0f,
        playerPos_.z - pos_.z
    };
    if (Length(dir) > 0.01f) {
        rot_.y = std::atan2f(dir.x, dir.z);
    }

    // ---------------------------------------------------
    // 2. 距離に合わせてカードを選ぶ
    // ---------------------------------------------------
    std::vector<Card> candidates;
    float dist = Length(dir);
    float closeRange = 6.0f; // 近距離の基準

    if (dist < closeRange) {
        for (const auto& card : heldCards_) {
            if (card.id == 101 && IsCardReady(card.id)) {
                candidates.push_back(card);
            }
        }
    } else {
        for (const auto& card : heldCards_) {
            if ((card.id == 102 || card.id == 103) && IsCardReady(card.id)) {
                candidates.push_back(card);
            }
        }
    }

    // 距離に合うカードが全部クールタイム中なら、使えるカード全体から探す
    if (candidates.empty()) {
        for (const auto& card : heldCards_) {
            if (IsCardReady(card.id)) {
                candidates.push_back(card);
            }
        }
    }

    // 全部クールタイム中なら今回は撃たない
    if (candidates.empty()) {
        state_ = State::Chase;
        thinkTimer_ = 20;
        isCasting_ = false;
        scale_ = { 2.0f, 2.0f, 2.0f };
        return;
    }

    // ---------------------------------------------------
    // 3. カード決定＆発動
    // ---------------------------------------------------
    if (!candidates.empty()) {
        static std::random_device rd;
        static std::mt19937 mt(rd());
        std::uniform_int_distribution<int> distCard(0, (int)candidates.size() - 1);

        selectedCard_ = candidates[distCard(mt)];

        cardUseRequest_ = true;
        cardCooldownTimer_ = 90;
    }

    if (selectedCard_.id == 101) {
        StartCardCooldown(101, 90);
    } else if (selectedCard_.id == 102) {
        StartCardCooldown(102, 150);
    } else if (selectedCard_.id == 103) {
        StartCardCooldown(103, 240);
    }

    // ---------------------------------------------------
    // 4. 状態戻す
    // ---------------------------------------------------
    state_ = State::Chase;
    thinkTimer_ = 60;

    isActionLocked_ = true;
    actionLockTimer_ = 35;

    // 詠唱リセット
    isCasting_ = false;
    scale_ = { 2.0f, 2.0f, 2.0f };
}

Card Boss::GetRandomDropCard() const {
    if (heldCards_.empty()) {
        return Card{ -1, "", 0 }; // カード未所持なら空を返す
    }

    static std::random_device rd;
    static std::mt19937 mt(rd());
    std::uniform_int_distribution<int> distCard(0, static_cast<int>(heldCards_.size()) - 1);

    return heldCards_[distCard(mt)]; // ランダムなカードを返す
}

void Boss::TakeDamage(int damage) {
    if (isDead_) {
        return; // 既に死亡していたら無視
    }

    if (state_ == State::UseCard) {
        cardUseRequest_ = false;     // スキル使用要求をキャンセル
        isActionLocked_ = false;     // 行動ロック解除
        actionLockTimer_ = 0;        // ロック時間リセット
        state_ = State::Chase;       // 追跡状態へ戻す
        thinkTimer_ = 15;            // 少し長めに立て直し時間を入れる
        isCasting_ = false;          // 詠唱解除
        scale_ = { 2.0f, 2.0f, 2.0f }; // 大きさを戻す
    }

    hp_ -= damage; // HP減少

    isHit_ = true;            // ヒット状態開始
    hitTimer_ = hitDuration_; // ヒット演出時間セット

    Vector3 hitDir = {
        pos_.x - playerPos_.x,
        0.0f,
        pos_.z - playerPos_.z
    };

    if (Length(hitDir) > 0.01f) {
        hitDir = Normalize(hitDir);
        knockbackVelocity_ = hitDir * 0.15f; // 少しだけノックバック
    }

    if (hp_ <= 0) {
        hp_ = 0;              // HP下限
        isDead_ = true;       // 死亡
        state_ = State::Dead; // 状態を死亡へ変更
    }
}

void Boss::SetActionLock(int frame) {
    isActionLocked_ = true;   // 行動ロック開始
    actionLockTimer_ = frame; // ロック時間設定
}

bool Boss::IsVisible() const {
    if (!isHit_) {
        return true; // 通常時は常に表示
    }

    return (hitTimer_ % 2) == 0; // ヒット中は点滅表示
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