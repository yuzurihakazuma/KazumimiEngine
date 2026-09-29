#include "game/enemy/editor/EnemyEditorList.h"
#include "game/enemy/editor/EnemyEditorWidgets.h"
#include "game/enemy/editor/EnemySpawnModel.h"
#include "game/enemy/EnemyEditor.h" // DisplayName
#include "engine/rail/SplineRail.h"
#include <algorithm>
#include <cstdio>
#include <string>

void EnemyEditorList::RemapRailFilter(const std::vector<int>& oldToNew){
    if ( filterRail_ >= 0 && filterRail_ < ( int ) oldToNew.size() ) { filterRail_ = oldToNew[filterRail_]; }
}

bool EnemyEditorList::PassesFilter(const EnemySpawnData& spawn) const{
    if ( filterType_ >= 0 && ( int ) spawn.type != filterType_ ) return false;
    if ( filterRail_ >= 0 && spawn.railIndex != filterRail_ ) return false;
    if ( filterText_[0] != '\0' ) {
        std::string shown = EnemyEditor::DisplayName(spawn);
        if ( shown.find(filterText_) == std::string::npos ) return false;
    }
    return true;
}

#ifdef USE_IMGUI
// ----------------------------------------------------------------
//  中段：絞り込み＋一覧（高さ固定のスクロール領域）
// ----------------------------------------------------------------
void EnemyEditorList::Draw(EnemySpawnModel& model, const std::vector<SplineRail>& splineRails){
    hoveredEntry_ = -1; // Game View ハイライトは毎フレーム取り直す
    const std::vector<EnemySpawnData>& spawns = model.Datas();

    ImGui::SeparatorText("配置済みの敵");
    DrawFilter(spawns, splineRails);

    // --- 一覧（敵が増えてもウィンドウが伸びない。下の設定欄は常に同じ場所に出る）---
    float listHeight = std::clamp(ImGui::GetContentRegionAvail().y * 0.38f, 130.0f, 320.0f);
    ImGui::BeginChild("##enemyList", ImVec2(0.0f, listHeight), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY);
    int shownCount = 0;
    if ( groupByRail_ ) {
        // レール番号の小さい順に、敵のいるレールだけ見出しを出す
        int maxRail = -1;
        for ( const auto& spawn : spawns ) { maxRail = ( std::max )( maxRail, spawn.railIndex ); }
        for ( int railIndex = 0; railIndex <= maxRail; ++railIndex ) {
            int railCount = 0;
            bool containsSelected = false;
            for ( int i = 0; i < ( int ) spawns.size(); ++i ) {
                if ( spawns[i].railIndex != railIndex || !PassesFilter(spawns[i]) ) continue;
                ++railCount;
                if ( i == model.SelectedEntry() ) { containsSelected = true; }
            }
            if ( railCount == 0 ) continue;
            // 外から選択された敵がいる見出しは自動で開く
            if ( containsSelected && model.IsScrollToSelectedPending() ) { ImGui::SetNextItemOpen(true); }
            char header[64];
            std::snprintf(header, sizeof(header), "Rail %d  (%d体)###rail%d", railIndex, railCount, railIndex);
            if ( ImGui::TreeNodeEx(header, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth) ) {
                for ( int i = 0; i < ( int ) spawns.size(); ++i ) {
                    if ( spawns[i].railIndex != railIndex || !PassesFilter(spawns[i]) ) continue;
                    DrawRow(model, i);
                    ++shownCount;
                }
                ImGui::TreePop();
            } else {
                shownCount += railCount;
            }
        }
    } else {
        for ( int i = 0; i < ( int ) spawns.size(); ++i ) {
            if ( !PassesFilter(spawns[i]) ) continue;
            DrawRow(model, i);
            ++shownCount;
        }
    }
    if ( spawns.empty() ) {
        ImGui::TextDisabled("敵が配置されていません");
    } else if ( shownCount == 0 ) {
        ImGui::TextDisabled("絞り込みに合う敵がいません");
    }
    model.ClearScrollToSelected(); // 絞り込みで行が出ていない時に持ち越さない
    ImGui::EndChild();
    ImGui::TextDisabled("クリック=選択 / ダブルクリック=カメラをそこへ / Game View でつかんで移動");
}

