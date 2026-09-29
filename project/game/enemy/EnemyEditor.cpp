#include "EnemyEditor.h"
#include "game/enemy/editor/EnemyEditorWidgets.h"
#include "externals/imgui/imgui.h"
#include <string>

EnemyEditor::EnemyEditor() = default;
EnemyEditor::~EnemyEditor() = default;

// 敵タイプ名のヘルパー（コンボボックスや詳細表示用）
const char* EnemyEditor::GetTypeName(EnemyType type) {
    int index = ( int ) type;
    return ( index >= 0 && index < EnemyEditorWidgets::kTypeCount ) ? EnemyEditorWidgets::kTypeNames[index] : "Unknown";
}

// 一覧行に収まるよう短縮した名前
const char* EnemyEditor::GetTypeLabel(EnemyType type) {
    switch ( type ) {
    case EnemyType::Zako:   return "ドングリン";
    case EnemyType::Strong: return "カミバナ";
    case EnemyType::Air:    return "フワリン";
    default:                return "???";
    }
}

std::string EnemyEditor::DisplayName(const EnemySpawnData& spawn){
    return spawn.name.empty() ? std::string(GetTypeLabel(spawn.type)) : spawn.name;
}

void EnemyEditor::Initialize() {
    // 起動時にテスト用の初期エネミー（Zako 1体）を登録しておく
    std::vector<EnemySpawnData>& datas = model_.MutableDatas();
    datas.clear();
    // レール0、距離5.0mの場所に雑魚敵を初期配置する
    EnemySpawnData first;
    first.type = EnemyType::Zako;
    first.railIndex = 0;
    first.distance = 5.0f;
    datas.push_back(first);
    addPanel_.Reset();
    model_.MarkChanged();
    history_.Reset(model_.Datas());
}

void EnemyEditor::SetSpawnDatas(const std::vector<EnemySpawnData>& datas) {
    model_.Replace(datas);
    // マップが変わったので履歴は持ち越さない（前のマップの敵へ戻せてしまうのを防ぐ）
    history_.Reset(model_.Datas());
}

// レールの数や並びが変わった：敵のレール番号を付け替える
void EnemyEditor::ApplyRailRemap(const std::vector<int>& oldToNew, const std::vector<EnemySpawnData>& revived){
    model_.ApplyRailRemap(oldToNew, revived);
    // 履歴の中身も同じように付け替える。控え（lastCommitted_）も同じにするので、この変化は1手に数えない
    history_.ForEachSnapshot([&](std::vector<EnemySpawnData>& snapshot){
        EnemySpawnModel::RemapRails(snapshot, oldToNew, revived);
    });
    list_.RemapRailFilter(oldToNew);
}

// ================================================================
//  元に戻す／やり直し
// ================================================================
void EnemyEditor::TickHistory(){
#ifdef USE_IMGUI
    // ドラッグ中・数値入力中は積まない（1回の操作＝1手にするため、手を離した時にまとめて積む）
    if ( ImGui::IsMouseDown(0) || ImGui::IsAnyItemActive() ) return;
#endif
    history_.Commit(model_.Datas());
}

void EnemyEditor::Undo(){
    if ( history_.Undo(model_.MutableDatas()) ) { model_.OnRestored(); }
}

void EnemyEditor::Redo(){
    if ( history_.Redo(model_.MutableDatas()) ) { model_.OnRestored(); }
}

// ================================================================
//  メインの描画関数
// ================================================================
void EnemyEditor::DrawWindow(const std::vector<SplineRail>& splineRails,
                             int pickRail, float pickDist, bool hasPick) {
#ifdef USE_IMGUI
    model_.DropStaleSelection();

    ImGui::SetNextWindowSize(ImVec2(380.0f, 640.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("敵配置エディタ (Enemy Editor)");

    DrawHistoryRow();
    addPanel_.Draw(model_, splineRails, pickRail, pickDist, hasPick);
    list_.Draw(model_, splineRails);
    inspector_.Draw(model_, splineRails, pickRail, pickDist, hasPick);

    ImGui::End();
#else
    ( void ) splineRails; ( void ) pickRail; ( void ) pickDist; ( void ) hasPick;
#endif
    // 履歴への確定（TickHistory）はシーンが毎フレーム呼ぶ。この窓が閉じていても
    // Game View や配置ビューでの変更を取りこぼさないため、ここでは呼ばない
}

#ifdef USE_IMGUI
// ----------------------------------------------------------------
//  上段：元に戻す／やり直し（敵の配置と設定だけが対象。レールの Ctrl+Z とは別）
// ----------------------------------------------------------------
void EnemyEditor::DrawHistoryRow(){
    ImGui::BeginDisabled(!CanUndo());
    if ( ImGui::Button("元に戻す") ) { Undo(); }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!CanRedo());
    if ( ImGui::Button("やり直し") ) { Redo(); }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("敵 %d 体", model_.Count());
}
#endif
