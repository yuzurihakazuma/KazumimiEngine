#include "game/enemy/editor/EnemyEditorAddPanel.h"
#include "game/enemy/editor/EnemyEditorWidgets.h"
#include "game/enemy/editor/EnemySpawnModel.h"
#include "engine/rail/SplineRail.h"
#include <algorithm>

void EnemyEditorAddPanel::Reset(){
    // 新規配置のひな形。距離0だとレール始点＝プレイヤー開始位置に重なるので離しておく
    newTemplate_ = EnemySpawnData {};
    newTemplate_.distance = 5.0f;
}

#ifdef USE_IMGUI
// ----------------------------------------------------------------
//  上段：新しく置く
// ----------------------------------------------------------------
void EnemyEditorAddPanel::Draw(EnemySpawnModel& model, const std::vector<SplineRail>& splineRails,
                               int pickRail, float pickDist, bool hasPick){
    if ( !ImGui::CollapsingHeader("新しく置く", model.Datas().empty() ? ImGuiTreeNodeFlags_DefaultOpen : 0) ) return;

    if ( splineRails.empty() ) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f),
            "レールがありません。先にレールを作成してください。");
        return;
    }

    // 置いた敵は選択状態にする＝そのまま下の設定欄で動きを決められる
    auto addEnemy = [&](int railIndex, float distance){
        EnemySpawnData spawn = newTemplate_;
        spawn.railIndex = railIndex;
        // レール端1mには置かない：距離0のままだとレール始点＝プレイヤーのスタート地点に
        // 敵が重なってしまう事故が起きる（実際に起きた）ため
        float railLength = splineRails[railIndex].GetLength();
        spawn.distance  = std::clamp(distance, 1.0f, ( std::max )( 1.0f, railLength - 1.0f ));
        spawn.patrolMin = spawn.patrolMax = -1.0f; // 行動範囲は置いた後に決める
        spawn.name.clear();
        model.AddEntry(spawn); // 追加＋選択＋一覧をその行まで送る＋変更通知
    };

    EnemyEditorWidgets::DrawTypeCombo("種類##new", &newTemplate_.type);
    EnemyEditorWidgets::DrawRailCombo("レール##new", &newTemplate_.railIndex, splineRails, model.Datas());
    float maxDist = splineRails[newTemplate_.railIndex].GetLength();
    EnemyEditorWidgets::DrawDistanceWidget("new", &newTemplate_.distance, maxDist);
    ImGui::Checkbox("巡回する（レールを往復）##new", &newTemplate_.patrol);

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.15f, 0.5f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.65f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.25f, 0.8f, 0.25f, 1.0f));
    if ( ImGui::Button("＋ 敵を追加", ImVec2(-1.0f, 28.0f)) ) {
        addEnemy(newTemplate_.railIndex, newTemplate_.distance);
    }
    ImGui::PopStyleColor(3);

    // --- 選択ノードの位置に追加（Game View でクリックした場所へ数値入力なしで置ける）---
    ImGui::BeginDisabled(!hasPick);
    if ( ImGui::Button("＋ 選択ノードの位置に追加", ImVec2(-1.0f, 0.0f)) ) {
        addEnemy(pickRail, pickDist);
    }
    ImGui::EndDisabled();
    if ( hasPick ) {
        ImGui::TextDisabled("配置先: Rail %d の %.1fm 地点（選択中ノード）", pickRail, pickDist);
    }

    // --- 等間隔で並べる（選んだレールにN体を均等配置）---
    ImGui::SetNextItemWidth(80.0f);
    ImGui::DragInt("体##multi", &multiCount_, 1, 1, 30);
    multiCount_ = std::clamp(multiCount_, 1, 30);
    ImGui::SameLine();
    if ( ImGui::Button("選んだレールに等間隔で並べる") ) {
        float len = splineRails[newTemplate_.railIndex].GetLength();
        for ( int k = 0; k < multiCount_; ++k ) {
            // 両端1割は空ける（端ぴったりに敵が立たないように）
            float t = ( multiCount_ > 1 ) ? ( float ) k / ( float ) ( multiCount_ - 1 ) : 0.5f;
            addEnemy(newTemplate_.railIndex, len * ( 0.1f + 0.8f * t ));
        }
    }
    ImGui::TextDisabled("Game View の右クリックメニューからも置けます");
}
#endif
