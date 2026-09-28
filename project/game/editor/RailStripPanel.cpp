#include "game/editor/RailStripPanel.h"

#include "engine/graphics/DebugDraw.h"
#include "engine/rail/SplineRail.h"
#include "engine/utils/Level/LevelEditor.h"
#include "engine/utils/Level/RailEditor.h"
#include "externals/imgui/imgui.h"
#include "game/enemy/EnemyEditor.h"
#include "game/stage/BlockSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
    const int   kMaxLevels      = 8;     // 置ける段数（0〜7段）
    const int   kBlockTypeCount = 9;
    const float kCoinMaxHeight  = 5.0f;  // コインの高さの上限(m)
    const float kHoverMaxHeight = 8.0f;  // 敵の浮く高さの上限(m)
    const float kPadLeft        = 40.0f;
    const float kPadRight       = 40.0f;
    const float kRulerHeight    = 20.0f;
    const float kGroundHeight   = 12.0f;
    const float kInfoHeight     = 46.0f;

    const char* kBlockTypeNames[kBlockTypeCount] = {
        "スポンジ", "段ボール", "斜面45°", "斜面26°", "バネ", "？", "すり抜け床", "横長2m", "台座2×2" };

    bool RailUsable(const SplineRail& rail){
        return rail.nodes.size() >= 2 && rail.GetLength() > 0.01f;
    }
    // 表示と当たりに使う距離。レールが短くなって範囲の外に残った物は、端に寄せて見せる
    float ShownDist(float dist, float railLength){ return std::clamp(dist, 0.0f, railLength); }

#ifdef USE_IMGUI
    float SnapTo(float value, float step){ return std::round(value / step) * step; }

    // 敵の体の中心の高さ（レール面から。浮いている敵は浮いた分も含む）
    float EnemyCenterHeight(const EnemySpawnData& spawn){
        EnemyTypeSpec spec = Enemy::TypeSpecOf(spawn.type);
        return Enemy::HoverOf(spawn) + ( spec.bodyBottom + spec.bodyTop ) * 0.5f * spawn.scale;
    }
    // 足元から体の中心までの高さ
    float EnemyBodyMid(const EnemySpawnData& spawn){
        EnemyTypeSpec spec = Enemy::TypeSpecOf(spawn.type);
        return ( spec.bodyBottom + spec.bodyTop ) * 0.5f * spawn.scale;
    }
    // 動ける範囲を決めてある敵か
    bool EnemyHasRange(const EnemySpawnData& spawn){
        return ( spawn.patrol || spawn.chaseRange > 0.0f )
            && ( spawn.patrolMin >= 0.0f || spawn.patrolMax >= 0.0f );
    }

    // ブロックの色（モデルの見た目に近い色）
    ImU32 BlockColor(int type, int alpha){
        switch ( type ) {
        case 0:  return IM_COL32(238, 200, 72, alpha);  // スポンジ（黄）
        case 1:  return IM_COL32(186, 140, 92, alpha);  // 段ボール層
        case 2:  return IM_COL32(200, 156, 104, alpha); // 斜面45°
        case 3:  return IM_COL32(200, 156, 104, alpha); // ゆるい斜面
        case 4:  return IM_COL32(96, 200, 110, alpha);  // ジャンプ台（緑）
        case 5:  return IM_COL32(250, 186, 40, alpha);  // ？ブロック（金）
        case 6:  return IM_COL32(236, 240, 246, alpha); // すり抜け床（白）
        case 7:  return IM_COL32(238, 200, 72, alpha);  // 横長
        default: return IM_COL32(170, 124, 80, alpha);  // 台座
        }
    }
    // 敵の色（Game View のピン・敵エディタの一覧と同じ色）
    ImU32 EnemyColor(EnemyType type, int alpha){
        switch ( type ) {
        case EnemyType::Zako:   return IM_COL32(255, 90, 64, alpha);   // 赤
        case EnemyType::Strong: return IM_COL32(190, 102, 255, alpha); // 紫
        default:                return IM_COL32(90, 204, 255, alpha);  // 水色
        }
    }
    // 横に並べていき、幅（rowRight）に入らなくなったら次の行へ折り返す。
    //   extraWidth は文字以外の幅（チェックボックスの四角・スライダーの本体など）
    void SameLineIfFits(const char* nextLabel, float extraWidth, float rowRight){
        const ImGuiStyle& style = ImGui::GetStyle();
        float nextWidth = ImGui::CalcTextSize(nextLabel, nullptr, true).x + style.FramePadding.x * 2.0f + extraWidth;
        if ( ImGui::GetItemRectMax().x + style.ItemSpacing.x + nextWidth <= rowRight ) { ImGui::SameLine(); }
    }

    // 文字を中央ぞろえで描く
    void AddTextCentered(ImDrawList* draw, const ImVec2& center, ImU32 color, const char* text){
        ImVec2 size = ImGui::CalcTextSize(text);
        draw->AddText({ center.x - size.x * 0.5f, center.y - size.y * 0.5f }, color, text);
    }
#endif
}

// =====================================================================
//  座標の変換（レール上の距離・高さ ⇔ 画面のピクセル）
//   マス k は距離 k を中心に前後 0.5m を占める（ブロックの置かれ方と同じ）
// =====================================================================
float RailStripPanel::DistToX(const Layout& layout, float dist) const{
    return layout.originX + layout.padLeft + ( dist + 0.5f ) * layout.cellPx;
}
float RailStripPanel::HeightToY(const Layout& layout, float heightMeters) const{
    return layout.groundY - heightMeters * layout.cellPx;
}
float RailStripPanel::XToDist(const Layout& layout, float screenX) const{
    return ( screenX - layout.originX - layout.padLeft ) / layout.cellPx - 0.5f;
}
float RailStripPanel::YToHeight(const Layout& layout, float screenY) const{
    return ( layout.groundY - screenY ) / layout.cellPx;
}

bool RailStripPanel::ConsumeFocusRequest(int& outRail, float& outDist, float& outHeight){
    if ( !focusPending_ ) return false;
    focusPending_ = false;
    outRail = focusRail_; outDist = focusDist_; outHeight = focusHeight_;
    return true;
}

bool RailStripPanel::ConsumeOpenEnemyPanelRequest(){
    bool pending = openEnemyPanelPending_;
    openEnemyPanelPending_ = false;
    return pending;
}

// 途中の操作（塗り・ドラッグ）を全部やめる。レールや道具を切り替えた時に呼ぶ
void RailStripPanel::CancelInteractions(){
    stroke_ = StrokeKind::None;
    moveIndex_ = -1;
    moveDragged_ = false;
    enemyDrag_ = EnemyDrag::None;
    enemyDragIndex_ = -1;
    enemyDragChanged_ = false;
    enemyHoverAdjust_ = false;
    rangeAmbiguous_ = false;
    coinDragIndex_ = -1;
    panning_ = false;
}

// =====================================================================
//  ブロックの判定
// =====================================================================
int RailStripPanel::FindBlockAt(const Context& context, float dist, int level) const{
    const auto& blocks = context.railEditor->GetBlocks();
    const float railLength = ( *context.rails )[currentRail_].GetLength();
    int   found = -1;
    float bestGap = 1e9f;
    for ( int i = 0; i < ( int ) blocks.size(); ++i ) {
        const BlockData& block = blocks[i];
        if ( block.rail != currentRail_ || block.level != level ) continue;
        if ( std::abs(block.side - ( float ) sideLayer_) > 0.5f ) continue;
        float gap = std::abs(ShownDist(block.dist, railLength) - dist);
        if ( gap > BlockSystem::VisualHalfAlong(block.type) ) continue; // 見えている形の中を指しているか
        if ( gap < bestGap ) { bestGap = gap; found = i; }
    }
    return found;
}

// RailEditor::AddBlock と同じ決まり：占有幅どうしが重なる場所には置けない
bool RailStripPanel::CanPlaceBlock(const Context& context, float dist, int level, int type, int ignoreIndex) const{
    if ( level < 0 || level >= kMaxLevels ) return false;
    const SplineRail& rail = ( *context.rails )[currentRail_];
    const float railLength = rail.GetLength();
    if ( dist < 0.0f || dist > railLength ) return false;
    if ( !context.railEditor->CanPlaceBlock(currentRail_, dist, level, ( float ) sideLayer_, type, ignoreIndex) ) {
        return false;
    }
    // ループ（始点と終点がつながったレール）：つなぎ目をまたいで重なる置き方も止める
    if ( rail.isLoop ) {
        const auto& blocks = context.railEditor->GetBlocks();
        const float newHalf = RailEditor::BlockOccupyHalfOf(type);
        for ( int i = 0; i < ( int ) blocks.size(); ++i ) {
            if ( i == ignoreIndex ) continue;
            const BlockData& block = blocks[i];
            if ( block.rail != currentRail_ || block.level != level ) continue;
            if ( std::abs(block.side - ( float ) sideLayer_) >= 0.51f ) continue;
            float acrossSeam = std::abs(std::abs(block.dist - dist) - railLength);
            if ( acrossSeam < RailEditor::BlockOccupyHalfOf(block.type) + newHalf - 0.01f ) return false;
        }
    }
    return true;
}

// マウスの位置から、実際に置く距離を決める。
//   まずマスの中心（整数の距離）。そこが塞がっている時だけ、半マスずらした位置を
//   マウスに近い側から試す。横長・台座（2m）を隣のブロックへぴったり付けて置けるようにするため
bool RailStripPanel::ResolvePlaceDist(const Context& context, float mouseDist, int level, int type,
                                      int ignoreIndex, float& outDist) const{
    const float cell = std::floor(mouseDist + 0.5f);
    const float nearSide = ( mouseDist >= cell ) ? 1.0f : -1.0f;
    const float candidates[3] = { cell, cell + 0.5f * nearSide, cell - 0.5f * nearSide };
    for ( float candidate : candidates ) {
        if ( CanPlaceBlock(context, candidate, level, type, ignoreIndex) ) {
            outDist = candidate;
            return true;
        }
    }
    return false;
}

// 塗りの1マスぶんを適用する（置く/塗り替える/消す は塗り始めに決めたものを続ける）。
//   置く時はマスの中心（cellDist）に置く。消す/塗り替える時は probeDist の位置にあるブロックを対象にする
//   （マウスの真下のマスではマウスの位置そのもの、飛ばしたマスを埋める時はマスの中心を渡す。
//     中心で探すと、2m のブロックや半マスずらしたブロックが並んだ所で隣のブロックを取り違える）
void RailStripPanel::ApplyBlockStroke(const Context& context, int cellDist, int level, float probeDist){
    RailEditor* railEditor = context.railEditor;
    const int paintType = railEditor->GetBlockPaintType();
    if ( stroke_ == StrokeKind::Place ) {
        const float dist = ( float ) cellDist;
        if ( ( *context.rails )[currentRail_].visible && CanPlaceBlock(context, dist, level, paintType) ) {
            railEditor->AddBlock(currentRail_, dist, level, ( float ) sideLayer_, paintType);
        }
    } else if ( stroke_ == StrokeKind::Replace || stroke_ == StrokeKind::Erase ) {
        int found = FindBlockAt(context, probeDist, level);
        if ( found >= 0 ) {
            if ( stroke_ == StrokeKind::Erase ) { railEditor->RemoveBlockAt(found); }
            else                                { railEditor->ReplaceBlockTypeAt(found, paintType); }
        }
    }
    strokeLastDist_  = cellDist;
    strokeLastLevel_ = level;
}

void RailStripPanel::SelectEnemy(const Context& context, int index){
    context.enemyEditor->SetSelectedEntry(index);
    lastSelectedEnemy_ = context.enemyEditor->GetSelectedEntry();
}

