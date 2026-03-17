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

    maxHP_ = 30;               // 最大HP設定
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
    appearTargetY_ = 2.0f;     // 最終位置
    appearTimer_ = appearDuration_; // 登場演出時間セット

    InitializeBossCards(); // ボス専用カードを登録
}

void Boss::InitializeBossCards() {
    heldCards_.clear(); // 既存カードをクリア

    heldCards_.push_back(CardDatabase::GetCardData(2)); // Fireball追加
    heldCards_.push_back(CardDatabase::GetCardData(6)); // IceBullet追加
    heldCards_.push_back(CardDatabase::GetCardData(2)); // Fireball追加
}

void Boss::Update() {
    if (isDead_) {
        state_ = State::Dead; // 死亡状態に固定
        return;
    }

    attackRequest_ = false;   // 毎フレーム攻撃フラグ初期化
    cardUseRequest_ = false;  // 毎フレームスキル使用フラグ初期化

    if (attackCooldownTimer_ > 0) {
        attackCooldownTimer_--; // 近接攻撃クールダウン減少
    }

    if (skillCooldownTimer_ > 0) {
        skillCooldownTimer_--; // スキルクールダウン減少
    }

    if (isHit_) {
        pos_ += knockbackVelocity_;      // ノックバック移動
        knockbackVelocity_ *= 0.85f;     // 徐々に減速

        hitTimer_--;                     // ヒット時間減少
        if (hitTimer_ <= 0) {
            isHit_ = false;              // ヒット演出終了
            knockbackVelocity_ = { 0.0f, 0.0f, 0.0f }; // ノックバック停止
        }
        return; // ヒット中は他の行動をしない
    }

    if (isActionLocked_) {
        actionLockTimer_--; // ロック残り時間減少
        if (actionLockTimer_ <= 0) {
            isActionLocked_ = false; // ロック解除
        }
        return; // ロック中は行動しない
    }

    if (thinkTimer_ > 0) {
        thinkTimer_--; // 次の判断まで待つ
    }

    if (thinkTimer_ <= 0) {
        DecideNextState(); // 次の状態を決定
    }

    switch (state_) {
    case State::Appear:
        UpdateAppear();
        break;

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

    // デバフタイマーの更新
    if (isAttackDebuffed_) {
        attackDebuffTimer_--;
        if (attackDebuffTimer_ <= 0) {
            isAttackDebuffed_ = false; // 時間切れで元に戻る
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
        attackEnter = 3.5f;
        skillEnter = 10.0f;
    }

    if (dist <= attackEnter) {
        state_ = State::Attack; // 攻撃範囲なら近接攻撃へ
        thinkTimer_ = 0;        // すぐ再判断できるようにする
        return;
    }

    if (dist <= skillEnter && skillCooldownTimer_ <= 0 && !heldCards_.empty()) {
        state_ = State::UseSkill; // スキル範囲ならスキルへ
        thinkTimer_ = 0;          // すぐ再判断できるようにする
        return;
    }

    if (dist > 0.01f) {
        dir = Normalize(dir);
        pos_ += dir * chaseSpeed_;           // プレイヤーへ移動
        rot_.y = std::atan2f(dir.x, dir.z);  // プレイヤー方向を向く
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
    Vector3 dir = {
        playerPos_.x - pos_.x,
        0.0f,
        playerPos_.z - pos_.z
    };

    float dist = Length(dir); // プレイヤーとの距離
    bool isPhase2 = (hp_ <= maxHP_ / 2); // HP半分以下で後半戦

    float skillExit = skillExitRange_;
    if (isPhase2) {
        skillExit = 12.0f;
    }

    if (dist > skillExit) {
        state_ = State::Chase; // 離れたら追跡へ戻る
        thinkTimer_ = 0;       // すぐ再判断できるようにする
        return;
    }

    if (dist > 0.01f) {
        rot_.y = std::atan2f(dir.x, dir.z); // プレイヤー方向を向く
    }

    if (skillCooldownTimer_ > 0 || heldCards_.empty()) {
        state_ = State::Chase; // 使えないなら追跡へ戻る
        thinkTimer_ = 0;       // すぐ再判断できるようにする
        return;
    }

    static std::random_device rd;
    static std::mt19937 mt(rd());
    std::uniform_int_distribution<int> distCard(0, static_cast<int>(heldCards_.size()) - 1);

    selectedCard_ = heldCards_[distCard(mt)]; // 使用カードをランダム選択
    cardUseRequest_ = true;                   // スキル使用要求を出す
    skillCooldownTimer_ = skillCooldown_;     // スキルクールダウン開始

    SetActionLock(35); // スキル使用後の硬直
    thinkTimer_ = 35;  // スキル後すぐ再判断しない
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