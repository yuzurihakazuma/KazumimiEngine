#pragma once
#include "engine/math/VectorMath.h"
#include "game/card/HandManager.h"
#include <vector>

class Boss {
public:
    // ボスの行動状態
    enum class State {
        Idle,      // 待機
        Chase,     // プレイヤー追跡
        Attack,    // 近接攻撃
        UseSkill,  // スキル使用
        Dead       // 死亡
    };

public:
    void Initialize(); // 初期化
    void Update();     // 更新

    // Transform
    const Vector3& GetPosition() const { return pos_; } // 位置取得
    const Vector3& GetRotation() const { return rot_; } // 回転取得
    const Vector3& GetScale() const { return scale_; }  // スケール取得

    void SetPosition(const Vector3& pos) { pos_ = pos; }     // 位置設定
    void SetScale(const Vector3& scale) { scale_ = scale; }  // スケール設定

    // プレイヤー情報
    void SetPlayerPosition(const Vector3& pos) { playerPos_ = pos; } // プレイヤー位置設定

    // 状態
    void SetState(State state) { state_ = state; } // 状態設定
    State GetState() const { return state_; }      // 状態取得

    // HP
    void TakeDamage(int damage);       // ダメージを受ける
    int GetHP() const { return hp_; }  // 現在HP取得
    int GetMaxHP() const { return maxHP_; } // 最大HP取得
    bool IsDead() const { return isDead_; } // 死亡判定

    // 表示
    bool IsHit() const { return isHit_; } // ヒット中か
    bool IsVisible() const;               // 描画するか

    // 行動ロック
    void SetActionLock(int frame);                          // 一定時間行動ロック
    bool IsActionLocked() const { return isActionLocked_; } // 行動ロック中か

    // 通常攻撃リクエスト
    bool GetAttackRequest() const { return attackRequest_; } // 近接攻撃発生取得
    void ClearAttackRequest() { attackRequest_ = false; }    // 近接攻撃発生クリア

    // カード使用リクエスト
    bool GetCardUseRequest() const { return cardUseRequest_; } // スキル使用発生取得
    void ClearCardUseRequest() { cardUseRequest_ = false; }    // スキル使用発生クリア

    // 今回使うカード
    const Card& GetSelectedCard() const { return selectedCard_; } // 選択中カード取得

    // ドロップ用
    bool HasAnyCard() const { return !heldCards_.empty(); } // カードを1枚以上持っているか
    Card GetRandomDropCard() const;                         // ランダムなドロップカード取得

private:
    void DecideNextState(); // 次の状態を決める

    void UpdateIdle();      // 待機処理
    void UpdateChase();     // 追跡処理
    void UpdateAttack();    // 近接攻撃処理
    void UpdateUseSkill();  // スキル使用処理

    void InitializeBossCards(); // ボス専用カード初期化

private:
    Vector3 pos_{ 10.0f, 0.0f, 10.0f }; // 位置
    Vector3 rot_{ 0.0f, 0.0f, 0.0f };   // 回転
    Vector3 scale_{ 2.0f, 2.0f, 2.0f }; // スケール

    Vector3 playerPos_{ 0.0f, 0.0f, 0.0f }; // プレイヤー位置

    State state_ = State::Idle; // 現在の状態

    int hp_ = 20;               // 現在HP
    int maxHP_ = 20;            // 最大HP
    bool isDead_ = false;       // 死亡フラグ

    float chaseSpeed_ = 0.06f;  // 追跡速度

    float chaseRange_ = 20.0f;       // 追跡開始距離
    float attackEnterRange_ = 2.8f;  // 近接攻撃に入る距離
    float attackExitRange_ = 4.0f;   // 近接攻撃をやめる距離

    float skillEnterRange_ = 8.0f;   // スキル使用に入る距離
    float skillExitRange_ = 10.0f;   // スキル使用をやめる距離

    int thinkTimer_ = 0;         // 次の判断まで待つ時間

    bool isActionLocked_ = false; // 行動ロック中
    int actionLockTimer_ = 0;     // ロック残り時間

    bool isHit_ = false;                            // ヒット中フラグ
    int hitTimer_ = 0;                              // ヒット残り時間
    const int hitDuration_ = 10;                    // ヒット演出時間
    Vector3 knockbackVelocity_{ 0.0f, 0.0f, 0.0f }; // ノックバック速度

    bool attackRequest_ = false; // 近接攻撃発生フラグ
    bool cardUseRequest_ = false; // スキル使用発生フラグ

    int attackCooldownTimer_ = 0; // 近接攻撃クールダウン
    int skillCooldownTimer_ = 0;  // スキル使用クールダウン

    const int attackCooldown_ = 45; // 近接攻撃クールダウン時間
    const int skillCooldown_ = 120; // スキル使用クールダウン時間

    std::vector<Card> heldCards_;     // ボス固有カード一覧
    Card selectedCard_{ -1, "", 0 };  // 今回使用するカード
};