// =====================================================================
//  レールの選択
// =====================================================================
void RailStripPanel::SyncCurrentRail(const Context& context){
    const auto& rails = *context.rails;
    const int railCount = ( int ) rails.size();

    // レールエディタ側で選択が変わったら、こちらも同じレールを開く。
    //   作ったばかりのレールは、まだゲーム側のレールに入っていない（次のフレームで作られる）か、
    //   点が足りないことがある。その時は「開く予定」として覚えておき、開けるようになったら開く
    int editorRail = context.railEditor->GetCurrentRailIndex();
    if ( editorRail != lastEditorRail_ ) {
        lastEditorRail_ = editorRail; // 見た値はいつも覚える（元のレールへ選び直した時も変化として拾える）
        pendingFollowRail_ = ( followEditorRail_ && editorRail != currentRail_ ) ? editorRail : -1;
    }
    if ( pendingFollowRail_ >= 0 ) {
        if ( !followEditorRail_ || pendingFollowRail_ != editorRail ) {
            pendingFollowRail_ = -1; // 連動を切った／エディタ側が別のレールへ移った
        } else if ( pendingFollowRail_ < railCount && RailUsable(rails[pendingFollowRail_]) ) {
            if ( pendingFollowRail_ != currentRail_ ) {
                currentRail_ = pendingFollowRail_;
                scrollResetPending_ = true;
                CancelInteractions();
            }
            pendingFollowRail_ = -1;
        }
    }

    // 敵エディタの一覧や Game View で別のレールの敵が選ばれたら、その敵のいるレールを開いて中央に見せる
    //   （レールエディタ側の選択は変えない。このパネルの中で選んだ時は lastSelectedEnemy_ が先に更新済み）
    const int selectedEnemy = context.enemyEditor->GetSelectedEntry();
    if ( selectedEnemy != lastSelectedEnemy_ ) {
        lastSelectedEnemy_ = selectedEnemy;
        const auto& spawns = context.enemyEditor->GetSpawnDatas();
        if ( followEditorRail_ && selectedEnemy >= 0 && selectedEnemy < ( int ) spawns.size() ) {
            const EnemySpawnData& spawn = spawns[selectedEnemy];
            if ( spawn.railIndex >= 0 && spawn.railIndex < railCount && RailUsable(rails[spawn.railIndex]) ) {
                if ( spawn.railIndex != currentRail_ ) {
                    currentRail_ = spawn.railIndex;
                    CancelInteractions();
                }
                scrollToDist_ = ShownDist(spawn.distance, rails[currentRail_].GetLength());
            }
        }
    }

    // 開いていたレールが消えた/短くなりすぎた時は、使える最初のレールへ移る
    if ( currentRail_ < 0 || currentRail_ >= railCount || !RailUsable(rails[currentRail_]) ) {
        currentRail_ = -1;
        for ( int i = 0; i < railCount; ++i ) {
            if ( RailUsable(rails[i]) ) { currentRail_ = i; break; }
        }
        scrollResetPending_ = true;
        scrollToDist_ = -1.0f;
        CancelInteractions();
    }
}

void RailStripPanel::SelectRail(const Context& context, int railIndex, float arrivalDist){
    const auto& rails = *context.rails;
    if ( railIndex < 0 || railIndex >= ( int ) rails.size() || !RailUsable(rails[railIndex]) ) return;
    if ( railIndex == currentRail_ ) return;
    currentRail_ = railIndex;
    CancelInteractions();
    // つながりをたどって来た時は着いた場所を、それ以外はレールの先頭を見せる
    if ( arrivalDist >= 0.0f ) { scrollToDist_ = ShownDist(arrivalDist, rails[railIndex].GetLength()); }
    else                       { scrollResetPending_ = true; scrollToDist_ = -1.0f; }
    // レールエディタと Game View の選択も合わせる（編集対象を固定している間は触らない）
    if ( followEditorRail_ && !context.railEditor->IsEditTargetLocked() ) {
        context.railEditor->SetCurrentRail(railIndex);
        context.railEditor->ClearMultiSelection(); // 前のレールの点が選ばれたまま残らないように
    }
    lastEditorRail_ = context.railEditor->GetCurrentRailIndex();
    pendingFollowRail_ = -1; // ここで選んだので、待っていた「開く予定」は取り消す
}

// =====================================================================
//  窓
// =====================================================================
void RailStripPanel::Draw(const Context& context){
#ifdef USE_IMGUI
    hoverValid_ = false;
    // 前のフレームに描かれていなかった（エディタを隠した・パネルを閉じた等）なら、途中の操作は捨てる。
    //   残しておくと、次に開いた瞬間に「ボタンを離した」扱いになって、古い範囲のまま塗られたり動いたりする
    const int frame = ImGui::GetFrameCount();
    if ( lastDrawFrame_ >= 0 && frame != lastDrawFrame_ + 1 ) { CancelInteractions(); }
    lastDrawFrame_ = frame;
    ImGui::SetNextWindowSize(ImVec2(980.0f, 330.0f), ImGuiCond_FirstUseEver);
    if ( !ImGui::Begin(kWindowTitle) ) {
        // 畳まれている/タブの裏にいる間は操作を続けない
        CancelInteractions();
        ImGui::End();
        return;
    }
    if ( !context.rails || !context.railEditor || !context.enemyEditor || context.rails->empty() ) {
        ImGui::TextDisabled("レールがありません（先にレールエディタでレールを作ってください）");
        ImGui::End();
        return;
    }
    SyncCurrentRail(context);
    if ( currentRail_ < 0 ) {
        ImGui::TextDisabled("点が2つ以上あるレールがありません（先にレールエディタでレールを作ってください）");
        ImGui::End();
        return;
    }
    if ( !context.editable ) { CancelInteractions(); }

    // 横に広い時：操作部を左の列にまとめ、展開図は右側で高さいっぱいに使う
    //   （このパネルは横長・低めの場所に置くことが多い。操作部を上に積むとマス目の段が見えなくなる）
    // 幅が狭い時：操作部を上に積む
    const float kControlsWidth = 336.0f;
    const bool sideBySide = ( ImGui::GetContentRegionAvail().x >= kControlsWidth + 420.0f );
    if ( sideBySide ) {
        ImGui::BeginChild("##stripControls", ImVec2(kControlsWidth, 0.0f));
        DrawHeader(context);
        DrawToolbar(context);
        DrawViewOptions();
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginGroup();
        DrawCanvas(context);
        ImGui::EndGroup();
    } else {
        DrawHeader(context);
        DrawToolbar(context);
        DrawViewOptions();
        DrawCanvas(context);
    }
    ImGui::End();
#else
    ( void ) context;
#endif
}

// ----------------------------------------------------------------
//  上段：レールの選択・表示の設定
// ----------------------------------------------------------------
void RailStripPanel::DrawHeader(const Context& context){
#ifdef USE_IMGUI
    const auto& rails = *context.rails;
    const int railCount = ( int ) rails.size();
    const SplineRail& rail = rails[currentRail_];

    auto typeText = [](const SplineRail& target) -> const char* {
        return ( target.type == SplineRail::RailType::Horizontal ) ? "横" : "縦";
    };

    const float rowRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;

    // --- レールの選択 ---
    char preview[96];
    std::snprintf(preview, sizeof(preview), "Rail %d  [%s] %.1fm", currentRail_, typeText(rail), rail.GetLength());
    int requestedRail = -1;
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
            if ( ImGui::Selectable(label, i == currentRail_) ) { requestedRail = i; }
            ImGui::EndDisabled();
            if ( i == currentRail_ ) { ImGui::SetItemDefaultFocus(); }
        }
        ImGui::EndCombo();
    }
    // 前後のレールへ（使えないレールは飛ばす）
    auto stepRail = [&](int direction) -> int {
        for ( int step = 1; step <= railCount; ++step ) {
            int candidate = ( ( currentRail_ + direction * step ) % railCount + railCount ) % railCount;
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
        focusPending_ = true;
        focusRail_ = currentRail_;
        focusDist_ = std::clamp(viewCenterDist_, 0.0f, rail.GetLength());
        focusHeight_ = 1.0f;
    }
    SameLineIfFits("選択を連動", ImGui::GetFrameHeight(), rowRight);
    ImGui::Checkbox("選択を連動", &followEditorRail_);
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

    if ( requestedRail >= 0 ) { SelectRail(context, requestedRail); }
#else
    ( void ) context;
#endif
}

// ----------------------------------------------------------------
//  表示の設定（横の位置・拡大・段数）
// ----------------------------------------------------------------
void RailStripPanel::DrawViewOptions(){
#ifdef USE_IMGUI
    const float rowRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    ImGui::Separator();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("横の位置");
    const char* sideNames[5] = { "-2", "-1", "中心", "+1", "+2" };
    for ( int i = 0; i < 5; ++i ) {
        int side = i - 2;
        ImGui::SameLine();
        bool chosen = ( sideLayer_ == side );
        if ( chosen ) { ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.75f, 1.0f)); }
        ImGui::PushID(i);
        if ( ImGui::Button(sideNames[i], ImVec2(40.0f, 0.0f)) ) {
            sideLayer_ = side;
            stroke_ = StrokeKind::None;
        }
        ImGui::PopID();
        if ( chosen ) { ImGui::PopStyleColor(); }
        if ( ImGui::IsItemHovered() ) {
            ImGui::SetTooltip("ブロックを置く道幅方向の位置（1m刻み）。\n"
                "「中心」に置いたブロックだけが乗れる/ぶつかる。脇（±1, ±2）は当たらない飾りになる");
        }
    }
    SameLineIfFits("他の位置も薄く表示", ImGui::GetFrameHeight(), rowRight);
    ImGui::Checkbox("他の位置も薄く表示", &showOtherSides_);
    SameLineIfFits("拡大", 100.0f, rowRight);
    ImGui::SetNextItemWidth(100.0f);
    ImGui::SliderFloat("拡大", &cellPx_, 12.0f, 56.0f, "%.0f");
    SameLineIfFits("全体", 0.0f, rowRight);
    if ( ImGui::Button("全体") ) { fitRequested_ = true; }
    if ( ImGui::IsItemHovered() ) { ImGui::SetTooltip("レール全体が見える大きさにする"); }
    SameLineIfFits("段数", 80.0f, rowRight);
    ImGui::SetNextItemWidth(80.0f);
    ImGui::SliderInt("段数", &visibleLevels_, 4, kMaxLevels);
#endif
}

