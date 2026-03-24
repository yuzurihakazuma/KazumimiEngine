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

    attackRequest_ = false;    // 近接攻撃フラグ初期化
    cardUseRequest_ = false;   // スキル使用フラグ初期化

    attackCooldownTimer_ = 0;  // 近接攻撃クールダウン初期化
    skillCooldownTimer_ = 0;   // スキルクールダウン初期化

    selectedCard_ = { -1, "", 0 }; // 選択カード初期化

    isAttackDebuffed_ = false; // 攻撃デバフ状態リセット
    attackDebuffTimer_ = 0;    // 攻撃デバフ時間リセット

    appearStartY_ = -4.0f;     // 地面下から出現
    appearTargetY_ = 1.5f;     // 最終位置
    appearTimer_ = appearDuration_; // 登場演出時間セット

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
    attackRequest_ = false;
    cardUseRequest_ = false;

    // 登場演出中は他の思考・攻撃・スキル処理を一切しない
    if (state_ == State::Appear) {
        UpdateAppear();
        return;
    }

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

    Vector3 toPlayer = {
        playerPos_.x - pos_.x,
        0.0f,
        playerPos_.z - pos_.z
    };

    float playerDist = Length(toPlayer); // プレイヤーとの距離
    bool isPhase2 = (hp_ <= maxHP_ / 2); // HP半分以下で後半戦

    float attackEnter = attackEnterRange_;
    float skillEnter = skillEnterRange_;
    float skillExit = skillExitRange_;

    // 後半は少し強気にする
    if (isPhase2) {
        attackEnter = 3.5f; // 少し遠くでも近接に入る
        skillEnter = 10.0f; // 少し遠くでもスキルを使う
        skillExit = 12.0f;  // スキル維持距離も伸ばす
    }

    if (state_ == State::Attack) {
        if (playerDist > attackExitRange_) {
            state_ = State::Chase; // 離れたら追跡へ戻る
        }
        return;
    }

    if (state_ == State::UseSkill) {
        if (playerDist > skillExit) {
            state_ = State::Chase; // 離れたら追跡へ戻る
        }
        return;
    }

    if (playerDist <= attackEnter) {
        state_ = State::Attack; // 近ければ近接攻撃
    } else if (playerDist <= skillEnter && !heldCards_.empty()) {
        state_ = State::UseSkill; // 中距離ならスキル使用
    } else if (playerDist <= chaseRange_) {
        state_ = State::Chase; // 追跡範囲なら追いかける
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

    if (dist <= chaseRange_) {
        state_ = State::Chase; // 追跡範囲に入ったら追跡開始
        thinkTimer_ = 0;       // すぐ再判断できるようにする
    }
}

void Boss::UpdateChase() {
    Vector3 dir = {
        playerPos_.x - pos_.x,
        0.0f,
        playerPos_.z - pos_.z
    };

    float dist = Length(dir); // プレイヤーとの距離
    bool isPhase2 = (hp_ <= maxHP_ / 2); // HP半分以下で後半戦

    float attackEnter = attackEnterRange_;
    float skillEnter = skillEnterRange_;

    if (isPhase2) {
        attackEnter = 6.0f;
        skillEnter = 15.0f;
    }

    // まずは常にプレイヤーの方向を向かせる
    if (dist > 0.01f) {
        Vector3 normDir = Normalize(dir);
        rot_.y = std::atan2f(normDir.x, normDir.z);
    }

    // =========================================================
    // 攻撃の判定
    // =========================================================
   // ① 近接攻撃（一番優先！）
    if (dist <= attackEnter) {
        // クールダウンが少し残っていても、近距離なら強引に攻撃させる
        if (skillCooldownTimer_ <= 15) {
            state_ = State::UseSkill;
            thinkTimer_ = 0;
            skillCooldownTimer_ = 45; // 次の攻撃までの間隔
            selectedCard_ = CardDatabase::GetCardData(101); // 爪攻撃
            return;
        }
    }
    // ② 中・遠距離の魔法攻撃
    else if (skillCooldownTimer_ <= 0) {

        if (dist <= skillEnter) {
            // 中距離：ランダムなスキル
            state_ = State::UseSkill;
            thinkTimer_ = 0;
            skillCooldownTimer_ = 60;
            selectedCard_ = CardDatabase::GetCardData(102);
            return;
        } else {
            
            state_ = State::UseSkill;
            thinkTimer_ = 0;

            // 次の魔法を撃つまでの間隔（連発しすぎると避けれなくなるので、ここで調整します）
            // 60なら約1秒に1回、90なら約1.5秒に1回撃ってきます。
            skillCooldownTimer_ = 60;

            // 102(火) と 103(召喚) のどちらかを選ばせる
            int longRangeCardIds[] = { 102, 103 };
            int randId = longRangeCardIds[rand() % 2];
            selectedCard_ = CardDatabase::GetCardData(randId);
            return;
        }
    }

    // 移動処理（近づきすぎたら止まる）
  
    if (dist > attackEnter - 1.5f) {
        Vector3 normDir = Normalize(dir);
        pos_ += normDir * chaseSpeed_; // プレイヤーへ移動
    }
}

