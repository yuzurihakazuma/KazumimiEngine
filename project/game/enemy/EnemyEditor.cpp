#include "EnemyEditor.h"
#include "engine/rail/SplineRail.h"
#include "externals/imgui/imgui.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

EnemyEditor::EnemyEditor() = default;
EnemyEditor::~EnemyEditor() = default;

namespace {
    const char* kTypeNames[] = { "ドングリン (地上・歩く)", "カミバナ (植物・噛みつき)", "フワリン (空中・浮遊)" };
    const int   kTypeCount = 3;

    // 種類ごとの既定値（UIに「既定＝いくつ」と見せるため）
    float DefaultSpeedOf(EnemyType type){ return Enemy::TypeSpecOf(type).speed; }
    float DefaultHoverOf(EnemyType type){ return Enemy::TypeSpecOf(type).hover; }

    // 種類の色（Game View のピンと同じ色。一覧の●に使う）
    ImVec4 TypeColorOf(EnemyType type){
        switch ( type ) {
        case EnemyType::Zako:   return ImVec4(1.0f, 0.35f, 0.25f, 1.0f); // 赤
        case EnemyType::Strong: return ImVec4(0.75f, 0.4f, 1.0f, 1.0f);  // 紫
        default:                return ImVec4(0.35f, 0.8f, 1.0f, 1.0f);  // 水色
        }
    }
}

// 敵タイプ名のヘルパー（コンボボックスや詳細表示用）
const char* EnemyEditor::GetTypeName(EnemyType type) {
    int index = ( int ) type;
    return ( index >= 0 && index < kTypeCount ) ? kTypeNames[index] : "Unknown";
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
    spawnDatas_.clear();
    // レール0、距離5.0mの場所に雑魚敵を初期配置する
    EnemySpawnData first;
    first.type = EnemyType::Zako;
    first.railIndex = 0;
    first.distance = 5.0f;
    spawnDatas_.push_back(first);
    // 新規配置のひな形。距離0だとレール始点＝プレイヤー開始位置に重なるので離しておく
    newTemplate_ = EnemySpawnData {};
    newTemplate_.distance = 5.0f;
    changed_ = true;
    undoStack_.clear();
    redoStack_.clear();
    lastCommitted_ = spawnDatas_;
}

void EnemyEditor::SetSpawnDatas(const std::vector<EnemySpawnData>& datas) {
    spawnDatas_ = datas;
    selectedEntry_ = -1;
    changed_ = true; // シーン側がリスポーンするよう変更フラグを立てる
    // マップが変わったので履歴は持ち越さない（前のマップの敵へ戻せてしまうのを防ぐ）
    undoStack_.clear();
    redoStack_.clear();
    lastCommitted_ = spawnDatas_;
}

// 敵を別の場所へ動かす。行動範囲つきの敵は範囲ごと平行移動する
void EnemyEditor::MoveEntry(int index, int railIndex, float distance, const std::vector<SplineRail>& splineRails){
    if ( index < 0 || index >= ( int ) spawnDatas_.size() ) return;
    if ( railIndex < 0 || railIndex >= ( int ) splineRails.size() ) return;
    EnemySpawnData& spawn = spawnDatas_[index];
    float railLength = splineRails[railIndex].GetLength();
    distance = std::clamp(distance, 0.0f, railLength);

    const bool ranged = ( spawn.patrolMin >= 0.0f || spawn.patrolMax >= 0.0f );
    if ( ranged ) {
        // 範囲の幅と、その中での相対位置を保ったまま動かす（レールの端では幅を保って内側へ寄せる）
        float oldMin = ( spawn.patrolMin >= 0.0f ) ? spawn.patrolMin : 0.0f;
        float oldMax = ( spawn.patrolMax >= 0.0f ) ? spawn.patrolMax : oldMin;
        float width  = ( std::min )( ( std::max )( oldMax - oldMin, 0.0f ), railLength );
        float newMin = oldMin + ( distance - spawn.distance );
        newMin = std::clamp(newMin, 0.0f, ( std::max )( railLength - width, 0.0f ));
        spawn.patrolMin = newMin;
        spawn.patrolMax = newMin + width;
        distance = std::clamp(distance, spawn.patrolMin, spawn.patrolMax);
    }
    spawn.railIndex = railIndex;
    spawn.distance  = distance;
}

