#include "game/enemy/editor/EnemyEditorInspector.h"
#include "game/enemy/editor/EnemyEditorWidgets.h"
#include "game/enemy/editor/EnemySpawnModel.h"
#include "game/enemy/EnemyEditor.h" // DisplayName / GetTypeLabel
#include "engine/rail/SplineRail.h"
#include <algorithm>
#include <cstdio>

#ifdef USE_IMGUI
namespace {
    // 種類ごとの既定値（UIに「既定＝いくつ」と見せるため）
    float DefaultSpeedOf(EnemyType type){ return Enemy::TypeSpecOf(type).speed; }
    float DefaultHoverOf(EnemyType type){ return Enemy::TypeSpecOf(type).hover; }
}

// ----------------------------------------------------------------
//  下段：選択中の1体の設定
// ----------------------------------------------------------------
void EnemyEditorInspector::Draw(EnemySpawnModel& model, const std::vector<SplineRail>& splineRails,
                                int pickRail, float pickDist, bool hasPick){
    ImGui::BeginChild("##enemyInspector", ImVec2(0.0f, 0.0f));
    if ( model.IsValidIndex(model.SelectedEntry()) ) {
        DrawSelected(model, splineRails, pickRail, pickDist, hasPick);
    } else {
        DrawNothingSelected(model);
    }
    ImGui::EndChild();
}

// 何も選ばれていない時：案内と全削除
void EnemyEditorInspector::DrawNothingSelected(EnemySpawnModel& model){
    ImGui::SeparatorText("敵の設定");
    ImGui::TextDisabled("一覧か Game View で敵を選ぶと、ここで1体ずつ動きを決められます");
    // --- 全削除（誤爆防止の2段階式：1回目で確認表示 → 3秒以内にもう一度で実行）---
    if ( model.Count() >= 2 ) {
        ImGui::Spacing();
        bool armed = ( ImGui::GetTime() - clearArmedTime_ ) < 3.0;
        ImGui::PushStyleColor(ImGuiCol_Button, armed ? ImVec4(0.85f, 0.2f, 0.2f, 1.0f) : ImVec4(0.5f, 0.12f, 0.12f, 1.0f));
        if ( ImGui::Button(armed ? "本当に全削除する（もう一度クリック）##clear" : "敵を全削除##clear", ImVec2(-1.0f, 0.0f)) ) {
            if ( armed ) {
                model.Clear();
                clearArmedTime_ = -100.0;
            } else {
                clearArmedTime_ = ImGui::GetTime();
            }
        }
        ImGui::PopStyleColor();
    }
    ImGui::TextDisabled("※配置はマップと一緒に保存される（自動保存 / 上書き保存 / Ctrl+S）");
}

void EnemyEditorInspector::DrawSelected(EnemySpawnModel& model, const std::vector<SplineRail>& splineRails,
                                        int pickRail, float pickDist, bool hasPick){
    const int index = model.SelectedEntry();
    EnemySpawnData& spawn = model.MutableDatas()[index];
    ImGui::PushID(index);

    char title[128];
    std::snprintf(title, sizeof(title), "#%02d %s の設定", index, EnemyEditor::DisplayName(spawn).c_str());
    ImGui::SeparatorText(title);

    if ( ImGui::Button("カメラをここへ") ) { model.RequestFocus(index); }
    ImGui::SameLine();
    if ( ImGui::Button("選択を外す") ) {
        model.ClearSelection();
        ImGui::PopID();
        return;
    }

    DrawBasicSection(model, spawn);

    // レールの長さは「場所」を描く前の値で以降の欄も揃える（この描画中にレールを選び直しても同じ）
    const bool validRail = ( spawn.railIndex >= 0 && spawn.railIndex < ( int ) splineRails.size() );
    const float railLength = validRail ? splineRails[spawn.railIndex].GetLength() : 100.0f;
    if ( !validRail ) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "レール %d がありません（下で選び直してください）", spawn.railIndex);
    }

    DrawPlaceSection(model, index, splineRails, railLength, validRail, pickRail, pickDist, hasPick);
    DrawMotionSection(model, spawn, railLength);
    DrawBodySection(model, spawn);
    DrawTypeSection(model, spawn);
    DrawCopyMotionSection(model, index);

    ImGui::Spacing();
    ImGui::Separator();

    const EntryAction action = DrawEntryButtons();

    ImGui::PopID();

    // 配列を書き換える操作は、spawn の参照を使い終わってから行う
    if ( action == EntryAction::Duplicate ) {
        model.DuplicateEntry(index, splineRails);
    } else if ( action == EntryAction::Delete ) {
        model.RemoveEntry(index);
    }
}

