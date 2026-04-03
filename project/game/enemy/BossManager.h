#pragma once
#include <memory>
#include "engine/math/VectorMath.h"

// 前方宣言
class Boss;
class Obj3d;
class Camera;
class Sprite;
class CardUseSystem;
class LevelEditor;
class Player;
class EnemyManager;
class CardPickupManager;

class BossManager {
public:
    // ボス部屋突入時のカメラ演出状態
    enum class IntroCameraState {
        None,
        PlayerFocus,
        BossFocus,
        ToBattle
    };


public:
    BossManager() = default;
    ~BossManager() = default;

    void Initialize(Camera* camera);
    void Finalize();

    // ボス再配置
    void RespawnInRoom(LevelEditor* levelEditor);

    // 状態リセット
    void Reset();

    // ボス本体の更新・カード雨・召喚・カード使用・死亡処理
    void Update(
        Player* player,
        EnemyManager* enemyManager,
        CardPickupManager* cardPickupManager,
        LevelEditor* levelEditor,
        Camera* camera,
        const Vector3& playerPos,
        const Vector3& targetPos
    );

    // ボス本体・カード演出・HPバーを描画する
    void Draw(LevelEditor* levelEditor);

    // ボスHPバーの描画
    void DrawHpBar(LevelEditor* levelEditor);

    // getter
    Boss* GetBoss() const { return boss_.get(); }
    CardUseSystem* GetBossCardSystem() const { return bossCardSystem_.get(); }

    Obj3d* GetBossObj() const { return bossObj_.get(); }

    Sprite* GetBossHpBackSprite() const { return bossHpBackSprite_.get(); }
    Sprite* GetBossHpFillSprite() const { return bossHpFillSprite_.get(); }

    bool IsBossIntroPlaying() const { return isBossIntroPlaying_; }
    int GetBossIntroTimer() const { return bossIntroTimer_; }
    IntroCameraState GetBossIntroCameraState() const { return bossIntroCameraState_; }

    int GetBossCardRainTimer() const { return bossCardRainTimer_; }
    int GetBossCardRainInterval() const { return bossCardRainInterval_; }
    int GetBossCardRainMax() const { return bossCardRainMax_; }
    bool IsBossCardRainEnabled() const { return isBossCardRainEnabled_; }

    void SetBossIntroPlaying(bool flag) { isBossIntroPlaying_ = flag; }
    void SetBossIntroTimer(int timer) { bossIntroTimer_ = timer; }
    void SetBossIntroCameraState(IntroCameraState state) { bossIntroCameraState_ = state; }

    void SetBossCardRainTimer(int timer) { bossCardRainTimer_ = timer; }
    void SetBossCardRainMax(int max) { bossCardRainMax_ = max; }
    void SetBossCardRainEnabled(bool flag) { isBossCardRainEnabled_ = flag; }

    bool IsBossDeadHandled() const { return bossDeadHandled_; }

    void SetBossDeadHandled(bool flag) { bossDeadHandled_ = flag; }

    void StartBossIntro();
    void EndBossIntro();

    bool ShouldTriggerGameClear(LevelEditor* levelEditor) const;

private:
    std::unique_ptr<Boss> boss_ = nullptr;
    std::unique_ptr<Obj3d> bossObj_ = nullptr;
    std::unique_ptr<CardUseSystem> bossCardSystem_ = nullptr;

    bool bossDeadHandled_ = false;

    // ボスHPバー
    std::unique_ptr<Sprite> bossHpBackSprite_ = nullptr;
    std::unique_ptr<Sprite> bossHpFillSprite_ = nullptr;

    // ボス部屋突入時のカメラ演出管理
    IntroCameraState bossIntroCameraState_ = IntroCameraState::None;
    int bossIntroTimer_ = 0;
    bool isBossIntroPlaying_ = false;

    // ボス部屋のカード降らせ演出
    int bossCardRainTimer_ = 0;
    const int bossCardRainInterval_ = 180;
    int bossCardRainMax_ = 5;
    bool isBossCardRainEnabled_ = true;
};