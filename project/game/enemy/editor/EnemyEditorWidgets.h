#pragma once
#include "game/enemy/Enemy.h" // EnemyType / EnemySpawnData
#include "externals/imgui/imgui.h"
#include <vector>

class SplineRail;

// =====================================================================
//  EnemyEditorWidgets : 敵エディタの各部（新しく置く／一覧／設定欄）で共有する小さな部品
//   種類名の表・種類コンボ・レール選択・距離入力など、同じ見た目を2か所以上で使うものを集める。
//   敵エディタの .cpp からだけ読む（ImGui を持ち込むので EnemyEditor.h には含めない）
// =====================================================================
namespace EnemyEditorWidgets {
    // 種類の表示名（コンボの並び＝enum の並び）
    inline constexpr int kTypeCount = 3;
    inline constexpr const char* kTypeNames[kTypeCount] = {
        "ドングリン (地上・歩く)", "カミバナ (植物・噛みつき)", "フワリン (空中・浮遊)"
    };

#ifdef USE_IMGUI
    // 種類の色（Game View のピンと同じ色。一覧の●に使う）
    ImVec4 TypeColorOf(EnemyType type);

    // 種類のコンボ。選び直されたら true
    bool DrawTypeCombo(const char* label, EnemyType* type);

    // レール選択用ドロップダウン（レール番号・タイプ・長さ・配置済み敵数を表示）
    bool DrawRailCombo(const char* label, int* railIndex,
                       const std::vector<SplineRail>& rails, const std::vector<EnemySpawnData>& spawns);

    // 距離入力（DragFloat ＋ 始点・中央・終点のクイックボタン）
    bool DrawDistanceWidget(const char* idSuffix, float* distance, float maxDist);

    // 「既定値を使う」チェックつきの数値欄
    bool DrawOptionalFloat(const char* label, float* value, float defaultValue, float unsetValue,
                           float speed, float minValue, float maxValue, const char* format);
#endif
}