void Boss::UpdateAttack() {
    Vector3 dir = {
        playerPos_.x - pos_.x,
        0.0f,
        playerPos_.z - pos_.z
    };

    float dist = Length(dir); // プレイヤーとの距離

    if (dist > attackExitRange_) {
        state_ = State::Chase; // 離れたら追跡へ戻る
        thinkTimer_ = 0;       // すぐ再判断できるようにする
        return;
    }

    if (dist > 0.01f) {
        rot_.y = std::atan2f(dir.x, dir.z); // プレイヤー方向を向く
    }

    if (attackCooldownTimer_ > 0) {
        return; // クールダウン中は攻撃しない
    }

    attackRequest_ = true;                 // 攻撃要求を出す
    attackCooldownTimer_ = attackCooldown_; // 近接攻撃クールダウン開始

    SetActionLock(20); // 攻撃後の硬直
    thinkTimer_ = 20;  // 攻撃後すぐ再判断しない
}

void Boss::UpdateUseSkill() {
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
        // 近距離なら ID:101(BossClaw)
        for (const auto &card : heldCards_) {
            if (card.id == 101) candidates.push_back(card);
        }
    } else {
        // 中・遠距離なら ID:102(BossFier) と 103(BossSummon)
        for (const auto &card : heldCards_) {
            if (card.id == 102 || card.id == 103) candidates.push_back(card);
        }
    }

    // 候補がない場合の保険
    if (candidates.empty()) {
        candidates = heldCards_;
    }

    // ---------------------------------------------------
    // 3. カードを決定して発動の合図を出す！
    // ---------------------------------------------------
    if (!candidates.empty()) {
        static std::random_device rd;
        static std::mt19937 mt(rd());
        std::uniform_int_distribution<int> distCard(0, static_cast<int>(candidates.size()) - 1);

        selectedCard_ = candidates[distCard(mt)];

        cardUseRequest_ = true;               // ★ 発動のお願い！
        skillCooldownTimer_ = 60;             // ★ クールダウン開始（※skillCooldown_に変えてもOK）
    }

    // ---------------------------------------------------
    // 4. 撃ち終わったら追跡状態に戻り、硬直を入れる
    // ---------------------------------------------------
    state_ = State::Chase; // 魔法を撃ったらすぐChaseに戻す（ただし下のロックがかかるので動かない）
    thinkTimer_ = 60;      // 次の行動までの待機時間

    // ※Enemyと同じように、Updateの最初で isActionLocked_ なら return する処理がある前提です
    isActionLocked_ = true;
    actionLockTimer_ = 35; // 35フレーム（約0.5秒）硬直して動かなくなる
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

    if (state_ == State::UseSkill) {
        cardUseRequest_ = false;     // スキル使用要求をキャンセル
        isActionLocked_ = false;     // 行動ロック解除
        actionLockTimer_ = 0;        // ロック時間リセット
        state_ = State::Chase;       // 追跡状態へ戻す
        thinkTimer_ = 15;            // 少し長めに立て直し時間を入れる
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