// ================= 基本 =================
void EnemyEditorInspector::DrawBasicSection(EnemySpawnModel& model, EnemySpawnData& spawn){
    char nameBuffer[64] = {};
    std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", spawn.name.c_str());
    if ( ImGui::InputTextWithHint("名前", EnemyEditor::GetTypeLabel(spawn.type), nameBuffer, sizeof(nameBuffer)) ) {
        spawn.name = nameBuffer;
        model.MarkChanged();
    }
    if ( EnemyEditorWidgets::DrawTypeCombo("種類", &spawn.type) ) {
        model.MarkChanged();
    }
}

// ================= 場所 =================
void EnemyEditorInspector::DrawPlaceSection(EnemySpawnModel& model, int index, const std::vector<SplineRail>& splineRails,
                                            float railLength, bool validRail, int pickRail, float pickDist, bool hasPick){
    const EnemySpawnData& spawn = model.Datas()[index];
    ImGui::SeparatorText("場所");
    if ( !splineRails.empty() ) {
        int railIndex = spawn.railIndex;
        if ( EnemyEditorWidgets::DrawRailCombo("レール", &railIndex, splineRails, model.Datas()) ) {
            // 別のレールへ：距離は同じ値のまま（新しいレールの長さに収める）。行動範囲も一緒に動く
            model.MoveEntry(index, railIndex, spawn.distance, splineRails);
            model.MarkChanged();
        }
    }
    {
        float distance = spawn.distance;
        if ( EnemyEditorWidgets::DrawDistanceWidget("edit", &distance, railLength) && validRail ) {
            model.MoveEntry(index, spawn.railIndex, distance, splineRails);
            model.MarkChanged();
        }
    }
    ImGui::BeginDisabled(!hasPick);
    if ( ImGui::Button("選択ノードの位置へ移動", ImVec2(-1.0f, 0.0f)) ) {
        model.MoveEntry(index, pickRail, pickDist, splineRails);
        model.MarkChanged();
    }
    ImGui::EndDisabled();
}