// ----------------------------------------------------------------
//  道具の選択・置く物の種類・元に戻す
// ----------------------------------------------------------------
void RailStripPanel::DrawToolbar(const Context& context){
#ifdef USE_IMGUI
    ImGui::Separator();
    ImGui::BeginDisabled(!context.editable);

    const float rowRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    const float radioExtra = ImGui::GetFrameHeight(); // ラジオボタンの丸の分

    int toolIndex = ( int ) tool_;
    ImGui::RadioButton("ブロック (B)", &toolIndex, 0);
    SameLineIfFits("敵 (E)", radioExtra, rowRight);
    ImGui::RadioButton("敵 (E)", &toolIndex, 1);
    SameLineIfFits("コイン (C)", radioExtra, rowRight);
    ImGui::RadioButton("コイン (C)", &toolIndex, 2);
    if ( toolIndex != ( int ) tool_ ) {
        tool_ = ( Tool ) toolIndex;
        CancelInteractions();
    }

    // --- 置く物の種類 ---
    if ( tool_ == Tool::Block ) {
        const int paintType = context.railEditor->GetBlockPaintType();
        for ( int type = 0; type < kBlockTypeCount; ++type ) {
            if ( type > 0 ) { SameLineIfFits(kBlockTypeNames[type], 0.0f, rowRight); }
            const bool chosen = ( paintType == type );
            ImVec4 color = ImGui::ColorConvertU32ToFloat4(BlockColor(type, 255));
            // 選んでいる種類はそのままの色、他は暗くして並べる（文字は色に合わせて読みやすい方）
            float dim = chosen ? 1.0f : 0.45f;
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(color.x * dim, color.y * dim, color.z * dim, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x * 0.85f, color.y * 0.85f, color.z * 0.85f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
            ImGui::PushStyleColor(ImGuiCol_Text, chosen ? ImVec4(0.08f, 0.08f, 0.08f, 1.0f) : ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
            ImGui::PushID(type);
            if ( ImGui::Button(kBlockTypeNames[type]) ) { context.railEditor->SetBlockPaintType(type); }
            ImGui::PopID();
            ImGui::PopStyleColor(4);
        }
    } else if ( tool_ == Tool::Enemy ) {
        const EnemyType types[3] = { EnemyType::Zako, EnemyType::Strong, EnemyType::Air };
        for ( int i = 0; i < 3; ++i ) {
            if ( i > 0 ) { SameLineIfFits(EnemyEditor::GetTypeName(types[i]), 0.0f, rowRight); }
            const bool chosen = ( enemyType_ == types[i] );
            ImVec4 color = ImGui::ColorConvertU32ToFloat4(EnemyColor(types[i], 255));
            float dim = chosen ? 1.0f : 0.45f;
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(color.x * dim, color.y * dim, color.z * dim, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x * 0.85f, color.y * 0.85f, color.z * 0.85f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
            ImGui::PushID(i);
            if ( ImGui::Button(EnemyEditor::GetTypeName(types[i])) ) { enemyType_ = types[i]; }
            ImGui::PopID();
            ImGui::PopStyleColor(3);
        }
    } else {
        ImGui::TextDisabled("高さはマウスの位置で決まります");
    }

    // 元に戻す／やり直し。ブロックとコインはレールの履歴、敵は敵の履歴（道具に合わせて切り替わる）
    ImGui::BeginGroup();
    if ( tool_ == Tool::Enemy ) {
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
        ImGui::SetTooltip(tool_ == Tool::Enemy
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
    ( void ) context;
#endif
}

// ----------------------------------------------------------------
//  展開図（横スクロールできるキャンバス）
// ----------------------------------------------------------------
void RailStripPanel::DrawCanvas(const Context& context){
#ifdef USE_IMGUI
    const auto& rails = *context.rails;
    const SplineRail& rail = rails[currentRail_];
    ImGuiIO& io = ImGui::GetIO();

    // --- 寸法を決める ---
    Layout layout;
    layout.railLength = rail.GetLength();
    layout.lastCell   = ( std::max )( 0, ( int ) std::floor(layout.railLength + 0.001f) );
    // 「全体」：レール全体が今の表示幅に入る拡大率にする（幅は前のフレームの値を使う）
    if ( fitRequested_ ) {
        fitRequested_ = false;
        if ( canvasViewWidth_ > kPadLeft + kPadRight + 40.0f ) {
            cellPx_ = ( canvasViewWidth_ - kPadLeft - kPadRight ) / ( float ) ( layout.lastCell + 1 );
        }
        scrollResetPending_ = true;
        scrollToDist_ = -1.0f;
    }
    cellPx_ = std::clamp(cellPx_, 12.0f, 56.0f);
    layout.cellPx = cellPx_;
    // 置いてある物が隠れないよう、一番高い物までは必ず見せる（ブロック・浮いている敵・高い所のコイン）
    int highestLevel = 0;
    for ( const BlockData& block : context.railEditor->GetBlocks() ) {
        if ( block.rail == currentRail_ ) { highestLevel = ( std::max )( highestLevel, block.level + 1 ); }
    }
    for ( const EnemySpawnData& spawn : context.enemyEditor->GetSpawnDatas() ) {
        if ( spawn.railIndex != currentRail_ ) continue;
        highestLevel = ( std::max )( highestLevel, ( int ) std::ceil(EnemyCenterHeight(spawn)) );
    }
    for ( const CoinData& coin : context.railEditor->GetCoins() ) {
        if ( coin.rail == currentRail_ ) { highestLevel = ( std::max )( highestLevel, ( int ) std::ceil(coin.height + 0.25f) ); }
    }
    int levels = std::clamp(( std::max )( visibleLevels_, highestLevel ), 1, kMaxLevels);
    // 操作の途中は段数を変えない。段数が変わるとマス目全体が上下にずれ、止まっているマウスの下へ別のマスが来る
    //   （一番上のブロックを消した瞬間に段数が減って、1段下も続けて消えてしまう等）
    const bool interacting = ( stroke_ != StrokeKind::None ) || ( enemyDrag_ != EnemyDrag::None ) || ( coinDragIndex_ >= 0 );
    if ( interacting && lastLevels_ > 0 ) { levels = lastLevels_; }
    lastLevels_ = levels;
    layout.levels       = levels;
    layout.padLeft      = kPadLeft;
    layout.rulerHeight  = kRulerHeight;
    layout.groundHeight = kGroundHeight;
    layout.infoHeight   = kInfoHeight;
    layout.width  = kPadLeft + ( float ) ( layout.lastCell + 1 ) * layout.cellPx + kPadRight;
    layout.height = kRulerHeight + ( float ) layout.levels * layout.cellPx + kGroundHeight + kInfoHeight;

    const float statusHeight = ImGui::GetTextLineHeightWithSpacing() + 2.0f;
    const float childHeight = ( std::max )( 120.0f, ImGui::GetContentRegionAvail().y - statusHeight );
    // 中身の大きさを先に教えておく。教えないと、拡大した直後のスクロールが「拡大前の大きさ」で
    // 端に止められて、マウスの下の場所がずれていく
    ImGui::SetNextWindowContentSize(ImVec2(layout.width, layout.height));
    ImGui::BeginChild("##stripScroll", ImVec2(0.0f, childHeight), ImGuiChildFlags_Borders,
        ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove);

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    layout.originX = origin.x;
    layout.originY = origin.y;
    layout.groundY = origin.y + kRulerHeight + ( float ) layout.levels * layout.cellPx;
    layout.infoTop = layout.groundY + kGroundHeight;

    // キャンバス全体を1つのボタンにして、クリックでウィンドウが動かないようにする
    ImGui::InvisibleButton("##stripCanvas", ImVec2(layout.width, layout.height),
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();
    canvasHovered_ = hovered;
    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY); // ホイールは拡大に使う（親の窓をスクロールさせない）

    canvasViewWidth_ = ImGui::GetWindowSize().x;
    if ( scrollToDist_ >= 0.0f ) {
        // 指定の距離が表示の中央に来る位置へ（範囲の外は ImGui が端で止める）。道（一番下）も見せる
        const float contentX = kPadLeft + ( scrollToDist_ + 0.5f ) * layout.cellPx;
        ImGui::SetScrollX(( std::max )( 0.0f, contentX - canvasViewWidth_ * 0.5f ));
        ImGui::SetScrollY(layout.height);
        scrollToDist_ = -1.0f;
        scrollResetPending_ = false;
    } else if ( scrollResetPending_ ) {
        // レールを開いた直後：先頭（距離0）と道（一番下）が見える位置へ
        ImGui::SetScrollX(0.0f);
        ImGui::SetScrollY(layout.height);
        scrollResetPending_ = false;
    }

    // 見えている範囲の中央の距離（「カメラをここへ」ボタン用）
    {
        const ImVec2 childPos = ImGui::GetWindowPos();
        viewCenterDist_ = XToDist(layout, childPos.x + canvasViewWidth_ * 0.5f);
    }
    if ( hovered ) {
        hoverValid_  = true;
        hoverDist_   = XToDist(layout, io.MousePos.x);
        hoverHeight_ = YToHeight(layout, io.MousePos.y);
    }

    // --- 描画（奥から順に）---
    pendingRailSelect_ = -1;
    pendingRailDist_   = -1.0f;
    overLinkLabel_     = false;
    DrawBackdrop(context, layout);
    DrawRailInfo(context, layout);
    DrawMarkers(context, layout);
    DrawBlocks(context, layout);
    DrawCoins(context, layout);
    DrawEnemies(context, layout);

    // --- 操作 ---
    //   レールのつながりのラベルの上では道具を動かさない（ラベルのクリックでレールを移る時に、
    //   前のレールへブロックや敵が置かれてしまわないように）
    const bool toolHovered = hovered && !overLinkLabel_;
    if ( context.editable ) {
        switch ( tool_ ) {
        case Tool::Block: UpdateBlockTool(context, layout, toolHovered); break;
        case Tool::Enemy: UpdateEnemyTool(context, layout, toolHovered); break;
        case Tool::Coin:  UpdateCoinTool(context, layout, toolHovered);  break;
        }
    }
    DrawEnemyContextMenu(context);
    DrawLevelLabels(layout);

    // 指している距離を Game View にも縦線で出す（3Dのどこを指しているかの目印）
    if ( hoverValid_ && hoverDist_ >= 0.0f && hoverDist_ <= layout.railLength ) {
        Vector3 railPoint = rail.GetPositionByDistance(hoverDist_);
        DebugDraw::GetInstance()->Line(railPoint, { railPoint.x, railPoint.y + ( float ) layout.levels, railPoint.z },
                                       { 1.0f, 1.0f, 1.0f, 0.55f });
    }

    // --- キー操作（展開図の上にマウスがある時だけ）---
    // 敵の道具の間：展開図の上での Ctrl+Z / Ctrl+Y は敵の履歴へ回す（レールの履歴は動かさない）。
    //   キーを押したフレームだけでなく、マウスが乗っている間ずっと譲る
    //   （レール側は DirectInput、こちらは ImGui でキーを見ていて、押した瞬間のフレームがずれ得るため）
    if ( hovered && context.editable && tool_ == Tool::Enemy ) {
        context.railEditor->SkipUndoHotkeyThisFrame();
        if ( !io.WantTextInput && io.KeyCtrl && !ImGui::IsMouseDown(ImGuiMouseButton_Left) ) {
            if ( ImGui::IsKeyPressed(ImGuiKey_Z, false) ) { context.enemyEditor->Undo(); CancelInteractions(); }
            if ( ImGui::IsKeyPressed(ImGuiKey_Y, false) ) { context.enemyEditor->Redo(); CancelInteractions(); }
        }
    }
    // Esc：途中の「四角くまとめて」「移動」を、何も適用せずにやめる
    if ( context.editable && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape, false)
        && ( stroke_ == StrokeKind::RectFill || stroke_ == StrokeKind::RectErase || stroke_ == StrokeKind::Move ) ) {
        stroke_ = StrokeKind::None;
        moveIndex_ = -1;
        moveDragged_ = false;
    }
    // 右ボタンを押している間は反応しない（Game View で視点を回しながら E で上昇している時に、
    // マウスが展開図の上へ流れて道具が切り替わらないように）
    if ( hovered && !io.WantTextInput && !io.KeyCtrl && !ImGui::IsMouseDown(ImGuiMouseButton_Right) ) {
        if ( context.editable ) {
            Tool requested = tool_;
            if ( ImGui::IsKeyPressed(ImGuiKey_B, false) ) { requested = Tool::Block; }
            if ( ImGui::IsKeyPressed(ImGuiKey_E, false) ) { requested = Tool::Enemy; }
            if ( ImGui::IsKeyPressed(ImGuiKey_C, false) ) { requested = Tool::Coin; }
            if ( requested != tool_ ) { tool_ = requested; CancelInteractions(); }

            // 数字キー：置く物の種類（ブロック 1〜9 / 敵 1〜3）
            for ( int number = 0; number < 9; ++number ) {
                if ( !ImGui::IsKeyPressed(( ImGuiKey ) ( ImGuiKey_1 + number ), false) ) continue;
                if ( tool_ == Tool::Block ) {
                    context.railEditor->SetBlockPaintType(number);
                } else if ( tool_ == Tool::Enemy && number < 3 ) {
                    const EnemyType types[3] = { EnemyType::Zako, EnemyType::Strong, EnemyType::Air };
                    enemyType_ = types[number];
                }
            }
            // Z / X：ブロックを置く横の位置を1つずらす
            if ( tool_ == Tool::Block ) {
                int side = sideLayer_;
                if ( ImGui::IsKeyPressed(ImGuiKey_Z, false) ) { --side; }
                if ( ImGui::IsKeyPressed(ImGuiKey_X, false) ) { ++side; }
                side = std::clamp(side, -2, 2);
                if ( side != sideLayer_ ) { sideLayer_ = side; stroke_ = StrokeKind::None; moveIndex_ = -1; }
            }
            // F：カメラを指している場所へ（編集中だけ。プレイ中の F は吐き出しに使う）
            if ( ImGui::IsKeyPressed(ImGuiKey_F, false) ) {
                focusPending_ = true;
                focusRail_    = currentRail_;
                focusDist_    = std::clamp(hoverDist_, 0.0f, layout.railLength);
                focusHeight_  = std::clamp(hoverHeight_, 0.0f, ( float ) kMaxLevels);
            }
        }
    }

    // --- 表示の移動と拡大 ---
    //   中ボタンドラッグ＝表示を動かす / ホイール＝マウスの位置を中心に拡大
    if ( hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ) { panning_ = true; }
    if ( panning_ ) {
        if ( ImGui::IsMouseDown(ImGuiMouseButton_Middle) ) {
            ImGui::SetScrollX(ImGui::GetScrollX() - io.MouseDelta.x);
            ImGui::SetScrollY(ImGui::GetScrollY() - io.MouseDelta.y);
        } else {
            panning_ = false;
        }
    }
    if ( hovered && io.MouseWheel != 0.0f ) {
        const float newCellPx = std::clamp(layout.cellPx * ( 1.0f + io.MouseWheel * 0.12f ), 12.0f, 56.0f);
        if ( newCellPx != layout.cellPx ) {
            // マウスの下にある場所が、拡大した後も同じ場所に来るようにスクロールを合わせる（横も縦も）。
            //   縦は道より下を指している時は道の高さを基準にする（道が下へ流れて見えなくならないように）
            const float anchorDist   = XToDist(layout, io.MousePos.x);
            const float anchorHeight = std::clamp(YToHeight(layout, io.MousePos.y), 0.0f, ( float ) layout.levels);
            ImGui::SetScrollX(ImGui::GetScrollX() + ( anchorDist + 0.5f ) * ( newCellPx - layout.cellPx ));
            ImGui::SetScrollY(ImGui::GetScrollY() + ( ( float ) layout.levels - anchorHeight ) * ( newCellPx - layout.cellPx ));
            cellPx_ = newCellPx;
        }
    }

    ImGui::EndChild();

    // --- 下の説明行：いま指している場所 ---
    if ( hoverValid_ ) {
        int level = ( int ) std::floor(hoverHeight_);
        if ( hoverHeight_ >= 0.0f && level < layout.levels ) {
            ImGui::Text("Rail %d ｜ 距離 %.1f m ｜ %d段目（高さ %.2f m）｜ 横の位置 %+d",
                currentRail_, std::clamp(hoverDist_, 0.0f, layout.railLength), level + 1, hoverHeight_, sideLayer_);
        } else {
            ImGui::Text("Rail %d ｜ 距離 %.1f m", currentRail_, std::clamp(hoverDist_, 0.0f, layout.railLength));
        }
    } else {
        ImGui::TextDisabled("ホイール=拡大 / 中ボタンドラッグ=表示を動かす / 指している場所は Game View にも白い線と枠で出ます");
    }

    // レールの切り替えは、このフレームの描画が全部終わってから（描画中の参照を壊さない）
    if ( pendingRailSelect_ >= 0 ) { SelectRail(context, pendingRailSelect_, pendingRailDist_); }
#else
    ( void ) context;
#endif
}

// ----------------------------------------------------------------
//  背景：マス目・目盛り・道
// ----------------------------------------------------------------
void RailStripPanel::DrawBackdrop(const Context& context, const Layout& layout) const{
#ifdef USE_IMGUI
    const SplineRail& rail = ( *context.rails )[currentRail_];
    ImDrawList* draw = ImGui::GetWindowDrawList();

    const float left   = layout.originX;
    const float top    = layout.originY;
    const float right  = left + layout.width;
    const float bottom = top + layout.height;
    const float cellsLeft  = DistToX(layout, -0.5f);
    const float cellsRight = DistToX(layout, ( float ) layout.lastCell + 0.5f);
    const float levelsTop  = HeightToY(layout, ( float ) layout.levels);

    draw->AddRectFilled({ left, top }, { right, bottom }, IM_COL32(26, 28, 34, 255));

    // 段ごとの帯（1段おきに少し明るく）
    for ( int level = 0; level < layout.levels; ++level ) {
        if ( level % 2 == 1 ) continue;
        draw->AddRectFilled({ cellsLeft, HeightToY(layout, ( float ) level + 1.0f) },
                            { cellsRight, HeightToY(layout, ( float ) level) }, IM_COL32(255, 255, 255, 10));
    }
    // レールの外（始点より手前・終点より先）は暗くする
    draw->AddRectFilled({ cellsLeft, levelsTop }, { DistToX(layout, 0.0f), layout.groundY }, IM_COL32(0, 0, 0, 90));
    draw->AddRectFilled({ DistToX(layout, layout.railLength), levelsTop }, { cellsRight, layout.groundY }, IM_COL32(0, 0, 0, 90));

    // マスの区切り線（5mごとに濃く）と目盛り
    for ( int cell = 0; cell <= layout.lastCell + 1; ++cell ) {
        float x = DistToX(layout, ( float ) cell - 0.5f);
        bool strong = ( cell % 5 == 0 );
        draw->AddLine({ x, levelsTop }, { x, layout.groundY },
                      strong ? IM_COL32(255, 255, 255, 46) : IM_COL32(255, 255, 255, 18));
    }
    for ( int level = 0; level <= layout.levels; ++level ) {
        float y = HeightToY(layout, ( float ) level);
        draw->AddLine({ cellsLeft, y }, { cellsRight, y }, IM_COL32(255, 255, 255, 22));
    }
    for ( int cell = 0; cell <= layout.lastCell; ++cell ) {
        bool labelled = ( cell % 5 == 0 ) || ( layout.cellPx >= 34.0f );
        float x = DistToX(layout, ( float ) cell);
        draw->AddLine({ x, top + kRulerHeight - ( cell % 5 == 0 ? 7.0f : 3.0f ) }, { x, top + kRulerHeight },
                      IM_COL32(255, 255, 255, 120));
        if ( labelled ) {
            char text[16];
            std::snprintf(text, sizeof(text), ( cell % 5 == 0 ) ? "%dm" : "%d", cell);
            AddTextCentered(draw, { x, top + 7.0f }, IM_COL32(220, 220, 225, ( cell % 5 == 0 ) ? 255 : 130), text);
        }
    }

    // 道（レールの始点〜終点）。道なしのレールは枠だけにする
    const float groundLeft  = DistToX(layout, 0.0f);
    const float groundRight = DistToX(layout, layout.railLength);
    if ( rail.roadMode == 0 ) {
        draw->AddRectFilled({ groundLeft, layout.groundY }, { groundRight, layout.groundY + kGroundHeight },
                            rail.visible ? IM_COL32(168, 124, 78, 255) : IM_COL32(110, 110, 118, 255));
    } else {
        draw->AddRect({ groundLeft, layout.groundY }, { groundRight, layout.groundY + kGroundHeight },
                      IM_COL32(150, 150, 160, 200));
    }
    draw->AddLine({ groundLeft, layout.groundY }, { groundRight, layout.groundY }, IM_COL32(255, 235, 200, 220), 2.0f);

    // 穴の区間：道をくり抜いて赤い斜線を入れる
    for ( const SplineRail::HoleInterval& hole : rail.GetHoleIntervals() ) {
        float holeLeft  = DistToX(layout, std::clamp(hole.d0, 0.0f, layout.railLength));
        float holeRight = DistToX(layout, std::clamp(hole.d1, 0.0f, layout.railLength));
        if ( holeRight - holeLeft < 1.0f ) continue;
        draw->AddRectFilled({ holeLeft, layout.groundY - 1.0f }, { holeRight, layout.groundY + kGroundHeight },
                            IM_COL32(40, 12, 14, 255));
        for ( float x = holeLeft; x < holeRight; x += 8.0f ) {
            float x1 = ( std::min )( x + kGroundHeight, holeRight );
            draw->AddLine({ x, layout.groundY + kGroundHeight }, { x1, layout.groundY + kGroundHeight - ( x1 - x ) },
                          IM_COL32(255, 80, 70, 200));
        }
        AddTextCentered(draw, { ( holeLeft + holeRight ) * 0.5f, layout.groundY - 9.0f }, IM_COL32(255, 120, 110, 255), "穴");
    }
#else
    ( void ) context; ( void ) layout;
#endif
}

// ----------------------------------------------------------------
//  下の情報帯：道の高さの変化・他のレールとのつながり
// ----------------------------------------------------------------
void RailStripPanel::DrawRailInfo(const Context& context, const Layout& layout){
#ifdef USE_IMGUI
    const auto& rails = *context.rails;
    const SplineRail& rail = rails[currentRail_];
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();

    const float bandTop    = layout.infoTop + 4.0f;
    const float bandBottom = layout.infoTop + kInfoHeight - 18.0f;

    // --- 道の高さの変化（坂が分かるように、ワールドの高さを折れ線で）---
    {
        const int sampleCount = std::clamp(( int ) ( layout.railLength / 0.5f ) + 1, 2, 400);
        std::vector<float> heights(sampleCount);
        float lowest = 1e9f, highest = -1e9f;
        for ( int i = 0; i < sampleCount; ++i ) {
            float dist = layout.railLength * ( float ) i / ( float ) ( sampleCount - 1 );
            // 動くレールの今のずれ（プレビュー再生中）は含めず、基準位置の高さを見せる
            heights[i] = rail.GetPositionByDistance(dist).y - rail.animOffset.y;
            lowest  = ( std::min )( lowest, heights[i] );
            highest = ( std::max )( highest, heights[i] );
        }
        const float range = highest - lowest;
        std::vector<ImVec2> points(sampleCount);
        for ( int i = 0; i < sampleCount; ++i ) {
            float dist = layout.railLength * ( float ) i / ( float ) ( sampleCount - 1 );
            float t = ( range > 0.05f ) ? ( heights[i] - lowest ) / range : 0.5f;
            points[i] = { DistToX(layout, dist), bandBottom - t * ( bandBottom - bandTop ) };
        }
        draw->AddPolyline(points.data(), sampleCount, IM_COL32(120, 200, 255, 220), ImDrawFlags_None, 1.5f);
        char text[64];
        if ( range > 0.05f ) { std::snprintf(text, sizeof(text), "道の高さ %.1f〜%.1f m", lowest, highest); }
        else                 { std::snprintf(text, sizeof(text), "道の高さ %.1f m（平ら）", lowest); }
        draw->AddText({ DistToX(layout, 0.0f) + 4.0f, layout.infoTop + kInfoHeight - 17.0f },
                      IM_COL32(150, 200, 235, 255), text);
    }

    // --- 他のレールとのつながり（クリックでそのレールを開く）---
    //   arrivalDist は、相手のレールへ移った時に最初に見せる距離（つながっている場所）
    auto linkLabel = [&](float centerX, float y, int targetRail, float arrivalDist,
                         const char* prefix, const char* suffix) {
        if ( targetRail < 0 || targetRail >= ( int ) rails.size() || !RailUsable(rails[targetRail]) ) return;
        char text[32];
        std::snprintf(text, sizeof(text), "%sR%d%s", prefix, targetRail, suffix);
        ImVec2 size = ImGui::CalcTextSize(text);
        ImVec2 boxMin = { centerX - size.x * 0.5f - 4.0f, y };
        ImVec2 boxMax = { centerX + size.x * 0.5f + 4.0f, y + size.y + 2.0f };
        // 何かを塗っている/動かしている途中はラベルに反応しない（ドラッグがラベルの上を通っただけでレールが移らないように）
        const bool idle = ( stroke_ == StrokeKind::None ) && ( enemyDrag_ == EnemyDrag::None )
                       && ( coinDragIndex_ < 0 ) && !panning_;
        bool over = idle && canvasHovered_
            && io.MousePos.x >= boxMin.x && io.MousePos.x <= boxMax.x
            && io.MousePos.y >= boxMin.y && io.MousePos.y <= boxMax.y;
        if ( over ) { overLinkLabel_ = true; }
        draw->AddRectFilled(boxMin, boxMax, over ? IM_COL32(60, 110, 170, 255) : IM_COL32(44, 60, 84, 255), 3.0f);
        draw->AddText({ boxMin.x + 4.0f, boxMin.y + 1.0f }, IM_COL32(230, 240, 255, 255), text);
        if ( over ) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            ImGui::SetTooltip("クリックで Rail %d を開く", targetRail);
            if ( ImGui::IsMouseClicked(ImGuiMouseButton_Left) ) {
                pendingRailSelect_ = targetRail;
                pendingRailDist_   = arrivalDist;
            }
        }
    };
    // 相手のレールの「どちらの端につながっているか」から、着いた先の距離を決める
    auto arrivalOf = [&](int targetRail, bool connectsToFront) -> float {
        if ( targetRail < 0 || targetRail >= ( int ) rails.size() ) return 0.0f;
        return connectsToFront ? 0.0f : rails[targetRail].GetLength();
    };
    const float linkY = layout.groundY + kGroundHeight + 2.0f;
    // 始点・終点でつながっているレール
    linkLabel(DistToX(layout, 0.0f) - 2.0f - kPadLeft * 0.5f + 6.0f, layout.groundY - 8.0f, rail.frontConnIndex,
              arrivalOf(rail.frontConnIndex, rail.frontConnToFront), "<", "");
    linkLabel(DistToX(layout, layout.railLength) + kPadRight * 0.5f, layout.groundY - 8.0f, rail.backConnIndex,
              arrivalOf(rail.backConnIndex, rail.backConnToFront), "", ">");
    // 途中の分かれ道
    for ( const SplineRail::BranchPoint& branch : rail.branchPoints ) {
        float x = DistToX(layout, std::clamp(branch.distance, 0.0f, layout.railLength));
        draw->AddTriangleFilled({ x, layout.groundY + kGroundHeight - 1.0f },
                                { x - 5.0f, layout.groundY + 2.0f }, { x + 5.0f, layout.groundY + 2.0f },
                                IM_COL32(120, 200, 255, 255));
        linkLabel(x, linkY, branch.targetRail, branch.targetDist, "→", "");
    }
#else
    ( void ) context; ( void ) layout;
#endif
}

// ----------------------------------------------------------------
//  スタート・ゴール・プレイヤー
// ----------------------------------------------------------------
void RailStripPanel::DrawMarkers(const Context& context, const Layout& layout) const{
#ifdef USE_IMGUI
    ImDrawList* draw = ImGui::GetWindowDrawList();
    auto flag = [&](float dist, ImU32 color, const char* text) {
        float x = DistToX(layout, std::clamp(dist, 0.0f, layout.railLength));
        float poleTop = HeightToY(layout, 1.6f);
        draw->AddLine({ x, layout.groundY }, { x, poleTop }, color, 2.0f);
        draw->AddTriangleFilled({ x, poleTop }, { x + 14.0f, poleTop + 6.0f }, { x, poleTop + 12.0f }, color);
        draw->AddText({ x + 3.0f, poleTop - 15.0f }, color, text);
    };
    if ( context.startRail == currentRail_ ) { flag(context.startDist, IM_COL32(110, 230, 120, 255), "スタート"); }
    if ( context.goalRail  == currentRail_ ) { flag(context.goalDist,  IM_COL32(255, 210, 80, 255),  "ゴール"); }

    // プレイ中のプレイヤー位置（このレールに乗っている時だけ）
    if ( context.hasPlayer && context.playerRail == currentRail_ ) {
        const SplineRail& rail = ( *context.rails )[currentRail_];
        float dist = rail.GetClosestDistance(context.playerPos);
        float height = context.playerPos.y - rail.GetPositionByDistance(dist).y;
        float x = DistToX(layout, dist);
        float footY = HeightToY(layout, height);
        float headY = HeightToY(layout, height + 1.0f);
        float halfWidth = 0.35f * layout.cellPx;
        draw->AddRectFilled({ x - halfWidth, headY }, { x + halfWidth, footY }, IM_COL32(90, 220, 110, 200), halfWidth);
        draw->AddText({ x - 6.0f, headY - 15.0f }, IM_COL32(150, 255, 170, 255), "P");
    }
#else
    ( void ) context; ( void ) layout;
#endif
}

// ----------------------------------------------------------------
//  ブロック
// ----------------------------------------------------------------
void RailStripPanel::DrawBlocks(const Context& context, const Layout& layout) const{
#ifdef USE_IMGUI
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const auto& blocks = context.railEditor->GetBlocks();

    // 他の横位置のブロックを先に薄く描き、編集中の横位置のブロックを上に描く
    for ( int pass = 0; pass < 2; ++pass ) {
        for ( int blockIndex = 0; blockIndex < ( int ) blocks.size(); ++blockIndex ) {
            const BlockData& block = blocks[blockIndex];
            if ( block.rail != currentRail_ ) continue;
            if ( block.level < 0 || block.level >= layout.levels ) continue;
            const bool onLayer = std::abs(block.side - ( float ) sideLayer_) < 0.5f;
            if ( ( pass == 1 ) != onLayer ) continue;
            if ( !onLayer && !showOtherSides_ ) continue;

            // 移動中のブロックは元の場所に薄く残す（どこから動かしているかが分かる）
            const bool moving = ( stroke_ == StrokeKind::Move && blockIndex == moveIndex_ && moveDragged_ );
            const int alpha = ( !onLayer ) ? 60 : ( moving ? 90 : 255 );
            const float half = BlockSystem::VisualHalfAlong(block.type);
            const float shownDist = ShownDist(block.dist, layout.railLength);
            const float x0 = DistToX(layout, shownDist - half) + 1.0f;
            const float x1 = DistToX(layout, shownDist + half) - 1.0f;
            const float yBottom = HeightToY(layout, ( float ) block.level) - 1.0f;
            const float yTop    = HeightToY(layout, ( float ) block.level + 1.0f) + 1.0f;
            const ImU32 fill    = BlockColor(block.type, alpha);
            const ImU32 outline = IM_COL32(30, 22, 14, alpha);

            if ( BlockSystem::IsSlope(block.type) ) {
                // 斜面：高い側は隣のブロックの方（実際の向きは BlockSystem が決めたものを使う）
                int ascend = context.blockSystem
                    ? context.blockSystem->AscendAt(block.rail, block.dist, block.level, block.side) : +1;
                ImVec2 low  = ( ascend > 0 ) ? ImVec2(x0, yBottom) : ImVec2(x1, yBottom);
                ImVec2 foot = ( ascend > 0 ) ? ImVec2(x1, yBottom) : ImVec2(x0, yBottom);
                ImVec2 peak = ( ascend > 0 ) ? ImVec2(x1, yTop)    : ImVec2(x0, yTop);
                draw->AddTriangleFilled(low, foot, peak, fill);
                draw->AddTriangle(low, foot, peak, outline);
            } else if ( block.type == BlockSystem::kTypeCloud ) {
                // すり抜け床：マスの上端の薄い板
                float plateBottom = yTop + ( yBottom - yTop ) * 0.3f;
                draw->AddRectFilled({ x0, yTop }, { x1, plateBottom }, fill, 3.0f);
                draw->AddRect({ x0, yTop }, { x1, plateBottom }, outline, 3.0f);
            } else {
                draw->AddRectFilled({ x0, yTop }, { x1, yBottom }, fill, 2.0f);
                draw->AddRect({ x0, yTop }, { x1, yBottom }, outline, 2.0f);
                if ( onLayer && layout.cellPx >= 22.0f ) {
                    const char* mark = nullptr;
                    if ( block.type == BlockSystem::kTypeHatena ) { mark = "?"; }
                    if ( block.type == BlockSystem::kTypeSpring ) { mark = "^"; }
                    if ( mark ) {
                        AddTextCentered(draw, { ( x0 + x1 ) * 0.5f, ( yTop + yBottom ) * 0.5f }, IM_COL32(40, 28, 10, 255), mark);
                    }
                }
            }
        }
    }
#else
    ( void ) context; ( void ) layout;
#endif
}

// ----------------------------------------------------------------
//  コイン
// ----------------------------------------------------------------
void RailStripPanel::DrawCoins(const Context& context, const Layout& layout) const{
#ifdef USE_IMGUI
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const auto& coins = context.railEditor->GetCoins();
    const float radius = ( std::max )( 4.0f, 0.2f * layout.cellPx );
    for ( int i = 0; i < ( int ) coins.size(); ++i ) {
        // ドラッグ中のコインは、移動先の位置に描く
        const CoinData& coin = ( i == coinDragIndex_ ) ? coinDragData_ : coins[i];
        if ( coin.rail != currentRail_ ) continue;
        ImVec2 center = { DistToX(layout, ShownDist(coin.dist, layout.railLength)), HeightToY(layout, coin.height) };
        draw->AddCircleFilled(center, radius, IM_COL32(255, 214, 60, 255), 16);
        draw->AddCircle(center, radius, IM_COL32(150, 100, 10, 255), 16, 1.5f);
    }
#else
    ( void ) context; ( void ) layout;
#endif
}

// ----------------------------------------------------------------
//  敵（体・番号と名前・動ける範囲）
// ----------------------------------------------------------------
void RailStripPanel::DrawEnemies(const Context& context, const Layout& layout) const{
#ifdef USE_IMGUI
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const auto& spawns = context.enemyEditor->GetSpawnDatas();
    const int selected = context.enemyEditor->GetSelectedEntry();
    const int listHovered = context.enemyEditor->GetHoveredEntry();
    const float rangeY = layout.groundY + kGroundHeight * 0.5f;

    for ( int i = 0; i < ( int ) spawns.size(); ++i ) {
        const EnemySpawnData& spawn = spawns[i];
        if ( spawn.railIndex != currentRail_ ) continue;
        const bool isSelected = ( i == selected );
        const float shownDist = ShownDist(spawn.distance, layout.railLength);
        const float x = DistToX(layout, shownDist);
        const float centerY = HeightToY(layout, EnemyCenterHeight(spawn));
        const float radius = ( std::max )( 7.0f, Enemy::TypeSpecOf(spawn.type).bodyRadius * spawn.scale * layout.cellPx * 1.15f );
        const ImU32 color = EnemyColor(spawn.type, 255);

        // 動ける範囲（橙）。範囲を決めていない巡回はレール全体を細い線で
        if ( EnemyHasRange(spawn) ) {
            float rangeMin = ( spawn.patrolMin >= 0.0f ) ? ( std::min )( spawn.patrolMin, layout.railLength ) : 0.0f;
            float rangeMax = ( spawn.patrolMax >= 0.0f ) ? std::clamp(spawn.patrolMax, rangeMin, layout.railLength) : layout.railLength;
            float xMin = DistToX(layout, rangeMin), xMax = DistToX(layout, rangeMax);
            draw->AddLine({ xMin, rangeY }, { xMax, rangeY }, IM_COL32(255, 156, 40, isSelected ? 255 : 170), isSelected ? 4.0f : 3.0f);
            const float handle = isSelected ? 6.0f : 4.5f;
            draw->AddRectFilled({ xMin - handle, rangeY - handle }, { xMin + handle, rangeY + handle }, IM_COL32(255, 156, 40, 255));
            draw->AddRectFilled({ xMax - handle, rangeY - handle }, { xMax + handle, rangeY + handle }, IM_COL32(255, 156, 40, 255));
        } else if ( spawn.patrol ) {
            draw->AddLine({ DistToX(layout, 0.0f), rangeY }, { DistToX(layout, layout.railLength), rangeY },
                          IM_COL32(255, 156, 40, 90), 2.0f);
        }
        // 追いかける敵：気づく距離を道の上の赤い線で
        if ( spawn.chaseRange > 0.0f ) {
            float xMin = DistToX(layout, ( std::max )( shownDist - spawn.chaseRange, 0.0f ));
            float xMax = DistToX(layout, ( std::min )( shownDist + spawn.chaseRange, layout.railLength ));
            draw->AddLine({ xMin, layout.groundY - 3.0f }, { xMax, layout.groundY - 3.0f }, IM_COL32(255, 80, 80, 150), 2.0f);
        }

        // 浮いている敵は、足元の位置が分かるよう道まで点線を下ろす
        if ( Enemy::HoverOf(spawn) > 0.05f ) {
            for ( float y = centerY + radius; y < layout.groundY; y += 6.0f ) {
                draw->AddLine({ x, y }, { x, ( std::min )( y + 3.0f, layout.groundY ) }, IM_COL32(255, 255, 255, 90));
            }
        }

        // 体
        draw->AddCircleFilled({ x, centerY }, radius, color, 20);
        if ( spawn.type == EnemyType::Air ) {
            // フワリン：左右に羽
            draw->AddTriangleFilled({ x - radius, centerY }, { x - radius * 1.9f, centerY - radius * 0.7f },
                                    { x - radius * 1.5f, centerY + radius * 0.3f }, color);
            draw->AddTriangleFilled({ x + radius, centerY }, { x + radius * 1.9f, centerY - radius * 0.7f },
                                    { x + radius * 1.5f, centerY + radius * 0.3f }, color);
        } else if ( spawn.type == EnemyType::Strong ) {
            // カミバナ：茎
            draw->AddLine({ x, centerY + radius }, { x, layout.groundY }, IM_COL32(90, 190, 90, 255), 3.0f);
        }
        // 動く敵は最初に進む向きを矢印で
        if ( spawn.patrol || spawn.chaseRange > 0.0f ) {
            float direction = ( spawn.startDir < 0 ) ? -1.0f : 1.0f;
            float tipX = x + direction * ( radius + 9.0f );
            draw->AddTriangleFilled({ tipX, centerY }, { tipX - direction * 7.0f, centerY - 5.0f },
                                    { tipX - direction * 7.0f, centerY + 5.0f }, IM_COL32(255, 255, 255, 230));
        }
        // 選択中＝白い太枠 / 敵エディタの一覧で指している敵＝黄色い枠
        if ( isSelected )            { draw->AddCircle({ x, centerY }, radius + 2.5f, IM_COL32(255, 255, 255, 255), 24, 2.5f); }
        else if ( i == listHovered ) { draw->AddCircle({ x, centerY }, radius + 2.5f, IM_COL32(255, 240, 80, 255), 24, 2.0f); }
        else                         { draw->AddCircle({ x, centerY }, radius, IM_COL32(20, 14, 14, 255), 20, 1.2f); }

        // 番号と名前（敵エディタの一覧と同じ番号）
        char label[96];
        if ( spawn.name.empty() ) { std::snprintf(label, sizeof(label), "#%02d", i); }
        else                      { std::snprintf(label, sizeof(label), "#%02d %s", i, spawn.name.c_str()); }
        ImVec2 size = ImGui::CalcTextSize(label);
        ImVec2 textPos = { x - size.x * 0.5f, centerY - radius - size.y - 3.0f };
        draw->AddRectFilled({ textPos.x - 2.0f, textPos.y }, { textPos.x + size.x + 2.0f, textPos.y + size.y },
                            isSelected ? IM_COL32(20, 70, 120, 230) : IM_COL32(0, 0, 0, 150), 2.0f);
        draw->AddText(textPos, IM_COL32(255, 255, 255, 255), label);
    }
#else
    ( void ) context; ( void ) layout;
#endif
}

// 段の番号。横へスクロールしても見えるよう、表示の左端に固定して描く
void RailStripPanel::DrawLevelLabels(const Layout& layout) const{
#ifdef USE_IMGUI
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float left = ImGui::GetWindowPos().x + 2.0f;
    draw->AddRectFilled({ left, HeightToY(layout, ( float ) layout.levels) }, { left + 24.0f, layout.groundY },
                        IM_COL32(26, 28, 34, 210));
    for ( int level = 0; level < layout.levels; ++level ) {
        char text[8];
        std::snprintf(text, sizeof(text), "%d", level + 1);
        AddTextCentered(draw, { left + 12.0f, HeightToY(layout, ( float ) level + 0.5f) }, IM_COL32(200, 200, 210, 255), text);
    }
#else
    ( void ) layout;
#endif
}

// =====================================================================
//  道具：ブロック
// =====================================================================
void RailStripPanel::UpdateBlockTool(const Context& context, const Layout& layout, bool hovered){
#ifdef USE_IMGUI
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    RailEditor* railEditor = context.railEditor;
    const SplineRail& rail = ( *context.rails )[currentRail_];
    const int paintType = railEditor->GetBlockPaintType();
    // 見えないレール（連結用の骨組み）には新しく置かない。今ある物の移動・塗り替え・削除はできる
    //   （Game View のブロック配置も見えないレールには置かない。同じ決まりにそろえる）
    const bool placementAllowed = rail.visible;

    // マウスが指しているマス
    const float mouseDist = XToDist(layout, io.MousePos.x);
    int cellDist = ( int ) std::floor(mouseDist + 0.5f);
    int level    = ( int ) std::floor(YToHeight(layout, io.MousePos.y));
    const bool inLevels = hovered && io.MousePos.y >= HeightToY(layout, ( float ) layout.levels)
                       && io.MousePos.y < layout.groundY;
    const bool cellValid = inLevels && cellDist >= 0 && cellDist <= layout.lastCell
                        && level >= 0 && level < layout.levels;

    // マスの枠（展開図）と、同じマスの枠（Game View）を描く
    auto drawCellFrame = [&](float dist, int cellLevel, int type, ImU32 color, const Vector4& worldColor, float inflate) {
        float half = BlockSystem::VisualHalfAlong(type);
        draw->AddRect({ DistToX(layout, dist - half), HeightToY(layout, ( float ) cellLevel + 1.0f) },
                      { DistToX(layout, dist + half), HeightToY(layout, ( float ) cellLevel) }, color, 2.0f, 0, 2.5f);
        if ( context.blockSystem ) {
            context.blockSystem->DrawCellGhost(currentRail_, dist, cellLevel, ( float ) sideLayer_, type, worldColor, inflate);
        }
    };

    // =============================================================
    //  何もしていない時：指している場所の予告と、操作の始まり
    // =============================================================
    if ( stroke_ == StrokeKind::None ) {
        if ( !cellValid ) return;
        const auto& blocks = railEditor->GetBlocks();
        const int under = FindBlockAt(context, mouseDist, level);
        // ブロックの「本体」を指しているか。ゆるい斜面は見た目が2mあるが、場所を取るのは中央の1mだけで、
        // 前後の裾のマスには別のブロックを置ける。裾を指した時は「置く」を優先する
        bool underSolid = false;
        if ( under >= 0 ) {
            float gap = std::abs(ShownDist(blocks[under].dist, layout.railLength) - mouseDist);
            underSolid = ( gap <= RailEditor::BlockOccupyHalfOf(blocks[under].type) );
        }
        float placeDist = ( float ) cellDist;
        const bool canPlace = placementAllowed
            && ResolvePlaceDist(context, mouseDist, level, paintType, -1, placeDist);
        const bool onBlock = ( under >= 0 ) && ( underSolid || !canPlace );

        if ( onBlock ) {
            // 置いてあるブロックを指している：別の種類なら塗り替え（黄）、同じなら白い枠だけ
            const BlockData& existing = blocks[under];
            const bool replace = ( existing.type != paintType );
            const float shownDist = ShownDist(existing.dist, layout.railLength);
            ImU32   frameColor = replace ? IM_COL32(255, 230, 64, 255) : IM_COL32(255, 255, 255, 200);
            Vector4 worldColor = replace ? Vector4 { 1.0f, 0.9f, 0.25f, 1.0f } : Vector4 { 1.0f, 1.0f, 1.0f, 0.8f };
            const char* leftAction = replace ? "選んでいる種類に塗り替える" : "（同じ種類です）";
            if ( io.KeyAlt ) {
                frameColor = IM_COL32(120, 220, 255, 255); worldColor = { 0.45f, 0.85f, 1.0f, 1.0f };
                leftAction = "この種類を選ぶ（スポイト）";
            } else if ( io.KeyCtrl ) {
                frameColor = IM_COL32(120, 255, 170, 255); worldColor = { 0.45f, 1.0f, 0.65f, 1.0f };
                leftAction = "ドラッグで移動";
            }
            drawCellFrame(shownDist, existing.level, existing.type, frameColor, worldColor, 0.04f);
            ImGui::SetTooltip("%s（%d段目 / 距離 %.1fm）\n左クリック: %s / 右クリック: 消す\nCtrl+ドラッグ: 移動 / Alt+クリック: この種類を選ぶ",
                kBlockTypeNames[std::clamp(existing.type, 0, kBlockTypeCount - 1)], existing.level + 1, existing.dist,
                leftAction);
        } else if ( canPlace ) {
            const bool decoration = ( sideLayer_ != 0 ); // 道の脇＝当たらない飾り
            drawCellFrame(placeDist, level, paintType,
                decoration ? IM_COL32(90, 204, 255, 255) : IM_COL32(80, 255, 130, 255),
                decoration ? Vector4 { 0.35f, 0.8f, 1.0f, 1.0f } : Vector4 { 0.3f, 1.0f, 0.5f, 1.0f }, 0.0f);
        } else {
            drawCellFrame(( float ) cellDist, level, paintType, IM_COL32(255, 90, 80, 255), { 1.0f, 0.35f, 0.3f, 1.0f }, 0.0f);
            ImGui::SetTooltip(placementAllowed
                ? "ここには置けません（隣のブロックと重なります）"
                : "見えないレールには新しく置けません（今ある物の移動・削除はできます）");
        }

        // 操作の始まり：押した瞬間に何をするかを決める（優先は Alt ＞ Ctrl ＞ Shift ＞ なし）
        const bool leftClicked  = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        const bool rightClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
        if ( !leftClicked && !rightClicked ) return;
        strokeButton_ = leftClicked ? 0 : 1;

        if ( leftClicked && io.KeyAlt ) {
            // スポイト：指しているブロックの種類を、置く種類にする（何も置かない）
            if ( under >= 0 ) { railEditor->SetBlockPaintType(blocks[under].type); }
        } else if ( leftClicked && io.KeyCtrl ) {
            // 移動：指しているブロックをつかむ（実際に動かすのはドラッグしてから）
            if ( under >= 0 ) {
                stroke_      = StrokeKind::Move;
                moveIndex_   = under;
                moveOrig_    = blocks[under];
                moveDist_    = blocks[under].dist;
                moveLevel_   = blocks[under].level;
                moveValid_   = false;
                moveDragged_ = false;
            }
        } else if ( io.KeyShift ) {
            // 四角くまとめて（置く方は、新しく置ける場所でだけ始められる）
            if ( rightClicked || placementAllowed ) {
                stroke_ = leftClicked ? StrokeKind::RectFill : StrokeKind::RectErase;
                rectStartDist_ = rectEndDist_ = cellDist;
                rectStartLevel_ = rectEndLevel_ = level;
            }
        } else if ( rightClicked ) {
            // 最初の1個は、枠で囲んで見せていたブロックそのものを消す（場所から探し直さない）
            stroke_ = StrokeKind::Erase;
            if ( under >= 0 ) { railEditor->RemoveBlockAt(under); }
            strokeLastDist_  = cellDist;
            strokeLastLevel_ = level;
        } else if ( onBlock ) {
            stroke_ = StrokeKind::Replace;
            railEditor->ReplaceBlockTypeAt(under, paintType);
            strokeLastDist_  = cellDist;
            strokeLastLevel_ = level;
        } else if ( canPlace ) {
            // 最初の1個は、予告の枠と同じ場所（半マスずらしを含む）に置く。続きのドラッグはマス目どおり
            stroke_ = StrokeKind::Place;
            railEditor->AddBlock(currentRail_, placeDist, level, ( float ) sideLayer_, paintType);
            strokeLastDist_  = cellDist;
            strokeLastLevel_ = level;
        }
        return;
    }

    // マウスが展開図の外へ出ても、端のマスに寄せて続ける
    cellDist = std::clamp(cellDist, 0, layout.lastCell);
    level    = std::clamp(level, 0, layout.levels - 1);

    // =============================================================
    //  移動の途中
    // =============================================================
    if ( stroke_ == StrokeKind::Move ) {
        const auto& blocks = railEditor->GetBlocks();
        // つかんだ後に配列が変わっていたら（元に戻す等）、別のブロックを動かさないようにやめる
        const bool stillSame = moveIndex_ >= 0 && moveIndex_ < ( int ) blocks.size()
            && blocks[moveIndex_].rail == moveOrig_.rail && blocks[moveIndex_].dist == moveOrig_.dist
            && blocks[moveIndex_].level == moveOrig_.level && blocks[moveIndex_].side == moveOrig_.side
            && blocks[moveIndex_].type == moveOrig_.type;
        if ( !stillSame ) {
            stroke_ = StrokeKind::None;
            moveIndex_ = -1;
            moveDragged_ = false;
            return;
        }
        if ( ImGui::IsMouseDown(ImGuiMouseButton_Left) ) {
            if ( ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f) ) { moveDragged_ = true; }
            if ( moveDragged_ ) {
                const float clampedMouse = std::clamp(mouseDist, 0.0f, layout.railLength);
                float targetDist = ( float ) cellDist;
                moveValid_ = ResolvePlaceDist(context, clampedMouse, level, moveOrig_.type, moveIndex_, targetDist);
                moveDist_  = targetDist;
                moveLevel_ = level;
                // 移動先の予告（緑=置ける / 赤=他のブロックと重なる）と、元の場所からの線
                drawCellFrame(moveDist_, moveLevel_, moveOrig_.type,
                    moveValid_ ? IM_COL32(80, 255, 130, 255) : IM_COL32(255, 90, 80, 255),
                    moveValid_ ? Vector4 { 0.3f, 1.0f, 0.5f, 1.0f } : Vector4 { 1.0f, 0.35f, 0.3f, 1.0f }, 0.0f);
                draw->AddLine(
                    { DistToX(layout, ShownDist(moveOrig_.dist, layout.railLength)), HeightToY(layout, ( float ) moveOrig_.level + 0.5f) },
                    { DistToX(layout, moveDist_), HeightToY(layout, ( float ) moveLevel_ + 0.5f) },
                    IM_COL32(255, 240, 80, 200), 1.5f);
                ImGui::SetTooltip(moveValid_ ? "離すとここへ移動（Esc でやめる）" : "ここへは動かせません（他のブロックと重なります）");
            }
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        } else {
            // 離した：置ける場所なら移動する。置けない場所・クリックだけなら何も変えない
            if ( moveDragged_ && moveValid_ ) {
                if ( railEditor->MoveBlock(moveIndex_, currentRail_, moveDist_, moveLevel_, moveOrig_.side) ) {
                    railEditor->UpdateBlockAnchors(*context.rails); // 離した瞬間に置いたので、錨もその場で入れる
                }
            }
            stroke_ = StrokeKind::None;
            moveIndex_ = -1;
            moveDragged_ = false;
        }
        return;
    }

    // =============================================================
    //  塗っている途中（置く / 塗り替える / 消す / 四角くまとめて）
    // =============================================================
    const bool rectStroke = ( stroke_ == StrokeKind::RectFill || stroke_ == StrokeKind::RectErase );
    if ( ImGui::IsMouseDown(strokeButton_) ) {
        if ( rectStroke ) {
            rectEndDist_  = cellDist;
            rectEndLevel_ = level;
            // まとめて塗る範囲の予告
            int distFrom  = ( std::min )( rectStartDist_, rectEndDist_ ),   distTo  = ( std::max )( rectStartDist_, rectEndDist_ );
            int levelFrom = ( std::min )( rectStartLevel_, rectEndLevel_ ), levelTo = ( std::max )( rectStartLevel_, rectEndLevel_ );
            ImU32 color = ( stroke_ == StrokeKind::RectErase ) ? IM_COL32(255, 90, 80, 255) : IM_COL32(80, 255, 130, 255);
            ImVec2 boxMin = { DistToX(layout, ( float ) distFrom - 0.5f), HeightToY(layout, ( float ) levelTo + 1.0f) };
            ImVec2 boxMax = { DistToX(layout, ( float ) distTo + 0.5f),   HeightToY(layout, ( float ) levelFrom) };
            draw->AddRectFilled(boxMin, boxMax, ( color & 0x00FFFFFF ) | 0x30000000);
            draw->AddRect(boxMin, boxMax, color, 0.0f, 0, 2.0f);
            char text[64];
            std::snprintf(text, sizeof(text), "%d × %d マス（Esc でやめる）", distTo - distFrom + 1, levelTo - levelFrom + 1);
            draw->AddText({ boxMin.x + 4.0f, boxMin.y + 2.0f }, IM_COL32(255, 255, 255, 255), text);
        } else if ( cellDist != strokeLastDist_ || level != strokeLastLevel_ ) {
            // 速く動かしてマスが飛んでも、間のマスを埋めて線がつながるようにする
            const int startDist = strokeLastDist_, startLevel = strokeLastLevel_;
            const int deltaDist = cellDist - startDist, deltaLevel = level - startLevel;
            const int steps = ( std::min )( ( std::max )( std::abs(deltaDist), std::abs(deltaLevel) ), 128 );
            for ( int step = 1; step <= steps; ++step ) {
                float t = ( float ) step / ( float ) steps;
                const int stepDist  = startDist + ( int ) std::lround(( float ) deltaDist * t);
                const int stepLevel = startLevel + ( int ) std::lround(( float ) deltaLevel * t);
                // マウスの真下のマスはマウスの位置で、飛ばしたマスはマスの中心で、消す/塗る相手を探す
                float probe = ( float ) stepDist;
                if ( step == steps ) {
                    probe = std::clamp(mouseDist, ( float ) stepDist - 0.5f, ( float ) stepDist + 0.5f);
                    probe = std::clamp(probe, 0.0f, layout.railLength);
                }
                ApplyBlockStroke(context, stepDist, stepLevel, probe);
            }
        }
    } else {
        // 離した：四角の範囲をまとめて適用
        if ( rectStroke ) {
            int distFrom  = ( std::min )( rectStartDist_, rectEndDist_ ),   distTo  = ( std::max )( rectStartDist_, rectEndDist_ );
            int levelFrom = ( std::min )( rectStartLevel_, rectEndLevel_ ), levelTo = ( std::max )( rectStartLevel_, rectEndLevel_ );
            stroke_ = ( stroke_ == StrokeKind::RectErase ) ? StrokeKind::Erase : StrokeKind::Place;
            for ( int fillLevel = levelFrom; fillLevel <= levelTo; ++fillLevel ) {
                for ( int fillDist = distFrom; fillDist <= distTo; ++fillDist ) {
                    ApplyBlockStroke(context, fillDist, fillLevel, ( float ) fillDist);
                }
            }
            railEditor->UpdateBlockAnchors(*context.rails); // 離した瞬間に置いたので、錨もその場で入れる
        }
        stroke_ = StrokeKind::None;
    }
#else
    ( void ) context; ( void ) layout; ( void ) hovered;
#endif
}

// =====================================================================
//  道具：敵
// =====================================================================
void RailStripPanel::UpdateEnemyTool(const Context& context, const Layout& layout, bool hovered){
#ifdef USE_IMGUI
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    EnemyEditor* enemyEditor = context.enemyEditor;
    const auto& rails = *context.rails;
    const SplineRail& rail = rails[currentRail_];
    auto& spawns = enemyEditor->MutableSpawnDatas();

    const float mouseDist   = XToDist(layout, io.MousePos.x);
    const float mouseHeight = YToHeight(layout, io.MousePos.y);
    // Alt を押している間は細かく（0.5m刻みに吸着しない）
    const float snappedDist = std::clamp(io.KeyAlt ? mouseDist : SnapTo(mouseDist, 0.5f), 0.0f, layout.railLength);
    const bool inEditArea = hovered && io.MousePos.y >= layout.originY + kRulerHeight && io.MousePos.y < layout.infoTop;
    const float rangeY = layout.groundY + kGroundHeight * 0.5f;

    // ---- ドラッグ中 ----
    if ( enemyDrag_ != EnemyDrag::None ) {
        if ( enemyDragIndex_ < 0 || enemyDragIndex_ >= ( int ) spawns.size() ) {
            CancelInteractions(); // ドラッグ中に消された等
            return;
        }
        if ( ImGui::IsMouseDown(ImGuiMouseButton_Left) ) {
            EnemySpawnData& spawn = spawns[enemyDragIndex_];
            const EnemySpawnData before = spawn;
            if ( enemyDrag_ == EnemyDrag::Move ) {
                // ただのクリック（3px未満）では動かさない＝選んだだけで位置がずれない
                if ( ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f) ) {
                    enemyEditor->MoveEntry(enemyDragIndex_, currentRail_, snappedDist, rails); // 範囲つきは範囲ごと動く
                    // フワリン：上下に動かすと浮く高さが変わる（少し動かしただけでは変えない）
                    if ( spawn.type == EnemyType::Air ) {
                        if ( std::abs(io.MousePos.y - enemyGrabMouseY_) > layout.cellPx * 0.4f ) { enemyHoverAdjust_ = true; }
                        if ( enemyHoverAdjust_ ) {
                            // 見えている段の中までにする（マウスを展開図の上へはみ出させても、見えない高さへ行かない）
                            const float bodyMid = EnemyBodyMid(spawn);
                            const float hoverMax = ( std::min )( kHoverMaxHeight,
                                ( std::max )( 0.0f, std::floor(( ( float ) layout.levels - bodyMid ) / 0.25f) * 0.25f ) );
                            spawn.hoverHeight = std::clamp(SnapTo(mouseHeight - bodyMid, 0.25f), 0.0f, hoverMax);
                        }
                    }
                }
            } else {
                // 動ける範囲の端。実際に動かし始めてから変える（■をクリックしただけで端が目盛りへ吸着して
                // 動いてしまわないように）。つかんだ時の■とマウスのずれは保つ。敵のいる場所は必ず範囲の中に残す
                if ( ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f) ) {
                    if ( rangeAmbiguous_ ) {
                        // 両端が重なっていた：最初に動かした横の向きで、どちらの端を動かすか決める
                        //   （しきい値0で測る。既定のしきい値だと 6px 動くまで 0 が返り、いつも右端になってしまう）
                        const float dragX = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f).x;
                        if ( dragX != 0.0f ) {
                            enemyDrag_ = ( dragX > 0.0f ) ? EnemyDrag::RangeMax : EnemyDrag::RangeMin;
                            rangeAmbiguous_ = false;
                        }
                    }
                }
                if ( !rangeAmbiguous_ && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f) ) {
                    float handleDist = mouseDist + rangeGrabOffset_;
                    handleDist = std::clamp(io.KeyAlt ? handleDist : SnapTo(handleDist, 0.5f), 0.0f, layout.railLength);
                    if ( spawn.patrolMin < 0.0f ) { spawn.patrolMin = 0.0f; }
                    if ( spawn.patrolMax < 0.0f ) { spawn.patrolMax = layout.railLength; }
                    if ( enemyDrag_ == EnemyDrag::RangeMin ) { spawn.patrolMin = ( std::min )( handleDist, spawn.distance ); }
                    else                                     { spawn.patrolMax = ( std::max )( handleDist, spawn.distance ); }
                }
            }
            if ( !( spawn == before ) ) {
                enemyDragChanged_ = true;
                enemyEditor->MarkChanged(); // Game View の敵もその場で動く（実体は使い回すので軽い）
            }
            ImGui::SetMouseCursor(enemyDrag_ == EnemyDrag::Move ? ImGuiMouseCursor_Hand : ImGuiMouseCursor_ResizeEW);
            ImGui::SetTooltip("距離 %.1f m%s", spawn.distance,
                ( spawn.type == EnemyType::Air && enemyHoverAdjust_ ) ? " / 浮く高さを変更中" : "");
        } else {
            enemyDrag_ = EnemyDrag::None;
            enemyDragIndex_ = -1;
            enemyDragChanged_ = false;
            enemyHoverAdjust_ = false;
            rangeAmbiguous_ = false;
        }
        return;
    }

    if ( !inEditArea ) return;

    // ---- マウスの下にあるもの：範囲の端（■）→ 敵の体 の順に探す ----
    const int selected = enemyEditor->GetSelectedEntry();
    int       overEnemy = -1;
    int       overHandleEnemy = -1;
    EnemyDrag overHandle = EnemyDrag::None;
    {
        float bestHandle = 8.0f;
        for ( int i = 0; i < ( int ) spawns.size(); ++i ) {
            const EnemySpawnData& spawn = spawns[i];
            if ( spawn.railIndex != currentRail_ || !EnemyHasRange(spawn) ) continue;
            float rangeMin = ( spawn.patrolMin >= 0.0f ) ? spawn.patrolMin : 0.0f;
            float rangeMax = ( spawn.patrolMax >= 0.0f ) ? spawn.patrolMax : layout.railLength;
            const float ends[2] = { rangeMin, rangeMax };
            for ( int side = 0; side < 2; ++side ) {
                float dx = DistToX(layout, ShownDist(ends[side], layout.railLength)) - io.MousePos.x;
                float dy = rangeY - io.MousePos.y;
                float gap = std::sqrt(dx * dx + dy * dy);
                if ( i == selected ) { gap -= 3.0f; } // 選んでいる敵の範囲を優先
                if ( gap < bestHandle ) {
                    bestHandle = gap;
                    overHandleEnemy = i;
                    overHandle = ( side == 0 ) ? EnemyDrag::RangeMin : EnemyDrag::RangeMax;
                }
            }
        }
        if ( overHandleEnemy < 0 ) {
            float bestBody = 1e9f;
            for ( int i = 0; i < ( int ) spawns.size(); ++i ) {
                const EnemySpawnData& spawn = spawns[i];
                if ( spawn.railIndex != currentRail_ ) continue;
                float radius = ( std::max )( 7.0f, Enemy::TypeSpecOf(spawn.type).bodyRadius * spawn.scale * layout.cellPx * 1.15f );
                // 描いている場所（範囲の外の敵は端に寄せて描いている）と同じ場所で当たりを取る
                float dx = DistToX(layout, ShownDist(spawn.distance, layout.railLength)) - io.MousePos.x;
                float dy = HeightToY(layout, EnemyCenterHeight(spawn)) - io.MousePos.y;
                float gap = std::sqrt(dx * dx + dy * dy);
                if ( gap <= radius + 5.0f && gap < bestBody ) { bestBody = gap; overEnemy = i; }
            }
        }
    }

    if ( overHandleEnemy >= 0 ) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        ImGui::SetTooltip("#%02d の動ける範囲（ドラッグで変更）", overHandleEnemy);
        if ( ImGui::IsMouseClicked(ImGuiMouseButton_Left) ) {
            SelectEnemy(context, overHandleEnemy);
            const EnemySpawnData& grabbed = spawns[overHandleEnemy];
            const float rangeMin = ( grabbed.patrolMin >= 0.0f ) ? grabbed.patrolMin : 0.0f;
            const float rangeMax = ( grabbed.patrolMax >= 0.0f ) ? grabbed.patrolMax : layout.railLength;
            const float handleDist = ShownDist(( overHandle == EnemyDrag::RangeMin ) ? rangeMin : rangeMax, layout.railLength);
            rangeGrabOffset_ = handleDist - mouseDist;
            // 両端が重なっている時は、どちらをつかんだかをドラッグした向きで決める
            rangeAmbiguous_ = std::abs(rangeMax - rangeMin) < 0.01f;
            enemyDrag_ = overHandle;
            enemyDragIndex_ = overHandleEnemy;
            enemyDragChanged_ = false;
        }
        return;
    }

    if ( overEnemy >= 0 ) {
        const EnemySpawnData& spawn = spawns[overEnemy];
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::SetTooltip("#%02d %s\n距離 %.1f m%s%s\nドラッグ=移動 / 右クリック=メニュー",
            overEnemy, EnemyEditor::DisplayName(spawn).c_str(), spawn.distance,
            spawn.patrol ? " / 巡回" : "", spawn.chaseRange > 0.0f ? " / 追跡" : "");
        // Game View の同じ敵を球で囲む
        Vector3 railPoint = rail.GetPositionByDistance(spawn.distance);
        DebugDraw::GetInstance()->Sphere({ railPoint.x, railPoint.y + EnemyCenterHeight(spawn), railPoint.z },
                                         0.8f, { 1.0f, 1.0f, 0.2f, 1.0f });
        if ( ImGui::IsMouseClicked(ImGuiMouseButton_Left) ) {
            SelectEnemy(context, overEnemy);
            enemyDrag_ = EnemyDrag::Move;
            enemyDragIndex_ = overEnemy;
            enemyDragChanged_ = false;
            enemyHoverAdjust_ = false;
            enemyGrabMouseY_ = io.MousePos.y;
        } else if ( ImGui::IsMouseClicked(ImGuiMouseButton_Right) ) {
            SelectEnemy(context, overEnemy);
            contextEnemy_ = overEnemy;
            ImGui::OpenPopup("##stripEnemyMenu");
        }
    } else if ( !rail.visible ) {
        // 見えないレール（連結用の骨組み）には新しく置かない。今いる敵の移動・削除はできる
        if ( mouseDist >= -0.25f && mouseDist <= layout.railLength + 0.25f ) {
            ImGui::SetTooltip("見えないレールには新しく置けません（今ある物の移動・削除はできます）");
        }
    } else if ( mouseDist >= -0.25f && mouseDist <= layout.railLength + 0.25f ) {
        // ---- 何もない所：置く場所の予告 → クリックで置く ----
        EnemySpawnData placing;
        placing.type      = enemyType_;
        placing.railIndex = currentRail_;
        // レール端1mには置かない（始点＝プレイヤーのスタート地点に敵が重なる事故を防ぐ）
        placing.distance  = std::clamp(snappedDist, ( std::min )( 1.0f, layout.railLength * 0.5f ),
                                       ( std::max )( layout.railLength - 1.0f, layout.railLength * 0.5f ));
        if ( placing.type == EnemyType::Air ) {
            // フワリンはマウスの高さに浮かせて置く（道のすぐ上を指した時は種類の既定の高さ）
            float hover = SnapTo(mouseHeight - EnemyBodyMid(placing), 0.25f);
            const float hoverMax = ( std::min )( kHoverMaxHeight,
                ( std::max )( 0.0f, std::floor(( ( float ) layout.levels - EnemyBodyMid(placing) ) / 0.25f) * 0.25f ) );
            if ( hover >= 0.5f ) { placing.hoverHeight = ( std::min )( hover, hoverMax ); }
        }
        const float x = DistToX(layout, placing.distance);
        const float centerY = HeightToY(layout, EnemyCenterHeight(placing));
        const float radius = ( std::max )( 7.0f, Enemy::TypeSpecOf(placing.type).bodyRadius * layout.cellPx * 1.15f );
        draw->AddCircleFilled({ x, centerY }, radius, EnemyColor(placing.type, 110), 20);
        draw->AddCircle({ x, centerY }, radius, IM_COL32(255, 255, 255, 200), 20, 1.5f);
        Vector3 railPoint = rail.GetPositionByDistance(placing.distance);
        DebugDraw::GetInstance()->Sphere({ railPoint.x, railPoint.y + EnemyCenterHeight(placing), railPoint.z },
                                         0.5f, { 0.3f, 1.0f, 0.6f, 1.0f });
        if ( ImGui::IsMouseClicked(ImGuiMouseButton_Left) ) {
            enemyEditor->AddEntry(placing); // 置いた敵は選択状態になる＝敵エディタですぐ動きを決められる
            lastSelectedEnemy_ = enemyEditor->GetSelectedEntry();
        }
    }

    // Delete：選んでいる敵（このレールの敵）を消す
    if ( ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !io.WantTextInput
        && selected >= 0 && selected < ( int ) spawns.size() && spawns[selected].railIndex == currentRail_ ) {
        enemyEditor->RemoveEntry(selected);
        lastSelectedEnemy_ = enemyEditor->GetSelectedEntry();
    }