// レールの数や並びが変わった：敵のレール番号を付け替える
void EnemyEditor::ApplyRailRemap(const std::vector<int>& oldToNew, const std::vector<EnemySpawnData>& revived){
    // 1つのリストを付け替える。範囲の外の番号（もともとレールが無い敵）はそのまま残す
    auto remap = [&](std::vector<EnemySpawnData>& list){
        std::erase_if(list, [&](const EnemySpawnData& spawn){
            return spawn.railIndex >= 0 && spawn.railIndex < ( int ) oldToNew.size() && oldToNew[spawn.railIndex] < 0;
        });
        for ( auto& spawn : list ) {
            if ( spawn.railIndex >= 0 && spawn.railIndex < ( int ) oldToNew.size() ) { spawn.railIndex = oldToNew[spawn.railIndex]; }
        }
        list.insert(list.end(), revived.begin(), revived.end());
    };
    const std::vector<EnemySpawnData> before = spawnDatas_;
    const EnemySpawnData* selectedBefore = ( selectedEntry_ >= 0 && selectedEntry_ < ( int ) before.size() ) ? &before[selectedEntry_] : nullptr;
    remap(spawnDatas_);
    // 履歴の中身も同じように付け替える。控え（lastCommitted_）も同じにするので、この変化は1手に数えない
    remap(lastCommitted_);
    for ( auto& snapshot : undoStack_ ) { remap(snapshot); }
    for ( auto& snapshot : redoStack_ ) { remap(snapshot); }

    // 選んでいた敵がまだ残っていれば、その敵を選んだままにする（番号が詰まっても追いかける）
    selectedEntry_ = -1;
    if ( selectedBefore ) {
        EnemySpawnData expected = *selectedBefore;
        if ( expected.railIndex >= 0 && expected.railIndex < ( int ) oldToNew.size() ) { expected.railIndex = oldToNew[expected.railIndex]; }
        for ( int i = 0; i < ( int ) spawnDatas_.size(); ++i ) {
            if ( spawnDatas_[i] == expected ) { selectedEntry_ = i; break; }
        }
    }
    if ( !( before == spawnDatas_ ) ) { changed_ = true; }
    if ( filterRail_ >= 0 && filterRail_ < ( int ) oldToNew.size() ) { filterRail_ = oldToNew[filterRail_]; }
}

bool EnemyEditor::ConsumeFocusRequest(int& outIndex){
    if ( focusRequest_ < 0 ) return false;
    outIndex = focusRequest_;
    focusRequest_ = -1;
    return outIndex < ( int ) spawnDatas_.size();
}

// ================================================================
//  敵の追加・削除・複製
// ================================================================
int EnemyEditor::AddEntry(const EnemySpawnData& spawn){
    spawnDatas_.push_back(spawn);
    selectedEntry_ = ( int ) spawnDatas_.size() - 1;
    scrollToSelected_ = true;
    changed_ = true;
    return selectedEntry_;
}

void EnemyEditor::RemoveEntry(int index){
    if ( index < 0 || index >= ( int ) spawnDatas_.size() ) return;
    spawnDatas_.erase(spawnDatas_.begin() + index);
    // 選択中の敵より前を消したら番号が1つ詰まる。消した本人を選んでいたら選択を外す
    if ( selectedEntry_ == index )     { selectedEntry_ = -1; }
    else if ( selectedEntry_ > index ) { --selectedEntry_; }
    changed_ = true;
}

