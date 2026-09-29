#include "game/editor/strip/StripControls.h"

#include "engine/rail/SplineRail.h"
#include "engine/utils/Level/RailEditor.h"
#include "externals/imgui/imgui.h"
#include "game/editor/strip/StripCommon.h"
#include "game/enemy/EnemyEditor.h"

#include <algorithm>
#include <cstdio>
#include <vector>

using namespace railstrip;

namespace {
#ifdef USE_IMGUI
    // 横に並べていき、幅（rowRight）に入らなくなったら次の行へ折り返す。
    //   extraWidth は文字以外の幅（チェックボックスの四角・スライダーの本体など）
    void SameLineIfFits(const char* nextLabel, float extraWidth, float rowRight){
        const ImGuiStyle& style = ImGui::GetStyle();
        float nextWidth = ImGui::CalcTextSize(nextLabel, nullptr, true).x + style.FramePadding.x * 2.0f + extraWidth;
        if ( ImGui::GetItemRectMax().x + style.ItemSpacing.x + nextWidth <= rowRight ) { ImGui::SameLine(); }
    }

    // 置く物の種類のボタン（ブロック・敵で共通）。
    //   選んでいる種類はそのままの色、他は暗くして並べる。
    //   darkTextWhenChosen=true の時は、文字も色に合わせて読みやすい方にする（明るい色のブロック用）
    bool TypeButton(int id, const char* label, ImU32 baseColor, bool chosen, bool darkTextWhenChosen){
        ImVec4 color = ImGui::ColorConvertU32ToFloat4(baseColor);
        float dim = chosen ? 1.0f : 0.45f;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(color.x * dim, color.y * dim, color.z * dim, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x * 0.85f, color.y * 0.85f, color.z * 0.85f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
        int pushed = 3;
        if ( darkTextWhenChosen ) {
            ImGui::PushStyleColor(ImGuiCol_Text, chosen ? ImVec4(0.08f, 0.08f, 0.08f, 1.0f) : ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
            ++pushed;
        }
        ImGui::PushID(id);
        const bool pressed = ImGui::Button(label);
        ImGui::PopID();
        ImGui::PopStyleColor(pushed);
        return pressed;
    }
#endif
}

// ----------------------------------------------------------------
//  上段：レールの選択・表示の設定
// ----------------------------------------------------------------
int StripControls::DrawHeader(const StripContext& context, StripState& state) const{
    int requestedRail = -1;
#ifdef USE_IMGUI
    const auto& rails = *context.rails;
    const int railCount = ( int ) rails.size();
    const int currentRail = state.currentRail;
    const SplineRail& rail = rails[currentRail];

    auto typeText = [](const SplineRail& target) -> const char* {
        return ( target.type == SplineRail::RailType::Horizontal ) ? "横" : "縦";
    };

    const float rowRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;

    // --- レールの選択 ---
    char preview[96];
    std::snprintf(preview, sizeof(preview), "Rail %d  [%s] %.1fm", currentRail, typeText(rail), rail.GetLength());
    ImGui::SetNextItemWidth(( std::min )( 300.0f, ImGui::GetContentRegionAvail().x ));
    if ( ImGui::BeginCombo("##stripRail", preview) ) {
        // 置いてある物の数をレールごとに数える（どのレールに何があるかの目安）
        std::vector<int> blockCount(railCount, 0), enemyCount(railCount, 0), coinCount(railCount, 0);
        for ( const BlockData& block : context.railEditor->GetBlocks() ) {
            if ( block.rail >= 0 && block.rail < railCount ) { ++blockCount[block.rail]; }
        }
        for ( const EnemySpawnData& spawn : context.enemyEditor->GetSpawnDatas() ) {
            if ( spawn.railIndex >= 0 && spawn.railIndex < railCount ) { ++enemyCount[spawn.railIndex]; }
        }
        for ( const CoinData& coin : context.railEditor->GetCoins() ) {
            if ( coin.rail >= 0 && coin.rail < railCount ) { ++coinCount[coin.rail]; }
        }
        for ( int i = 0; i < railCount; ++i ) {
            const bool usable = RailUsable(rails[i]);
            char label[160];
            if ( usable ) {
                std::snprintf(label, sizeof(label), "Rail %d  [%s] %.1fm   ブロック%d 敵%d コイン%d%s",
                    i, typeText(rails[i]), rails[i].GetLength(),
                    blockCount[i], enemyCount[i], coinCount[i],
                    rails[i].visible ? "" : "  (見えないレール)");
            } else {
                std::snprintf(label, sizeof(label), "Rail %d  (点が足りない)", i);
            }
            ImGui::BeginDisabled(!usable);
            if ( ImGui::Selectable(label, i == currentRail) ) { requestedRail = i; }
            ImGui::EndDisabled();
            if ( i == currentRail ) { ImGui::SetItemDefaultFocus(); }
        }
        ImGui::EndCombo();
    }
    // 前後のレールへ（使えないレールは飛ばす）
    auto stepRail = [&](int direction) -> int {
        for ( int step = 1; step <= railCount; ++step ) {
            int candidate = ( ( currentRail + direction * step ) % railCount + railCount ) % railCount;
            if ( RailUsable(rails[candidate]) ) { return candidate; }
        }
        return -1;
    };
    if ( ImGui::ArrowButton("##stripPrevRail", ImGuiDir_Left) )  { requestedRail = stepRail(-1); }
    if ( ImGui::IsItemHovered() ) { ImGui::SetTooltip("前のレールへ"); }
    ImGui::SameLine();
    if ( ImGui::ArrowButton("##stripNextRail", ImGuiDir_Right) ) { requestedRail = stepRail(+1); }
    if ( ImGui::IsItemHovered() ) { ImGui::SetTooltip("次のレールへ"); }
    SameLineIfFits("カメラをここへ (F)", 0.0f, rowRight);
    if ( ImGui::Button("カメラをここへ (F)") ) {
        state.RequestFocus(currentRail, std::clamp(state.viewCenterDist, 0.0f, rail.GetLength()), 1.0f);
    }
    SameLineIfFits("選択を連動", ImGui::GetFrameHeight(), rowRight);
    ImGui::Checkbox("選択を連動", &state.followEditorRail);
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip("ON：Game View やレールエディタでレールを選ぶと、ここも同じレールを開く\n"
            "（ここでレールを選んだ時は、レールエディタ側の選択も変わる）");
    }

    // このレールの性質（配置の前に知っておきたいこと）
    const ImVec4 kPlain { 0.7f, 0.7f, 0.75f, 1.0f };
    const ImVec4 kBlue  { 0.55f, 0.75f, 1.0f, 1.0f };
    bool firstTag = true;
    auto tag = [&](bool shown, const ImVec4& color, const char* text) {
        if ( !shown ) return;
        if ( !firstTag ) { ImGui::SameLine(); }
        firstTag = false;
        ImGui::TextColored(color, "%s", text);
    };
    tag(!rail.visible,      kPlain, "見えないレール");
    tag(rail.roadMode != 0, kPlain, "道なし");
    tag(rail.HasMotion(),   kBlue,  "動くレール");
    tag(rail.isLoop,        kBlue,  "ループ");
#else
    ( void ) context; ( void ) state;
#endif
    return requestedRail;
}

// ----------------------------------------------------------------
//  表示の設定（横の位置・拡大・段数）
// ----------------------------------------------------------------
bool StripControls::DrawViewOptions(StripState& state) const{
    bool sideClicked = false;
#ifdef USE_IMGUI
    const float rowRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    ImGui::Separator();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("横の位置");
    const char* sideNames[5] = { "-2", "-1", "中心", "+1", "+2" };
    for ( int i = 0; i < 5; ++i ) {
        int side = i - 2;
        ImGui::SameLine();
        bool chosen = ( state.sideLayer == side );
        if ( chosen ) { ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.75f, 1.0f)); }
        ImGui::PushID(i);
        if ( ImGui::Button(sideNames[i], ImVec2(40.0f, 0.0f)) ) {
            state.sideLayer = side;
            sideClicked = true;
        }
        ImGui::PopID();
        if ( chosen ) { ImGui::PopStyleColor(); }
        if ( ImGui::IsItemHovered() ) {
            ImGui::SetTooltip("ブロックを置く道幅方向の位置（1m刻み）。\n"
                "「中心」に置いたブロックだけが乗れる/ぶつかる。脇（±1, ±2）は当たらない飾りになる");
        }
    }
    SameLineIfFits("他の位置も薄く表示", ImGui::GetFrameHeight(), rowRight);
    ImGui::Checkbox("他の位置も薄く表示", &state.showOtherSides);
    SameLineIfFits("拡大", 100.0f, rowRight);
    ImGui::SetNextItemWidth(100.0f);
    ImGui::SliderFloat("拡大", &state.cellPx, kMinCellPx, kMaxCellPx, "%.0f");
    SameLineIfFits("全体", 0.0f, rowRight);
    if ( ImGui::Button("全体") ) { state.fitRequested = true; }
    if ( ImGui::IsItemHovered() ) { ImGui::SetTooltip("レール全体が見える大きさにする"); }
    SameLineIfFits("段数", 80.0f, rowRight);
    ImGui::SetNextItemWidth(80.0f);
    ImGui::SliderInt("段数", &state.visibleLevels, 4, kMaxLevels);