#else
    ( void ) context; ( void ) layout; ( void ) hovered;
#endif
}

// 敵の右クリックメニュー
void RailStripPanel::DrawEnemyContextMenu(const Context& context){
#ifdef USE_IMGUI
    if ( !ImGui::BeginPopup("##stripEnemyMenu") ) return;
    EnemyEditor* enemyEditor = context.enemyEditor;
    auto& spawns = enemyEditor->MutableSpawnDatas();
    if ( !context.editable || contextEnemy_ < 0 || contextEnemy_ >= ( int ) spawns.size() ) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    const int index = contextEnemy_;
    ImGui::TextDisabled("#%02d %s", index, EnemyEditor::DisplayName(spawns[index]).c_str());
    ImGui::Separator();
    if ( ImGui::MenuItem("設定を開く（敵エディタ）") ) {
        SelectEnemy(context, index);
        openEnemyPanelPending_ = true;
    }
    if ( ImGui::MenuItem("カメラをここへ") ) {
        focusPending_ = true;
        focusRail_    = spawns[index].railIndex;
        focusDist_    = spawns[index].distance;
        focusHeight_  = EnemyCenterHeight(spawns[index]);
    }
    ImGui::Separator();
    {
        EnemySpawnData& spawn = spawns[index];
        if ( ImGui::MenuItem("巡回する（往復）", nullptr, spawn.patrol) ) {
            spawn.patrol = !spawn.patrol;
            enemyEditor->MarkChanged();
        }
        const bool moves = ( spawn.patrol || spawn.chaseRange > 0.0f );
        const bool ranged = ( spawn.patrolMin >= 0.0f || spawn.patrolMax >= 0.0f );
        if ( ImGui::MenuItem("動ける範囲を決める", nullptr, moves && ranged, moves) ) {
            if ( ranged ) {
                spawn.patrolMin = spawn.patrolMax = -1.0f;
            } else {
                // 初期値は「今の位置±3m」。展開図の橙の■をドラッグして調整できる
                float railLength = ( *context.rails )[spawn.railIndex].GetLength();
                spawn.patrolMin = std::clamp(spawn.distance - 3.0f, 0.0f, railLength);
                spawn.patrolMax = std::clamp(spawn.distance + 3.0f, spawn.patrolMin, railLength);
            }
            enemyEditor->MarkChanged();
        }
    }
    ImGui::Separator();
    if ( ImGui::MenuItem("複製（2m先へ）") ) {
        enemyEditor->DuplicateEntry(index, *context.rails);
        lastSelectedEnemy_ = enemyEditor->GetSelectedEntry();
        contextEnemy_ = -1;
    } else if ( ImGui::MenuItem("削除") ) {
        enemyEditor->RemoveEntry(index);
        lastSelectedEnemy_ = enemyEditor->GetSelectedEntry();
        contextEnemy_ = -1;
    }
    ImGui::EndPopup();
#else
    ( void ) context;
#endif
}