int EnemyEditor::DuplicateEntry(int index, const std::vector<SplineRail>& splineRails){
    if ( index < 0 || index >= ( int ) spawnDatas_.size() ) return -1;
    EnemySpawnData copied = spawnDatas_[index];
    int newIndex = AddEntry(copied);
    // 同じ場所に重ねると選べなくなるので、2m先（レールの端なら2m手前）へずらして置く
    if ( copied.railIndex >= 0 && copied.railIndex < ( int ) splineRails.size() ) {
        float railLength = splineRails[copied.railIndex].GetLength();
        float shifted = copied.distance + 2.0f;
        if ( shifted > railLength ) { shifted = copied.distance - 2.0f; }
        MoveEntry(newIndex, copied.railIndex, shifted, splineRails);
    }
    return newIndex;
}

// ================================================================
//  元に戻す／やり直し
// ================================================================
void EnemyEditor::CommitPending(){
    if ( spawnDatas_ == lastCommitted_ ) return;
    undoStack_.push_back(lastCommitted_);
    if ( undoStack_.size() > kMaxHistory ) { undoStack_.erase(undoStack_.begin()); }
    redoStack_.clear();
    lastCommitted_ = spawnDatas_;
}

void EnemyEditor::TickHistory(){
#ifdef USE_IMGUI
    // ドラッグ中・数値入力中は積まない（1回の操作＝1手にするため、手を離した時にまとめて積む）
    if ( ImGui::IsMouseDown(0) || ImGui::IsAnyItemActive() ) return;
#endif
    CommitPending();
}

void EnemyEditor::Undo(){
    // まだ履歴に積んでいない変更（名前を打っている途中に「元に戻す」を押した等）は、先に1手として積む。
    //   積まずに戻すと、その変更とひとつ前の操作が1回でまとめて戻ってしまう
    CommitPending();
    if ( undoStack_.empty() ) return;
    redoStack_.push_back(spawnDatas_);
    spawnDatas_ = undoStack_.back();
    undoStack_.pop_back();
    lastCommitted_ = spawnDatas_;
    if ( selectedEntry_ >= ( int ) spawnDatas_.size() ) { selectedEntry_ = -1; }
    changed_ = true;
}

void EnemyEditor::Redo(){
    CommitPending(); // 積んでいない変更があれば、それが最新の手になる（やり直す先は無くなる）
    if ( redoStack_.empty() ) return;
    undoStack_.push_back(spawnDatas_);
    spawnDatas_ = redoStack_.back();
    redoStack_.pop_back();
    lastCommitted_ = spawnDatas_;
    if ( selectedEntry_ >= ( int ) spawnDatas_.size() ) { selectedEntry_ = -1; }
    changed_ = true;
}