#else
    ( void ) state;
#endif
    return sideClicked;
}

// ----------------------------------------------------------------
//  道具の選択・置く物の種類・元に戻す
// ----------------------------------------------------------------
void StripControls::DrawToolbar(const StripContext& context, StripState& state) const{
#ifdef USE_IMGUI
    ImGui::Separator();
    ImGui::BeginDisabled(!context.editable);

    const float rowRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    const float radioExtra = ImGui::GetFrameHeight(); // ラジオボタンの丸の分

    int toolIndex = ( int ) state.tool;
    ImGui::RadioButton("ブロック (B)", &toolIndex, 0);
    SameLineIfFits("敵 (E)", radioExtra, rowRight);
    ImGui::RadioButton("敵 (E)", &toolIndex, 1);
    SameLineIfFits("コイン (C)", radioExtra, rowRight);
    ImGui::RadioButton("コイン (C)", &toolIndex, 2);
    // 道具が変わった時の「途中の操作をやめる」は、変化を見た RailStripPanel が行う
    state.tool = ( StripTool ) toolIndex;

    // --- 置く物の種類 ---
    if ( state.tool == StripTool::Block ) {
        const int paintType = context.railEditor->GetBlockPaintType();
        for ( int type = 0; type < kBlockTypeCount; ++type ) {
            if ( type > 0 ) { SameLineIfFits(kBlockTypeNames[type], 0.0f, rowRight); }
            if ( TypeButton(type, kBlockTypeNames[type], BlockColor(type, 255), paintType == type, true) ) {
                context.railEditor->SetBlockPaintType(type);
            }
        }
    } else if ( state.tool == StripTool::Enemy ) {
        for ( int i = 0; i < 3; ++i ) {
            const char* name = EnemyEditor::GetTypeName(kEnemyTypes[i]);
            if ( i > 0 ) { SameLineIfFits(name, 0.0f, rowRight); }
            if ( TypeButton(i, name, EnemyColor(kEnemyTypes[i], 255), state.enemyType == kEnemyTypes[i], false) ) {
                state.enemyType = kEnemyTypes[i];
            }
        }
    } else {
        ImGui::TextDisabled("高さはマウスの位置で決まります");
    }

    // 元に戻す／やり直し。ブロックとコインはレールの履歴、敵は敵の履歴（道具に合わせて切り替わる）
    ImGui::BeginGroup();
    if ( state.tool == StripTool::Enemy ) {
        ImGui::BeginDisabled(!context.enemyEditor->CanUndo());
        if ( ImGui::Button("元に戻す##strip") ) { context.enemyEditor->Undo(); }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!context.enemyEditor->CanRedo());
        if ( ImGui::Button("やり直し##strip") ) { context.enemyEditor->Redo(); }
        ImGui::EndDisabled();
    } else {
        ImGui::BeginDisabled(context.railEditor->GetUndoCount() == 0);
        if ( ImGui::Button("元に戻す##strip") ) {
            context.railEditor->Undo();
            if ( context.onCoinsChanged ) { context.onCoinsChanged(); }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(context.railEditor->GetRedoCount() == 0);
        if ( ImGui::Button("やり直し##strip") ) {
            context.railEditor->Redo();
            if ( context.onCoinsChanged ) { context.onCoinsChanged(); }
        }
        ImGui::EndDisabled();
    }
    ImGui::EndGroup(); // 2つのボタンのどちらに触れても説明が出るように、まとめて1つの項目にする
    if ( ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) ) {
        ImGui::SetTooltip(state.tool == StripTool::Enemy
            ? "敵の配置と設定だけを戻す/やり直す（敵エディタの「元に戻す」と同じ履歴）"
            : "ブロック・コイン・レールの編集を戻す/やり直す（Ctrl+Z / Ctrl+Y と同じ履歴）");
    }

    ImGui::EndDisabled();

    // 使い方（行を取らないよう、(?) に触れた時だけ出す）
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip(
            "【ブロック】左クリック/ドラッグ=置く（置いてあるブロックの上では、選んでいる種類に塗り替え）\n"
            "　　　　　　右クリック/ドラッグ=消す / Shift+ドラッグ=四角くまとめて置く・消す（Esc でやめる）\n"
            "　　　　　　Ctrl+ドラッグ=ブロックを移動 / Alt+クリック=そのブロックの種類を選ぶ（スポイト）\n"
            "　　　　　　数字 1〜9=種類 / Z・X=横の位置を1つずらす\n"
            "　　　　　　マスが塞がっている時は半マスずらして置く（横長・台座を隣へぴったり付けられる）\n"
            "【敵】何もない所を左クリック=置く / 敵をドラッグ=移動（フワリンは上下で浮く高さ）\n"
            "　　　橙の■をドラッグ=動ける範囲 / 右クリック=メニュー / Delete=選んでいる敵を消す\n"
            "　　　数字 1〜3=種類 / Ctrl+Z・Ctrl+Y=敵の操作を戻す・やり直す（展開図の上で）\n"
            "【コイン】左クリック=置く（高さはマウスの位置） / ドラッグ=移動 / 右クリック=消す\n"
            "【共通】ホイール=拡大 / 中ボタンドラッグ=表示を動かす / F=カメラを指している場所へ\n"
            "　　　　Alt を押している間は 0.5m 刻みに吸着しない（敵・コイン）\n"
            "　　　　B / E / C=道具の切り替え / 下の帯の「<R3」「R5>」「→R8」=つながっているレールを開く");
    }

    if ( !context.editable ) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "プレイ中は見るだけです");
    }
#else
    ( void ) context; ( void ) state;
#endif
}