// ================= 動き =================
void EnemyEditorInspector::DrawMotionSection(EnemySpawnModel& model, EnemySpawnData& spawn, float railLength){
    ImGui::SeparatorText("動き");
    int moveMode = spawn.patrol ? 1 : 0;
    if ( ImGui::RadioButton("その場にいる", &moveMode, 0) ) { spawn.patrol = false; model.MarkChanged(); }
    ImGui::SameLine();
    if ( ImGui::RadioButton("巡回する（往復）", &moveMode, 1) ) { spawn.patrol = true; model.MarkChanged(); }

    // プレイヤーを追いかける
    bool chase = ( spawn.chaseRange > 0.0f );
    if ( ImGui::Checkbox("プレイヤーが近づくと追いかける", &chase) ) {
        spawn.chaseRange = chase ? 5.0f : 0.0f;
        model.MarkChanged();
    }
    if ( chase ) {
        ImGui::Indent(16.0f);
        ImGui::SetNextItemWidth(110.0f);
        if ( ImGui::DragFloat("気づく距離(m)", &spawn.chaseRange, 0.1f, 0.5f, 30.0f, "%.1f") ) {
            spawn.chaseRange = std::clamp(spawn.chaseRange, 0.5f, 30.0f);
            model.MarkChanged();
        }
        ImGui::SetNextItemWidth(110.0f);
        if ( ImGui::DragFloat("追う時の速さ(倍)", &spawn.chaseSpeedMul, 0.05f, 0.5f, 4.0f, "%.2f") ) {
            spawn.chaseSpeedMul = std::clamp(spawn.chaseSpeedMul, 0.5f, 4.0f);
            model.MarkChanged();
        }
        if ( !spawn.patrol ) { ImGui::TextDisabled("見失うと置いた場所へ歩いて戻ります"); }
        ImGui::Unindent(16.0f);
    }

    const bool moves = ( spawn.patrol || chase );
    ImGui::BeginDisabled(!moves);
    // 速度
    if ( EnemyEditorWidgets::DrawOptionalFloat("速度(m/s)", &spawn.speed, DefaultSpeedOf(spawn.type), 0.0f,
                                               0.05f, 0.1f, 15.0f, "%.2f") ) {
        model.MarkChanged();
    }
    // 最初の向き
    int startDir = ( spawn.startDir < 0 ) ? 1 : 0;
    ImGui::TextUnformatted("最初に進む向き"); ImGui::SameLine();
    if ( ImGui::RadioButton("終点側へ", &startDir, 0) ) { spawn.startDir = 1;  model.MarkChanged(); }
    ImGui::SameLine();
    if ( ImGui::RadioButton("始点側へ", &startDir, 1) ) { spawn.startDir = -1; model.MarkChanged(); }
    ImGui::EndDisabled();

    // 折り返しの間
    ImGui::BeginDisabled(!spawn.patrol);
    ImGui::SetNextItemWidth(110.0f);
    if ( ImGui::DragFloat("折り返しで止まる(秒)", &spawn.turnWait, 0.05f, 0.0f, 10.0f, "%.2f") ) {
        spawn.turnWait = std::clamp(spawn.turnWait, 0.0f, 10.0f);
        model.MarkChanged();
    }
    ImGui::EndDisabled();

    // 行動範囲（巡回・追跡の両方に効く。-1=レール全体）
    ImGui::BeginDisabled(!moves);
    bool ranged = EnemySpawnModel::HasMoveRange(spawn);
    if ( ImGui::Checkbox("動ける範囲を決める（OFF=レール全体）", &ranged) ) {
        if ( ranged ) {
            // 初期値は「今の位置±3m」（Game View に橙線で範囲が出る）
            spawn.patrolMin = std::clamp(spawn.distance - 3.0f, 0.0f, railLength);
            spawn.patrolMax = std::clamp(spawn.distance + 3.0f, spawn.patrolMin, railLength);
        } else {
            spawn.patrolMin = spawn.patrolMax = -1.0f;
        }
        model.MarkChanged();
    }
    if ( ranged ) {
        if ( ImGui::DragFloatRange2("範囲(m)", &spawn.patrolMin, &spawn.patrolMax,
                0.1f, 0.0f, railLength, "始 %.1f", "終 %.1f") ) {
            spawn.patrolMin = std::clamp(spawn.patrolMin, 0.0f, railLength);
            spawn.patrolMax = std::clamp(spawn.patrolMax, spawn.patrolMin, railLength);
            model.MarkChanged();
        }
        ImGui::TextDisabled("(Game View に橙色の線で範囲が出ます)");
    }
    ImGui::EndDisabled();
}