#ifdef USE_IMGUI
// ================================================================
//  レール選択用ドロップダウンを描画するヘルパー関数
//  レール番号・タイプ・長さ・配置済み敵数を表示して選択できるようにする
// ================================================================
static bool DrawRailCombo(const char* label,
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
static bool DrawDistanceWidget(const char* idSuffix, float* distance, float maxDist) {
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
static bool DrawOptionalFloat(const char* label, float* value, float defaultValue, float unsetValue,
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
#endif

bool EnemyEditor::PassesFilter(const EnemySpawnData& spawn) const{
    if ( filterType_ >= 0 && ( int ) spawn.type != filterType_ ) return false;
    if ( filterRail_ >= 0 && spawn.railIndex != filterRail_ ) return false;
    if ( filterText_[0] != '\0' ) {
        std::string shown = DisplayName(spawn);
        if ( shown.find(filterText_) == std::string::npos ) return false;
    }
    return true;
}

// ================================================================
//  メインの描画関数
// ================================================================
void EnemyEditor::DrawWindow(const std::vector<SplineRail>& splineRails,
                             int pickRail, float pickDist, bool hasPick) {
#ifdef USE_IMGUI
    hoveredEntry_ = -1; // Game View ハイライトは毎フレーム取り直す
    if ( selectedEntry_ >= ( int ) spawnDatas_.size() ) { selectedEntry_ = -1; }

    ImGui::SetNextWindowSize(ImVec2(380.0f, 640.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("敵配置エディタ (Enemy Editor)");

    DrawToolbar(splineRails, pickRail, pickDist, hasPick);
    DrawList(splineRails);
    DrawInspector(splineRails, pickRail, pickDist, hasPick);

    ImGui::End();
#else
    ( void ) splineRails; ( void ) pickRail; ( void ) pickDist; ( void ) hasPick;
#endif
    // 履歴への確定（TickHistory）はシーンが毎フレーム呼ぶ。この窓が閉じていても
    // Game View や配置ビューでの変更を取りこぼさないため、ここでは呼ばない
}

// ----------------------------------------------------------------
//  上段：元に戻す／新しく置く
// ----------------------------------------------------------------
void EnemyEditor::DrawToolbar(const std::vector<SplineRail>& splineRails, int pickRail, float pickDist, bool hasPick){
#ifdef USE_IMGUI
    // --- 元に戻す／やり直し（敵の配置と設定だけが対象。レールの Ctrl+Z とは別）---
    ImGui::BeginDisabled(!CanUndo());
    if ( ImGui::Button("元に戻す") ) { Undo(); }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(redoStack_.empty());
    if ( ImGui::Button("やり直し") ) { Redo(); }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("敵 %d 体", ( int ) spawnDatas_.size());

    if ( !ImGui::CollapsingHeader("新しく置く", spawnDatas_.empty() ? ImGuiTreeNodeFlags_DefaultOpen : 0) ) return;

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
        spawnDatas_.push_back(spawn);
        selectedEntry_ = ( int ) spawnDatas_.size() - 1;
        scrollToSelected_ = true;
        changed_ = true;
    };

    int typeIndex = std::clamp(( int ) newTemplate_.type, 0, kTypeCount - 1);
    if ( ImGui::Combo("種類##new", &typeIndex, kTypeNames, kTypeCount) ) {
        newTemplate_.type = ( EnemyType ) typeIndex; // コンボの並び＝enumの並び
    }
    DrawRailCombo("レール##new", &newTemplate_.railIndex, splineRails, spawnDatas_);
    float maxDist = splineRails[newTemplate_.railIndex].GetLength();
    DrawDistanceWidget("new", &newTemplate_.distance, maxDist);
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
#else
    ( void ) splineRails; ( void ) pickRail; ( void ) pickDist; ( void ) hasPick;
#endif
}

// ----------------------------------------------------------------
//  一覧の1行
// ----------------------------------------------------------------
bool EnemyEditor::DrawListRow(int index){
#ifdef USE_IMGUI
    const EnemySpawnData& spawn = spawnDatas_[index];
    ImGui::PushID(index);

    // 行の頭に種類の色●（Game View のピンと同じ色）
    ImGui::TextColored(TypeColorOf(spawn.type), "●");
    ImGui::SameLine();

    // 動きの設定が一目で分かる印
    std::string tags;
    if ( spawn.patrol )            { tags += " [巡回]"; }
    if ( spawn.chaseRange > 0.0f ) { tags += " [追跡]"; }
    if ( spawn.speed > 0.0f )      { tags += " [速度]"; }
    char label[192];
    if ( groupByRail_ ) {
        std::snprintf(label, sizeof(label), "#%02d %s  %.1fm%s",
            index, DisplayName(spawn).c_str(), spawn.distance, tags.c_str());
    } else {
        std::snprintf(label, sizeof(label), "#%02d %s  Rail %d  %.1fm%s",
            index, DisplayName(spawn).c_str(), spawn.railIndex, spawn.distance, tags.c_str());
    }

    bool clicked = false;
    bool isSelected = ( selectedEntry_ == index );
    if ( ImGui::Selectable(label, isSelected, ImGuiSelectableFlags_AllowDoubleClick) ) {
        selectedEntry_ = index;
        clicked = true;
        if ( ImGui::IsMouseDoubleClicked(0) ) { focusRequest_ = index; } // ダブルクリック＝カメラをそこへ
    }
    // ホバー中の敵は Game View で黄色くハイライト（どの行がどの敵か一目で分かる）
    if ( ImGui::IsItemHovered() ) { hoveredEntry_ = index; }
    // Game View でつかんだ敵など、外から選択された行まで一覧を送る
    if ( isSelected && scrollToSelected_ ) {
        ImGui::SetScrollHereY(0.5f);
        scrollToSelected_ = false;
    }
    ImGui::PopID();
    return clicked;
#else
    ( void ) index;
    return false;
#endif
}

// ----------------------------------------------------------------
//  中段：絞り込み＋一覧（高さ固定のスクロール領域）
// ----------------------------------------------------------------
void EnemyEditor::DrawList(const std::vector<SplineRail>& splineRails){
#ifdef USE_IMGUI
    ImGui::SeparatorText("配置済みの敵");

    // --- 絞り込み ---
    {
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
                for ( const auto& spawn : spawnDatas_ ) { if ( spawn.railIndex == railIndex ) ++count; }
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

    // --- 一覧（敵が増えてもウィンドウが伸びない。下の設定欄は常に同じ場所に出る）---
    float listHeight = std::clamp(ImGui::GetContentRegionAvail().y * 0.38f, 130.0f, 320.0f);
    ImGui::BeginChild("##enemyList", ImVec2(0.0f, listHeight), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY);
    int shownCount = 0;
    if ( groupByRail_ ) {
        // レール番号の小さい順に、敵のいるレールだけ見出しを出す
        int maxRail = -1;
        for ( const auto& spawn : spawnDatas_ ) { maxRail = ( std::max )( maxRail, spawn.railIndex ); }
        for ( int railIndex = 0; railIndex <= maxRail; ++railIndex ) {
            int railCount = 0;
            bool containsSelected = false;
            for ( int i = 0; i < ( int ) spawnDatas_.size(); ++i ) {
                if ( spawnDatas_[i].railIndex != railIndex || !PassesFilter(spawnDatas_[i]) ) continue;
                ++railCount;
                if ( i == selectedEntry_ ) { containsSelected = true; }
            }
            if ( railCount == 0 ) continue;
            // 外から選択された敵がいる見出しは自動で開く
            if ( containsSelected && scrollToSelected_ ) { ImGui::SetNextItemOpen(true); }
            char header[64];
            std::snprintf(header, sizeof(header), "Rail %d  (%d体)###rail%d", railIndex, railCount, railIndex);
            if ( ImGui::TreeNodeEx(header, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth) ) {
                for ( int i = 0; i < ( int ) spawnDatas_.size(); ++i ) {
                    if ( spawnDatas_[i].railIndex != railIndex || !PassesFilter(spawnDatas_[i]) ) continue;
                    DrawListRow(i);
                    ++shownCount;
                }
                ImGui::TreePop();
            } else {
                shownCount += railCount;
            }
        }
    } else {
        for ( int i = 0; i < ( int ) spawnDatas_.size(); ++i ) {
            if ( !PassesFilter(spawnDatas_[i]) ) continue;
            DrawListRow(i);
            ++shownCount;
        }
    }
    if ( spawnDatas_.empty() ) {
        ImGui::TextDisabled("敵が配置されていません");
    } else if ( shownCount == 0 ) {
        ImGui::TextDisabled("絞り込みに合う敵がいません");
    }
    scrollToSelected_ = false; // 絞り込みで行が出ていない時に持ち越さない
    ImGui::EndChild();
    ImGui::TextDisabled("クリック=選択 / ダブルクリック=カメラをそこへ / Game View でつかんで移動");
#else
    ( void ) splineRails;
#endif
}

// ----------------------------------------------------------------
//  下段：選択中の1体の設定
// ----------------------------------------------------------------
void EnemyEditor::DrawInspector(const std::vector<SplineRail>& splineRails, int pickRail, float pickDist, bool hasPick){
#ifdef USE_IMGUI
    ImGui::BeginChild("##enemyInspector", ImVec2(0.0f, 0.0f));

    if ( selectedEntry_ < 0 || selectedEntry_ >= ( int ) spawnDatas_.size() ) {
        ImGui::SeparatorText("敵の設定");
        ImGui::TextDisabled("一覧か Game View で敵を選ぶと、ここで1体ずつ動きを決められます");
        // --- 全削除（誤爆防止の2段階式：1回目で確認表示 → 3秒以内にもう一度で実行）---
        if ( spawnDatas_.size() >= 2 ) {
            ImGui::Spacing();
            bool armed = ( ImGui::GetTime() - clearArmedTime_ ) < 3.0;
            ImGui::PushStyleColor(ImGuiCol_Button, armed ? ImVec4(0.85f, 0.2f, 0.2f, 1.0f) : ImVec4(0.5f, 0.12f, 0.12f, 1.0f));
            if ( ImGui::Button(armed ? "本当に全削除する（もう一度クリック）##clear" : "敵を全削除##clear", ImVec2(-1.0f, 0.0f)) ) {
                if ( armed ) {
                    spawnDatas_.clear();
                    selectedEntry_ = -1;
                    changed_ = true;
                    clearArmedTime_ = -100.0;
                } else {
                    clearArmedTime_ = ImGui::GetTime();
                }
            }
            ImGui::PopStyleColor();
        }
        ImGui::TextDisabled("※配置はマップと一緒に保存される（自動保存 / 上書き保存 / Ctrl+S）");
        ImGui::EndChild();
        return;
    }

    const int index = selectedEntry_;
    EnemySpawnData& spawn = spawnDatas_[index];
    ImGui::PushID(index);

    char title[128];
    std::snprintf(title, sizeof(title), "#%02d %s の設定", index, DisplayName(spawn).c_str());
    ImGui::SeparatorText(title);

    if ( ImGui::Button("カメラをここへ") ) { focusRequest_ = index; }
    ImGui::SameLine();
    if ( ImGui::Button("選択を外す") ) {
        selectedEntry_ = -1;
        ImGui::PopID();
        ImGui::EndChild();
        return;
    }

    // ================= 基本 =================
    {
        char nameBuffer[64] = {};
        std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", spawn.name.c_str());
        if ( ImGui::InputTextWithHint("名前", GetTypeLabel(spawn.type), nameBuffer, sizeof(nameBuffer)) ) {
            spawn.name = nameBuffer;
            changed_ = true;
        }
        int typeIndex = std::clamp(( int ) spawn.type, 0, kTypeCount - 1);
        if ( ImGui::Combo("種類", &typeIndex, kTypeNames, kTypeCount) ) {
            spawn.type = ( EnemyType ) typeIndex;
            changed_ = true;
        }
    }

    const bool validRail = ( spawn.railIndex >= 0 && spawn.railIndex < ( int ) splineRails.size() );
    const float railLength = validRail ? splineRails[spawn.railIndex].GetLength() : 100.0f;
    if ( !validRail ) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "レール %d がありません（下で選び直してください）", spawn.railIndex);
    }

    // ================= 場所 =================
    ImGui::SeparatorText("場所");
    if ( !splineRails.empty() ) {
        int railIndex = spawn.railIndex;
        if ( DrawRailCombo("レール", &railIndex, splineRails, spawnDatas_) ) {
            // 別のレールへ：距離は同じ値のまま（新しいレールの長さに収める）。行動範囲も一緒に動く
            MoveEntry(index, railIndex, spawn.distance, splineRails);
            changed_ = true;
        }
    }
    {
        float distance = spawn.distance;
        if ( DrawDistanceWidget("edit", &distance, railLength) && validRail ) {
            MoveEntry(index, spawn.railIndex, distance, splineRails);
            changed_ = true;
        }
    }
    ImGui::BeginDisabled(!hasPick);
    if ( ImGui::Button("選択ノードの位置へ移動", ImVec2(-1.0f, 0.0f)) ) {
        MoveEntry(index, pickRail, pickDist, splineRails);
        changed_ = true;
    }
    ImGui::EndDisabled();

    // ================= 動き =================
    ImGui::SeparatorText("動き");
    {
        int moveMode = spawn.patrol ? 1 : 0;
        if ( ImGui::RadioButton("その場にいる", &moveMode, 0) ) { spawn.patrol = false; changed_ = true; }
        ImGui::SameLine();
        if ( ImGui::RadioButton("巡回する（往復）", &moveMode, 1) ) { spawn.patrol = true; changed_ = true; }

        // プレイヤーを追いかける
        bool chase = ( spawn.chaseRange > 0.0f );
        if ( ImGui::Checkbox("プレイヤーが近づくと追いかける", &chase) ) {
            spawn.chaseRange = chase ? 5.0f : 0.0f;
            changed_ = true;
        }
        if ( chase ) {
            ImGui::Indent(16.0f);
            ImGui::SetNextItemWidth(110.0f);
            if ( ImGui::DragFloat("気づく距離(m)", &spawn.chaseRange, 0.1f, 0.5f, 30.0f, "%.1f") ) {
                spawn.chaseRange = std::clamp(spawn.chaseRange, 0.5f, 30.0f);
                changed_ = true;
            }
            ImGui::SetNextItemWidth(110.0f);
            if ( ImGui::DragFloat("追う時の速さ(倍)", &spawn.chaseSpeedMul, 0.05f, 0.5f, 4.0f, "%.2f") ) {
                spawn.chaseSpeedMul = std::clamp(spawn.chaseSpeedMul, 0.5f, 4.0f);
                changed_ = true;
            }
            if ( !spawn.patrol ) { ImGui::TextDisabled("見失うと置いた場所へ歩いて戻ります"); }
            ImGui::Unindent(16.0f);
        }

        const bool moves = ( spawn.patrol || chase );
        ImGui::BeginDisabled(!moves);
        // 速度
        if ( DrawOptionalFloat("速度(m/s)", &spawn.speed, DefaultSpeedOf(spawn.type), 0.0f,
                               0.05f, 0.1f, 15.0f, "%.2f") ) {
            changed_ = true;
        }
        // 最初の向き
        int startDir = ( spawn.startDir < 0 ) ? 1 : 0;
        ImGui::TextUnformatted("最初に進む向き"); ImGui::SameLine();
        if ( ImGui::RadioButton("終点側へ", &startDir, 0) ) { spawn.startDir = 1;  changed_ = true; }
        ImGui::SameLine();
        if ( ImGui::RadioButton("始点側へ", &startDir, 1) ) { spawn.startDir = -1; changed_ = true; }
        ImGui::EndDisabled();

        // 折り返しの間
        ImGui::BeginDisabled(!spawn.patrol);
        ImGui::SetNextItemWidth(110.0f);
        if ( ImGui::DragFloat("折り返しで止まる(秒)", &spawn.turnWait, 0.05f, 0.0f, 10.0f, "%.2f") ) {
            spawn.turnWait = std::clamp(spawn.turnWait, 0.0f, 10.0f);
            changed_ = true;
        }
        ImGui::EndDisabled();

        // 行動範囲（巡回・追跡の両方に効く。-1=レール全体）
        ImGui::BeginDisabled(!moves);
        bool ranged = ( spawn.patrolMin >= 0.0f || spawn.patrolMax >= 0.0f );
        if ( ImGui::Checkbox("動ける範囲を決める（OFF=レール全体）", &ranged) ) {
            if ( ranged ) {
                // 初期値は「今の位置±3m」（Game View に橙線で範囲が出る）
                spawn.patrolMin = std::clamp(spawn.distance - 3.0f, 0.0f, railLength);
                spawn.patrolMax = std::clamp(spawn.distance + 3.0f, spawn.patrolMin, railLength);
            } else {
                spawn.patrolMin = spawn.patrolMax = -1.0f;
            }
            changed_ = true;
        }
        if ( ranged ) {
            if ( ImGui::DragFloatRange2("範囲(m)", &spawn.patrolMin, &spawn.patrolMax,
                    0.1f, 0.0f, railLength, "始 %.1f", "終 %.1f") ) {
                spawn.patrolMin = std::clamp(spawn.patrolMin, 0.0f, railLength);
                spawn.patrolMax = std::clamp(spawn.patrolMax, spawn.patrolMin, railLength);
                changed_ = true;
            }
            ImGui::TextDisabled("(Game View に橙色の線で範囲が出ます)");
        }
        ImGui::EndDisabled();
    }

    // ================= 高さ・大きさ =================
    ImGui::SeparatorText("高さ・大きさ");
    if ( DrawOptionalFloat("浮く高さ(m)", &spawn.hoverHeight, DefaultHoverOf(spawn.type), -0.0001f,
                           0.05f, 0.0f, 8.0f, "%.2f") ) {
        changed_ = true;
    }
    ImGui::SetNextItemWidth(110.0f);
    if ( ImGui::DragFloat("ふわふわの幅(m)", &spawn.bobAmp, 0.01f, 0.0f, 3.0f, "%.2f") ) {
        spawn.bobAmp = std::clamp(spawn.bobAmp, 0.0f, 3.0f);
        changed_ = true;
    }
    if ( spawn.bobAmp > 0.0f ) {
        ImGui::SetNextItemWidth(110.0f);
        if ( ImGui::DragFloat("ふわふわの速さ", &spawn.bobSpeed, 0.05f, 0.1f, 12.0f, "%.2f") ) {
            spawn.bobSpeed = std::clamp(spawn.bobSpeed, 0.1f, 12.0f);
            changed_ = true;
        }
    }
    ImGui::SetNextItemWidth(110.0f);
    if ( ImGui::DragFloat("大きさ(倍)", &spawn.scale, 0.01f, 0.3f, 4.0f, "%.2f") ) {
        spawn.scale = std::clamp(spawn.scale, 0.3f, 4.0f);
        changed_ = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(当たり判定も同じ倍率)");

    // ================= 種類ごとの設定 =================
    if ( spawn.type == EnemyType::Strong ) {
        ImGui::SeparatorText("カミバナ");
        if ( DrawOptionalFloat("噛みつく距離(m)", &spawn.biteRange, 2.6f, -0.0001f,
                               0.05f, 0.0f, 10.0f, "%.2f") ) {
            changed_ = true;
        }
    }

    // ================= まとめて反映 =================
    ImGui::SeparatorText("この動きを他の敵にも");
    {
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
            for ( int i = 0; i < ( int ) spawnDatas_.size(); ++i ) {
                if ( i != index && spawnDatas_[i].type == spawn.type ) { copyMotionTo(spawnDatas_[i]); }
            }
            changed_ = true;
        }
        ImGui::SameLine();
        if ( ImGui::Button("同じレールの全員へ", ImVec2(halfWidth, 0.0f)) ) {
            for ( int i = 0; i < ( int ) spawnDatas_.size(); ++i ) {
                if ( i != index && spawnDatas_[i].railIndex == spawn.railIndex ) { copyMotionTo(spawnDatas_[i]); }
            }
            changed_ = true;
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    // --- 操作ボタン行：複製と削除を横に並べる ---
    bool deleteRequested = false;
    float halfWidth = ( ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x ) * 0.5f;
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.2f, 0.35f, 0.55f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.45f, 0.7f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.3f, 0.55f, 0.85f, 1.0f));
    bool duplicateRequested = ImGui::Button("複製（2m先へ）", ImVec2(halfWidth, 0.0f));
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.5f, 0.12f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.18f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.9f, 0.22f, 0.22f, 1.0f));
    if ( ImGui::Button("削除", ImVec2(halfWidth, 0.0f)) ) { deleteRequested = true; }
    ImGui::PopStyleColor(3);

    ImGui::PopID();

    // 配列を書き換える操作は、spawn の参照を使い終わってから行う
    if ( duplicateRequested ) {
        DuplicateEntry(index, splineRails);
    } else if ( deleteRequested ) {
        RemoveEntry(index);
    }

    ImGui::EndChild();
#else
    ( void ) splineRails; ( void ) pickRail; ( void ) pickDist; ( void ) hasPick;
#endif
}
