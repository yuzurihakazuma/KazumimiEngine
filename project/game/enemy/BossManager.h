#pragma once
#include <memory>
#include "engine/math/VectorMath.h"

// 前方宣言
class Boss;
class Obj3d;
class Camera;
class Sprite;
class CardUseSystem;
class MapManager;
class Player;
class EnemyManager;
class CardPickupManager;

class BossManager {
public:
    // ボス登場時のカメラ演出状態
    enum class IntroCameraState {
        None,        // 通常
        PlayerFocus, // プレイヤーを見る
        BossFocus,   // ボスを見る
        ToBattle     // 戦闘位置へ移動
    };

public:
    BossManager() = default;
    ~BossManager() = default;

    // ボス・描画・カードシステム生成
    void Initialize(Camera* camera);

    // リソース解放
    void Finalize();

    // ボスを部屋に再配置（通常 / ボス部屋で位置分岐）
    void RespawnInRoom(MapManager* mapManager);

    // 内部状態リセット（演出やフラグ）
    void Reset();

    // ボスの全更新（AI / カード / 召喚 / 演出 / 死亡処理）
    void Update(
        Player* player,
        EnemyManager* enemyManager,
        CardPickupManager* cardPickupManager,
        MapManager* mapManager,
        Camera* camera,
        const Vector3& playerPos,
        const Vector3& targetPos
    );

    // ボス本体 + カード演出 + HPバー描画
    void Draw(MapManager* mapManager);

    // HPバーのみ描画
    void DrawHpBar(MapManager* mapManager);

    // ボス本体取得（状態参照用）
    Boss* GetBoss() const { return boss_.get(); }

    // ボス用カードシステム取得
    CardUseSystem* GetBossCardSystem() const { return bossCardSystem_.get(); }

    // 描画オブジェクト取得
    Obj3d* GetBossObj() const { return bossObj_.get(); }

    // HPバーUI取得
    Sprite* GetBossHpBackSprite() const { return bossHpBackSprite_.get(); }
    Sprite* GetBossHpFillSprite() const { return bossHpFillSprite_.get(); }

    // 登場演出状態
    bool IsBossIntroPlaying() const { return isBossIntroPlaying_; }
    int GetBossIntroTimer() const { return bossIntroTimer_; }
    IntroCameraState GetBossIntroCameraState() const { return bossIntroCameraState_; }

    // カード降らせ演出状態
    int GetBossCardRainTimer() const { return bossCardRainTimer_; }
    int GetBossCardRainInterval() const { return bossCardRainInterval_; }
    int GetBossCardRainMax() const { return bossCardRainMax_; }
    bool IsBossCardRainEnabled() const { return isBossCardRainEnabled_; }

    // デバッグ・外部制御用
    void SetBossIntroPlaying(bool flag) { isBossIntroPlaying_ = flag; }
    void SetBossIntroTimer(int timer) { bossIntroTimer_ = timer; }
    void SetBossIntroCameraState(IntroCameraState state) { bossIntroCameraState_ = state; }

    void SetBossCardRainTimer(int timer) { bossCardRainTimer_ = timer; }
    void SetBossCardRainMax(int max) { bossCardRainMax_ = max; }
    void SetBossCardRainEnabled(bool flag) { isBossCardRainEnabled_ = flag; }

    // 死亡処理が既に実行されたか（多重実行防止）
    bool IsBossDeadHandled() const { return bossDeadHandled_; }
    void SetBossDeadHandled(bool flag) { bossDeadHandled_ = flag; }

    // 登場演出開始 / 終了
    void StartBossIntro();
    void EndBossIntro();

    // ボス撃破時にゲームクリアするか判定
    bool ShouldTriggerGameClear(MapManager* mapManager) const;

private:
    std::unique_ptr<Boss> boss_ = nullptr;              // ボス本体
    std::unique_ptr<Obj3d> bossObj_ = nullptr;          // 描画用
    std::unique_ptr<CardUseSystem> bossCardSystem_ = nullptr; // カード処理

    bool bossDeadHandled_ = false; // 死亡処理フラグ

    // HPバー
    std::unique_ptr<Sprite> bossHpBackSprite_ = nullptr;
    std::unique_ptr<Sprite> bossHpFillSprite_ = nullptr;

    // 登場演出
    IntroCameraState bossIntroCameraState_ = IntroCameraState::None;
    int bossIntroTimer_ = 0;
    bool isBossIntroPlaying_ = false;

    // カード降らせ演出
    int bossCardRainTimer_ = 0;
    const int bossCardRainInterval_ = 180;
    int bossCardRainMax_ = 5;
    bool isBossCardRainEnabled_ = true;
};