// =====================================================================
//  道具：コイン
// =====================================================================
void RailStripPanel::UpdateCoinTool(const Context& context, const Layout& layout, bool hovered){
#ifdef USE_IMGUI
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    RailEditor* railEditor = context.railEditor;
    const SplineRail& rail = ( *context.rails )[currentRail_];
    const auto& coins = railEditor->GetCoins();

    const float mouseDist = XToDist(layout, io.MousePos.x);
    const float snappedDist = std::clamp(io.KeyAlt ? mouseDist : SnapTo(mouseDist, 0.5f), 0.0f, layout.railLength);
    // 高さは見えている段の中まで（はみ出したコインは展開図でつかめなくなるため）
    const float coinHeightMax = ( std::min )( kCoinMaxHeight, ( float ) layout.levels - 0.25f );
    const float snappedHeight = std::clamp(
        io.KeyAlt ? YToHeight(layout, io.MousePos.y) : SnapTo(YToHeight(layout, io.MousePos.y), 0.25f),
        0.0f, coinHeightMax);
    const bool inEditArea = hovered && io.MousePos.y >= layout.originY + kRulerHeight && io.MousePos.y < layout.groundY;
    const float radius = ( std::max )( 4.0f, 0.2f * layout.cellPx );

    // コインを変えた後の後始末：Game View のコインを作り直し、未保存の印を付ける
    auto commitCoins = [&]() {
        if ( context.onCoinsChanged ) { context.onCoinsChanged(); }
        if ( context.levelEditor ) { context.levelEditor->MarkDirty(); }
    };

    // ---- ドラッグ中（確定は離した時。ドラッグ中は移動先を描くだけ＝軽い）----
    if ( coinDragIndex_ >= 0 ) {
        if ( coinDragIndex_ >= ( int ) coins.size() ) { coinDragIndex_ = -1; return; }
        if ( ImGui::IsMouseDown(ImGuiMouseButton_Left) ) {
            if ( ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f) ) {
                coinDragData_.rail   = currentRail_;
                coinDragData_.dist   = snappedDist;
                coinDragData_.height = snappedHeight;
            }
            Vector3 railPoint = rail.GetPositionByDistance(coinDragData_.dist);
            DebugDraw::GetInstance()->Sphere({ railPoint.x, railPoint.y + coinDragData_.height, railPoint.z },
                                             0.35f, { 1.0f, 0.9f, 0.2f, 1.0f }, 12);
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            ImGui::SetTooltip("距離 %.1f m / 高さ %.2f m", coinDragData_.dist, coinDragData_.height);
        } else {
            // つかんだ時と中身が変わっていたら書き込まない（ドラッグ中の Ctrl+Z 等で別のコインを上書きしない）
            const bool sameCoin = ( coins[coinDragIndex_] == coinDragOrig_ );
            const bool moved = !( coinDragData_ == coinDragOrig_ );
            if ( sameCoin && moved ) {
                railEditor->SetCoinAt(coinDragIndex_, coinDragData_.rail, coinDragData_.dist, coinDragData_.height);
                commitCoins();
            }
            coinDragIndex_ = -1;
        }
        return;
    }

    if ( !inEditArea ) return;

    // ---- マウスの下のコイン ----
    int overCoin = -1;
    float bestGap = radius + 5.0f;
    for ( int i = 0; i < ( int ) coins.size(); ++i ) {
        if ( coins[i].rail != currentRail_ ) continue;
        float dx = DistToX(layout, ShownDist(coins[i].dist, layout.railLength)) - io.MousePos.x;
        float dy = HeightToY(layout, coins[i].height) - io.MousePos.y;
        float gap = std::sqrt(dx * dx + dy * dy);
        if ( gap < bestGap ) { bestGap = gap; overCoin = i; }
    }

    if ( overCoin >= 0 ) {
        const CoinData& coin = coins[overCoin];
        draw->AddCircle({ DistToX(layout, ShownDist(coin.dist, layout.railLength)), HeightToY(layout, coin.height) }, radius + 3.0f,
                        IM_COL32(255, 255, 255, 255), 16, 2.0f);
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::SetTooltip("コイン（距離 %.1f m / 高さ %.2f m）\nドラッグ=移動 / 右クリック=削除", coin.dist, coin.height);
        Vector3 railPoint = rail.GetPositionByDistance(coin.dist);
        DebugDraw::GetInstance()->Sphere({ railPoint.x, railPoint.y + coin.height, railPoint.z },
                                         0.45f, { 1.0f, 0.95f, 0.3f, 1.0f }, 12);
        if ( ImGui::IsMouseClicked(ImGuiMouseButton_Left) ) {
            coinDragIndex_ = overCoin;
            coinDragData_  = coin;
            coinDragOrig_  = coin;
        } else if ( ImGui::IsMouseClicked(ImGuiMouseButton_Right) ) {
            railEditor->RemoveCoinAt(overCoin);
            commitCoins();
        }
    } else if ( !rail.visible ) {
        // 見えないレール（連結用の骨組み）には新しく置かない。今あるコインの移動・削除はできる
        if ( mouseDist >= -0.25f && mouseDist <= layout.railLength + 0.25f ) {
            ImGui::SetTooltip("見えないレールには新しく置けません（今ある物の移動・削除はできます）");
        }
    } else if ( mouseDist >= -0.25f && mouseDist <= layout.railLength + 0.25f ) {
        // ---- 何もない所：置く場所の予告 → クリックで置く ----
        ImVec2 center = { DistToX(layout, snappedDist), HeightToY(layout, snappedHeight) };
        draw->AddCircleFilled(center, radius, IM_COL32(255, 214, 60, 110), 16);
        draw->AddCircle(center, radius, IM_COL32(255, 255, 255, 200), 16, 1.5f);
        Vector3 railPoint = rail.GetPositionByDistance(snappedDist);
        DebugDraw::GetInstance()->Sphere({ railPoint.x, railPoint.y + snappedHeight, railPoint.z },
                                         0.35f, { 1.0f, 0.9f, 0.2f, 1.0f }, 12);
        if ( ImGui::IsMouseClicked(ImGuiMouseButton_Left) ) {
            railEditor->AddCoinAt(currentRail_, snappedDist);
            int added = ( int ) railEditor->GetCoins().size() - 1;
            railEditor->SetCoinAt(added, currentRail_, snappedDist, snappedHeight);
            commitCoins();
        }
    }
#else
    ( void ) context; ( void ) layout; ( void ) hovered;
#endif
}
