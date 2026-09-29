#pragma once
// =====================================================================
//  PlaySceneInspector：ゲームプレイシーンが共有の「インスペクター (詳細設定)」へ足す項目。
//   ・レール表示・カメラ視点（緑線・プレイ中カメラ・道の設定・視点プリセット）
//   ・デバッグ描画（グリッド・当たり判定の形の表示）
//   ・当たり判定のふるまい（敵に横からぶつかると弾かれるか）
//  値そのものはシーンの各システムが持つ。ここは触る窓口だけ
// =====================================================================
class Camera;
class DebugCamera;
class RailField;
class RoadMesh;
class DissolveRoad;
class PlayCameraController;
class CoinSystem;
class BlockSystem;
class CombatSystem;
class PlayerAvatar;

class PlaySceneInspector {
public:
    // 触る対象（どれもシーンが持つ。所有しない）
    struct Targets {
        Camera*               camera = nullptr;
        DebugCamera*          debugCamera = nullptr;
        RailField*            railField = nullptr;
        RoadMesh*             roadMesh = nullptr;
        DissolveRoad*         dissolveRoad = nullptr;
        PlayCameraController* cameraController = nullptr;
        CoinSystem*           coinSystem = nullptr;
        BlockSystem*          blockSystem = nullptr;
        CombatSystem*         combat = nullptr;
        PlayerAvatar*         avatar = nullptr;
        bool*                 showDebugGrid = nullptr;
        bool*                 showHitShapes = nullptr;
    };

    // インスペクターのパネルが表示中の時だけ呼ぶ
    static void Draw(const Targets& targets);

private:
    static void DrawRailAndCamera(const Targets& targets);
    static void DrawRoadSettings(const Targets& targets);
    static void DrawDebugDrawSettings(const Targets& targets);
    static void DrawCollisionSettings(const Targets& targets);
};
