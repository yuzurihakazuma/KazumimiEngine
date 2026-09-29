#include "game/enemy/editor/EnemyEditorWidgets.h"
#include "engine/rail/SplineRail.h"
#include <algorithm>
#include <cstdio>

#ifdef USE_IMGUI
namespace EnemyEditorWidgets {

ImVec4 TypeColorOf(EnemyType type){
    switch ( type ) {
    case EnemyType::Zako:   return ImVec4(1.0f, 0.35f, 0.25f, 1.0f); // 赤
    case EnemyType::Strong: return ImVec4(0.75f, 0.4f, 1.0f, 1.0f);  // 紫
    default:                return ImVec4(0.35f, 0.8f, 1.0f, 1.0f);  // 水色
    }
}

bool DrawTypeCombo(const char* label, EnemyType* type){
    int typeIndex = std::clamp(( int ) *type, 0, kTypeCount - 1);
    if ( !ImGui::Combo(label, &typeIndex, kTypeNames, kTypeCount) ) return false;
    *type = ( EnemyType ) typeIndex; // コンボの並び＝enumの並び
    return true;
}

// ================================================================
//  レール選択用ドロップダウンを描画するヘルパー関数
//  レール番号・タイプ・長さ・配置済み敵数を表示して選択できるようにする
// ================================================================
bool DrawRailCombo(const char* label,
                   int* railIndex,
                   const std::vector<SplineRail>& rails,
                   const std::vector<EnemySpawnData>& spawns) {
    if ( rails.empty() ) return false;

    // 各レールに配置済みの敵数を数える
    std::vector<int> countPerRail(rails.size(), 0);
    for ( const auto& s : spawns ) {
        if ( s.railIndex >= 0 && s.railIndex < static_cast<int>(rails.size()) ) {
            countPerRail[s.railIndex]++;
        }
    }

    // 現在選択中のレールのプレビュー文字列を作成
    *railIndex = std::clamp(*railIndex, 0, static_cast<int>(rails.size()) - 1);
    const SplineRail& cur = rails[*railIndex];
    const char* curType = (cur.type == SplineRail::RailType::Horizontal) ? "横" : "縦";
    char preview[128];
    std::snprintf(preview, sizeof(preview), "Rail %d  [%s] %.1fm",
        *railIndex, curType, cur.GetLength());

    bool changed = false;
    if ( ImGui::BeginCombo(label, preview) ) {
        for ( int i = 0; i < static_cast<int>(rails.size()); ++i ) {
            const SplineRail& r = rails[i];
            const char* rType = (r.type == SplineRail::RailType::Horizontal) ? "横" : "縦";

            // ラベル：レール番号 / タイプ / 長さ / ループ有無 / 配置済み数
            char itemLabel[128];
            if ( countPerRail[i] > 0 ) {
                std::snprintf(itemLabel, sizeof(itemLabel),
                    "Rail %d  [%s] %.1fm%s  (%d体)",
                    i, rType, r.GetLength(),
                    r.isLoop ? " Loop" : "",
                    countPerRail[i]);
            } else {
                std::snprintf(itemLabel, sizeof(itemLabel),
                    "Rail %d  [%s] %.1fm%s",
                    i, rType, r.GetLength(),
                    r.isLoop ? " Loop" : "");
            }

            bool selected = (*railIndex == i);
            if ( ImGui::Selectable(itemLabel, selected) ) {
                *railIndex = i;
                changed = true;
            }
            if ( selected ) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

// ================================================================
//  距離入力ウィジェット：DragFloat（ドラッグで調整 + Ctrl+クリックで直接入力）
//  + 始点・中央・終点のクイックボタン
// ================================================================
bool DrawDistanceWidget(const char* idSuffix, float* distance, float maxDist) {
    if ( maxDist < 0.01f ) maxDist = 0.01f;

    bool changed = false;
    char label[32];
    std::snprintf(label, sizeof(label), "距離(m)##%s", idSuffix);

    // DragFloat：ドラッグで滑らかに調整。Ctrl+クリックで数値直接入力も可能
    if ( ImGui::DragFloat(label, distance, 0.1f, 0.0f, maxDist, "%.1f") ) {
        changed = true;
    }
    *distance = std::clamp(*distance, 0.0f, maxDist);

    // パーセント表示（現在位置が全長のどの辺りかひと目で分かる）
    float pct = (maxDist > 0.01f) ? (*distance / maxDist * 100.0f) : 0.0f;
    ImGui::SameLine();
    ImGui::TextDisabled("(%.0f%%)", pct);

    // クイック配置ボタン
    char b0[16], b1[16], b2[16];
    std::snprintf(b0, sizeof(b0), "始点##%s", idSuffix);
    std::snprintf(b1, sizeof(b1), "中央##%s", idSuffix);
    std::snprintf(b2, sizeof(b2), "終点##%s", idSuffix);
    if ( ImGui::Button(b0) )  { *distance = 0.0f;          changed = true; }
    ImGui::SameLine();
    if ( ImGui::Button(b1) )  { *distance = maxDist * 0.5f; changed = true; }
    ImGui::SameLine();
    if ( ImGui::Button(b2) )  { *distance = maxDist;        changed = true; }

    return changed;
}

// 「既定値を使う」チェックつきの数値欄。
//   value が unsetValue 以下（=既定を使う）の間はチェックON＋既定値をグレー表示する
bool DrawOptionalFloat(const char* label, float* value, float defaultValue, float unsetValue,
                       float speed, float minValue, float maxValue, const char* format) {
    bool changed = false;
    bool useDefault = ( *value <= unsetValue );
    ImGui::PushID(label);
    if ( ImGui::Checkbox("既定", &useDefault) ) {
        *value = useDefault ? unsetValue : defaultValue;
        changed = true;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f);
    if ( useDefault ) {
        float shown = defaultValue;
        ImGui::BeginDisabled();
        ImGui::DragFloat(label, &shown, speed, minValue, maxValue, format);
        ImGui::EndDisabled();
    } else if ( ImGui::DragFloat(label, value, speed, minValue, maxValue, format) ) {
        *value = std::clamp(*value, minValue, maxValue);
        changed = true;
    }
    ImGui::PopID();
    return changed;
}

} // namespace EnemyEditorWidgets
#endif