// ================= 高さ・大きさ =================
void EnemyEditorInspector::DrawBodySection(EnemySpawnModel& model, EnemySpawnData& spawn){
    ImGui::SeparatorText("高さ・大きさ");
    if ( EnemyEditorWidgets::DrawOptionalFloat("浮く高さ(m)", &spawn.hoverHeight, DefaultHoverOf(spawn.type), -0.0001f,
                                               0.05f, 0.0f, 8.0f, "%.2f") ) {
        model.MarkChanged();
    }
    ImGui::SetNextItemWidth(110.0f);
    if ( ImGui::DragFloat("ふわふわの幅(m)", &spawn.bobAmp, 0.01f, 0.0f, 3.0f, "%.2f") ) {
        spawn.bobAmp = std::clamp(spawn.bobAmp, 0.0f, 3.0f);
        model.MarkChanged();
    }
    if ( spawn.bobAmp > 0.0f ) {
        ImGui::SetNextItemWidth(110.0f);
        if ( ImGui::DragFloat("ふわふわの速さ", &spawn.bobSpeed, 0.05f, 0.1f, 12.0f, "%.2f") ) {
            spawn.bobSpeed = std::clamp(spawn.bobSpeed, 0.1f, 12.0f);
            model.MarkChanged();
        }
    }
    ImGui::SetNextItemWidth(110.0f);
    if ( ImGui::DragFloat("大きさ(倍)", &spawn.scale, 0.01f, 0.3f, 4.0f, "%.2f") ) {
        spawn.scale = std::clamp(spawn.scale, 0.3f, 4.0f);
        model.MarkChanged();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(当たり判定も同じ倍率)");
}

// ================= 種類ごとの設定 =================
void EnemyEditorInspector::DrawTypeSection(EnemySpawnModel& model, EnemySpawnData& spawn){
    if ( spawn.type != EnemyType::Strong ) return;
    ImGui::SeparatorText("カミバナ");
    if ( EnemyEditorWidgets::DrawOptionalFloat("噛みつく距離(m)", &spawn.biteRange, 2.6f, -0.0001f,
                                               0.05f, 0.0f, 10.0f, "%.2f") ) {
        model.MarkChanged();
    }
}

// ================= まとめて反映 =================
void EnemyEditorInspector::DrawCopyMotionSection(EnemySpawnModel& model, int index){
    ImGui::SeparatorText("この動きを他の敵にも");
    std::vector<EnemySpawnData>& spawns = model.MutableDatas();
    const EnemySpawnData& spawn = spawns[index];
    // 場所・名前・種類は写さない（動き・高さ・大きさだけ）
    auto copyMotionTo = [&](EnemySpawnData& target){
        float keepMin = target.patrolMin, keepMax = target.patrolMax;
        EnemySpawnData copied = spawn;
        copied.type      = target.type;
        copied.railIndex = target.railIndex;
        copied.distance  = target.distance;
        copied.name      = target.name;
        copied.patrolMin = keepMin; // 行動範囲は場所に紐づくので相手のものを残す
        copied.patrolMax = keepMax;
        target = copied;
    };
    float halfWidth = ( ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x ) * 0.5f;
    if ( ImGui::Button("同じ種類の全員へ", ImVec2(halfWidth, 0.0f)) ) {
        for ( int i = 0; i < ( int ) spawns.size(); ++i ) {
            if ( i != index && spawns[i].type == spawn.type ) { copyMotionTo(spawns[i]); }
        }
        model.MarkChanged();
    }
    ImGui::SameLine();
    if ( ImGui::Button("同じレールの全員へ", ImVec2(halfWidth, 0.0f)) ) {
        for ( int i = 0; i < ( int ) spawns.size(); ++i ) {
            if ( i != index && spawns[i].railIndex == spawn.railIndex ) { copyMotionTo(spawns[i]); }
        }
        model.MarkChanged();
    }
}

// --- 操作ボタン行：複製と削除を横に並べる ---
EnemyEditorInspector::EntryAction EnemyEditorInspector::DrawEntryButtons(){
    EntryAction action = EntryAction::None;
    float halfWidth = ( ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x ) * 0.5f;
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.2f, 0.35f, 0.55f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.45f, 0.7f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.3f, 0.55f, 0.85f, 1.0f));
    if ( ImGui::Button("複製（2m先へ）", ImVec2(halfWidth, 0.0f)) ) { action = EntryAction::Duplicate; }
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.5f, 0.12f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.18f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.9f, 0.22f, 0.22f, 1.0f));
    if ( ImGui::Button("削除", ImVec2(halfWidth, 0.0f)) ) { action = EntryAction::Delete; }
    ImGui::PopStyleColor(3);
    return action;
}
#endif
