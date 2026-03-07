#pragma once
#include "engine/math/VectorMath.h"

class Player {
public:
    void Initialize();
    void Update();

    const Vector3& GetPosition() const { return pos_; }      // 位置取得
    const Vector3& GetScale() const { return scale_; }       // スケール取得
    const Vector3& GetRotation() const { return rot_; }      // 回転取得

    void SetPosition(const Vector3& pos) { pos_ = pos; }     // 位置設定
    void SetScale(const Vector3& scale) { scale_ = scale; }  // スケール設定
    void SetRotation(const Vector3& rot) { rot_ = rot; }     // 回転設定

    void LockAction(int frame); // 指定フレームの間プレイヤー操作をロック

    int GetLevel() const { return level_; }          // レベル取得
    int GetExp() const { return exp_; }              // 経験値取得
    int GetNextLevelExp() const { return nextLevelExp_; } // 次レベル必要経験値
    int GetCost() const { return cost_; }            // 現在コスト取得
    int GetMaxCost() const { return maxCost_; }      // 最大コスト取得

    void AddExp(int value);                          // 経験値加算
    bool CanUseCost(int value) const;                // コスト使用可能判定
    void UseCost(int value);                         // コスト消費

private:
    void LevelUp();          // レベルアップ処理
    void UpdateCost();       // コスト自然回復

private:
    Vector3 pos_{ 0.0f, 0.0f, 0.0f };             // 位置
    Vector3 rot_{ 0.0f, 0.0f, 0.0f };             // 回転
    Vector3 scale_{ 1.0f, 1.0f, 1.0f };           // スケール

    float moveSpeed_ = 0.2f;                      // 通常移動速度

    bool isDodging_ = false;                      // 回避中フラグ
    int dodgeTimer_ = 0;                          // 回避残り時間
    const int dodgeDuration_ = 15;                // 回避継続フレーム
    float dodgeSpeed_ = 0.6f;                     // 回避速度
    Vector3 dodgeDirection_{ 0.0f, 0.0f, 0.0f }; // 回避方向

    bool isActionLocked_ = false;                 // 行動ロック中フラグ
    int actionLockTimer_ = 0;                     // ロック残りフレーム

    int level_ = 1;                               // 現在レベル
    int exp_ = 0;                                 // 現在経験値
    int nextLevelExp_ = 3;                        // 次レベルまでの必要経験値

    int cost_ = 3;                                // 現在コスト
    int maxCost_ = 3;                             // 最大コスト

    int costRecoveryTimer_ = 0;                   // コスト回復タイマー
    int costRecoveryInterval_ = 180;              // 何フレームごとに1回復するか
};