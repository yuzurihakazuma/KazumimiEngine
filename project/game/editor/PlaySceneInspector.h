#pragma once
// =====================================================================
//  PlaySceneInspector：ゲームプレイシーンが共有の「インスペクター (詳細設定)」へ足す項目。
//   ・レール表示・カメラ視点（緑線・プレイ中カメラ・道の設定・視点プリセット）
//   ・当たり判定のふるまい（敵に横からぶつかると弾かれるか）
//   ・「デバッグ描画」の中の、当たり判定の形の表示
//  エンジン側の項目・ウィンドウの開閉・グリッドは BaseScene が持つ（全シーン共通）。
//  値そのものはシーンの各システムが持つ。ここは触る窓口だけ
// =====================================================================
class Camera;
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
        RailField*            railField = nullptr;
        RoadMesh*             roadMesh = nullptr;
        DissolveRoad*         dissolveRoad = nullptr;
        PlayCameraController* cameraController = nullptr;
        CoinSystem*           coinSystem = nullptr;
        BlockSystem*          blockSystem = nullptr;
        CombatSystem*         combat = nullptr;
        PlayerAvatar*         avatar = nullptr;
    };

    // インスペクターのウィンドウの中で呼ぶ（BaseScene::OnDrawInspector から）
    static void Draw(const Targets& targets);
    // 「デバッグ描画」の中に出す「当たり判定を表示」のチェック
    static void DrawHitShapeToggle(bool* showHitShapes);

private:
    static void DrawRailAndCamera(const Targets& targets);
    static void DrawRoadSettings(const Targets& targets);
    static void DrawCollisionSettings(const Targets& targets);
};
