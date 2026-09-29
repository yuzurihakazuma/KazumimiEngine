#pragma once
// =====================================================================
//  SceneEditContext：Game View 上の編集ツール（敵・コイン・ブロックのつかみ移動、
//   ブロックの塗り、右クリックメニュー、ガイドハンドル、ミニマップ）が毎フレーム受け取るもの。
//   どれも所有しない（シーンが持っている物への参照）。
//
//   ツールごとに同じ処理（ワールド→Game View の画面座標、マウスに一番近いレール上の点、
//   マウスの近くの敵）を書き写していたので、ここへまとめた
// =====================================================================
#include "engine/math/struct.h"
#include "engine/utils/EditorManager.h" // GameViewMouse

#include <vector>

class SplineRail;
class LevelEditor;
class RailEditor;
class EnemyEditor;
class BlockSystem;
class CoinSystem;
class RailField;
class Camera;
class Player;
struct EnemySpawnData;

struct SceneEditContext {
    EditorManager*                      editor      = nullptr;
    const EditorManager::GameViewMouse* gameView    = nullptr;
    const std::vector<SplineRail>*      rails       = nullptr;
    const RailField*                    railField   = nullptr;
    LevelEditor*                        levelEditor = nullptr; // マップ編集が無い時は nullptr
    RailEditor*                         railEditor  = nullptr; // 同上
    EnemyEditor*                        enemyEditor = nullptr;
    BlockSystem*                        blockSystem = nullptr;
    CoinSystem*                         coinSystem  = nullptr;
    Camera*                             camera      = nullptr;
    const Player*                       player      = nullptr;
    bool                                editing     = false;   // Edit モード中か

    const std::vector<SplineRail>& Rails() const { return *rails; }
    const EditorManager::GameViewMouse& View() const { return *gameView; }

    // ワールド → Game View の画面座標（カメラの後ろなら false）
    bool ProjectToScreen(const Vector3& world, Vector2& out) const;
    // ワールドの点とマウスの画面距離(px)。カメラの後ろなら非常に大きい値
    float ScreenDistanceToMouse(const Vector3& world) const;

    // レール上の点（レール番号・距離・レール線からの高さ）のワールド位置。レールが無効なら false
    bool RailPoint(int railIndex, float distance, float height, Vector3& out) const;

    // マウスに一番近いレール上の点を探す（0.5m刻み。見えない骨組みのレールは除く）。
    //   height      : レール線からこの高さの点で比べる（高い段・浮いている物を狙いやすく）
    //   maxPx       : これより遠ければ見つからない扱い
    //   stickRail   : このレールには stickBonus(px) ぶん吸い付く（隣のレールへ飛び移りにくく）
    //   allowHidden : 見えないレールでもこの番号だけは候補にする（-1=なし）
    struct RailPick { int rail = -1; float dist = 0.0f; };
    bool PickRailPoint(float height, float maxPx, int stickRail, float stickBonus,
                       RailPick& out, int allowHidden = -1) const;

    // 敵の見えている体の中心（浮いている敵は浮いた高さで）
    bool EnemyWorldPos(const EnemySpawnData& spawnData, Vector3& out) const;
    // マウスから maxPx 以内で一番近い敵の番号（-1=なし）
    int  FindEnemyNearMouse(float maxPx) const;

    // エディタのコイン配置から CoinSystem を作り直す（コインを変えた後に呼ぶ）
    void ResyncCoins() const;
};