// ----------------------------------------------------------------
//  絞り込み（種類・レール・名前・レール別まとめ）
// ----------------------------------------------------------------
void EnemyEditorList::DrawFilter(const std::vector<EnemySpawnData>& spawns, const std::vector<SplineRail>& splineRails){
    const char* typeFilterNames[] = { "全種類", "ドングリン", "カミバナ", "フワリン" };
    int typeFilterIndex = filterType_ + 1;
    ImGui::SetNextItemWidth(100.0f);
    if ( ImGui::Combo("##filterType", &typeFilterIndex, typeFilterNames, IM_ARRAYSIZE(typeFilterNames)) ) {
        filterType_ = typeFilterIndex - 1;
    }
    ImGui::SameLine();
    char railPreview[32];
    if ( filterRail_ >= 0 ) { std::snprintf(railPreview, sizeof(railPreview), "Rail %d", filterRail_); }
    else                    { std::snprintf(railPreview, sizeof(railPreview), "全レール"); }
    ImGui::SetNextItemWidth(100.0f);
    if ( ImGui::BeginCombo("##filterRail", railPreview) ) {
        if ( ImGui::Selectable("全レール", filterRail_ < 0) ) { filterRail_ = -1; }
        for ( int railIndex = 0; railIndex < ( int ) splineRails.size(); ++railIndex ) {
            int count = 0;
            for ( const auto& spawn : spawns ) { if ( spawn.railIndex == railIndex ) ++count; }
            if ( count == 0 ) continue; // 敵のいないレールは候補に出さない
            char itemLabel[48];
            std::snprintf(itemLabel, sizeof(itemLabel), "Rail %d  (%d体)", railIndex, count);
            if ( ImGui::Selectable(itemLabel, filterRail_ == railIndex) ) { filterRail_ = railIndex; }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##filterText", "名前で探す", filterText_, sizeof(filterText_));
    ImGui::Checkbox("レール別にまとめる", &groupByRail_);
}

// ----------------------------------------------------------------
//  一覧の1行
// ----------------------------------------------------------------
void EnemyEditorList::DrawRow(EnemySpawnModel& model, int index){
    const EnemySpawnData& spawn = model.Datas()[index];
    ImGui::PushID(index);

    // 行の頭に種類の色●（Game View のピンと同じ色）
    ImGui::TextColored(EnemyEditorWidgets::TypeColorOf(spawn.type), "●");
    ImGui::SameLine();

    // 動きの設定が一目で分かる印
    std::string tags;
    if ( spawn.patrol )            { tags += " [巡回]"; }
    if ( spawn.chaseRange > 0.0f ) { tags += " [追跡]"; }
    if ( spawn.speed > 0.0f )      { tags += " [速度]"; }
    char label[192];
    if ( groupByRail_ ) {
        std::snprintf(label, sizeof(label), "#%02d %s  %.1fm%s",
            index, EnemyEditor::DisplayName(spawn).c_str(), spawn.distance, tags.c_str());
    } else {
        std::snprintf(label, sizeof(label), "#%02d %s  Rail %d  %.1fm%s",
            index, EnemyEditor::DisplayName(spawn).c_str(), spawn.railIndex, spawn.distance, tags.c_str());
    }

    bool isSelected = ( model.SelectedEntry() == index );
    if ( ImGui::Selectable(label, isSelected, ImGuiSelectableFlags_AllowDoubleClick) ) {
        model.Select(index);
        if ( ImGui::IsMouseDoubleClicked(0) ) { model.RequestFocus(index); } // ダブルクリック＝カメラをそこへ
    }
    // ホバー中の敵は Game View で黄色くハイライト（どの行がどの敵か一目で分かる）
    if ( ImGui::IsItemHovered() ) { hoveredEntry_ = index; }
    // Game View でつかんだ敵など、外から選択された行まで一覧を送る
    if ( isSelected && model.IsScrollToSelectedPending() ) {
        ImGui::SetScrollHereY(0.5f);
        model.ClearScrollToSelected();
    }
    ImGui::PopID();
}
#endif
