#include "game/editor/PlaySceneInspector.h"
#include "game/camera/EditorCameraUtil.h"
#include "game/camera/PlayCameraController.h"
#include "game/combat/CombatSystem.h"
#include "game/player/PlayerAvatar.h"
#include "game/rail/DissolveRoad.h"
#include "game/rail/RailField.h"
#include "game/rail/RoadMesh.h"
#include "game/stage/BlockSystem.h"
#include "game/stage/CoinSystem.h"
#include "engine/camera/Camera.h"
#include "engine/utils/EditorManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void PlaySceneInspector::Draw(const Targets& targets){
#ifdef USE_IMGUI
    DrawRailAndCamera(targets);
    DrawCollisionSettings(targets);
#else
    ( void ) targets;
#endif
}

void PlaySceneInspector::DrawRailAndCamera(const Targets& targets){
#ifdef USE_IMGUI
    if ( !ImGui::CollapsingHeader("レール表示・カメラ視点 (Rail Debug)") ) return;
    RailField& railField = *targets.railField;
    bool showMarkers = railField.ShowMarkers();
    if ( ImGui::Checkbox("レール経路を表示", &showMarkers) ) { railField.SetShowMarkers(showMarkers); }
    targets.cameraController->DrawDebugUI(); // プレイ中カメラ（プレイヤー追従＋カメラ演出ゾーン）
    ImGui::Text("マーカー数: %d", railField.MarkerCount());
    if ( ImGui::Button("マーカー再構築") ) { railField.RebuildMarkers(); }

    DrawRoadSettings(targets);

    // --- カメラ視点プリセット（レールを編集しやすく）---
    ImGui::Separator();
    ImGui::TextDisabled("カメラ視点プリセット:");
    if ( ImGui::Button("トップビュー（真上から）") ) { EditorCameraUtil::TopView(*targets.camera, railField.GetRails()); }
    ImGui::SameLine();
    if ( ImGui::Button("斜め視点に戻す") ) { EditorCameraUtil::DefaultAngle(*targets.camera); }
    ImGui::TextDisabled("※デバッグカメラONなら右ドラッグで自由に回せます");
#else
    ( void ) targets;
#endif
}

// 道の設定（表示／両面描画／曲がり角の形／危険帯の長さ／再生成）
void PlaySceneInspector::DrawRoadSettings(const Targets& targets){
#ifdef USE_IMGUI
    RoadMesh& roadMesh = *targets.roadMesh;
    const auto& rails = targets.railField->GetRails();
    ImGui::Separator();
    ImGui::TextDisabled("道の設定:");
    bool roadVisible = roadMesh.IsVisible();
    if ( ImGui::Checkbox("道を表示", &roadVisible) ) { roadMesh.SetVisible(roadVisible); }
    bool cullNone = roadMesh.IsCullNone();
    if ( ImGui::Checkbox("両面描画（OFF=背面カリングで軽量化）", &cullNone) ) {
        roadMesh.SetCullNone(cullNone); // 即時反映（再生成不要）
    }
    int cornerStyle = roadMesh.GetCornerStyle();
    const char* cornerStyleLabels[] = { "自動（角度で判定）", "いつも丸広場（ヨッシー風）", "丸なし（角ばり）" };
    ImGui::SetNextItemWidth(200.0f);
    if ( ImGui::Combo("曲がり角の形", &cornerStyle, cornerStyleLabels, 3) ) {
        roadMesh.SetCornerStyle(cornerStyle);
        roadMesh.Build(rails, targets.camera); // 選んだ瞬間に道を作り直して反映
    }
    if ( ImGui::IsItemHovered() ) ImGui::SetTooltip("レールが曲がって繋がる角の見た目：\n 自動＝鋭い角はマイター、大きく回る角は丸広場\n いつも丸広場＝全部の角に丸い広場を出す\n 丸なし＝丸広場を出さず角ばった接続にする");
    float warnLength = roadMesh.GetWarnLength();
    ImGui::SetNextItemWidth(160.0f);
    if ( ImGui::SliderFloat("危険帯の長さ(m)", &warnLength, 0.5f, 5.0f, "%.1f") ) {
        roadMesh.SetWarnLength(warnLength);
    }
    // スライダーを離した時に道を作り直して反映（ドラッグ中の連続再生成はしない）
    if ( ImGui::IsItemDeactivatedAfterEdit() ) { roadMesh.Build(rails, targets.camera); }
    ImGui::SameLine();
    if ( ImGui::Button("道を再生成") ) {
        EditorManager* editorManager = EditorManager::GetInstance();
        roadMesh.Build(rails, targets.camera);
        targets.dissolveRoad->Build(rails);
        targets.coinSystem->Sync(editorManager->GetEditorCoins(), rails);
        targets.blockSystem->Sync(editorManager->GetEditorBlocks(), &rails);
    }
    ImGui::Text("SDF溶け道: チェーン点 %d / 描画チャンク %d", targets.dissolveRoad->PieceCount(), targets.dissolveRoad->ActiveCount());
    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderFloat("プレイヤーモデル高さ補正(m)", targets.avatar->ModelYOffsetPtr(), -0.6f, 0.6f, "%.2f");
    if ( ImGui::IsItemHovered() ) ImGui::SetTooltip("プレイヤーの足元と道の上面が合うように調整（マイナスで下がる）");
    ImGui::Text("道メッシュ/ピース数: %d", roadMesh.TileCount());
    ImGui::Text("道の頂点数: %d / 三角形: %d", roadMesh.VertexCount(), roadMesh.TriangleCount());
#else
    ( void ) targets;
#endif
}

void PlaySceneInspector::DrawHitShapeToggle(bool* showHitShapes){
#ifdef USE_IMGUI
    ImGui::Checkbox("当たり判定を表示", showHitShapes);
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip("敵（赤）・プレイヤー（緑）・ブロック（水色）・飛んでいる卵（黄）の\n"
            "当たり判定の形を線で表示する。見た目とずれていないかの確認用");
    }
#else
    ( void ) showHitShapes;
#endif
}

void PlaySceneInspector::DrawCollisionSettings(const Targets& targets){
#ifdef USE_IMGUI
    if ( !ImGui::CollapsingHeader("当たり判定 (Collision)") ) return;
    bool contactKnockback = targets.combat->IsContactKnockback();
    if ( ImGui::Checkbox("敵に横からぶつかると弾かれる", &contactKnockback) ) {
        targets.combat->SetContactKnockback(contactKnockback);
    }
    ImGui::TextDisabled("OFF にすると敵をすり抜ける（踏みつけ・卵・舌は OFF でも当たる）");
#else
    ( void ) targets;
#endif
}
