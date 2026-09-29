#include "game/editor/strip/StripCanvas.h"

#include "engine/graphics/DebugDraw.h"
#include "engine/rail/SplineRail.h"
#include "engine/utils/Level/RailEditor.h"
#include "externals/imgui/imgui.h"
#include "game/editor/strip/StripCommon.h"
#include "game/editor/strip/StripRenderer.h"
#include "game/enemy/EnemyEditor.h"

#include <algorithm>
#include <cmath>

using namespace railstrip;

// 途中の操作（塗り・ドラッグ）を全部やめる。レールや道具を切り替えた時に呼ぶ
void StripCanvas::CancelInteractions(){
    blockTool_.Cancel();
    enemyTool_.Cancel();
    coinTool_.Cancel();
    panning_ = false;
}

bool StripCanvas::IsInteracting() const{
    return blockTool_.IsBusy() || enemyTool_.IsBusy() || coinTool_.IsBusy();
}

// ----------------------------------------------------------------
//  寸法を決める
// ----------------------------------------------------------------
StripLayout StripCanvas::MeasureLayout(const StripContext& context, StripState& state){
    StripLayout layout;
#ifdef USE_IMGUI
    const SplineRail& rail = ( *context.rails )[state.currentRail];
    layout.railLength = rail.GetLength();
    layout.lastCell   = ( std::max )( 0, ( int ) std::floor(layout.railLength + 0.001f) );
    // 「全体」：レール全体が今の表示幅に入る拡大率にする（幅は前のフレームの値を使う）
    if ( state.fitRequested ) {
        state.fitRequested = false;
        if ( canvasViewWidth_ > kPadLeft + kPadRight + 40.0f ) {
            state.cellPx = ( canvasViewWidth_ - kPadLeft - kPadRight ) / ( float ) ( layout.lastCell + 1 );
        }
        state.scrollResetPending = true;
        state.scrollToDist = -1.0f;
    }
    state.cellPx = std::clamp(state.cellPx, kMinCellPx, kMaxCellPx);
    layout.cellPx = state.cellPx;
    // 置いてある物が隠れないよう、一番高い物までは必ず見せる（ブロック・浮いている敵・高い所のコイン）
    int highestLevel = 0;
    for ( const BlockData& block : context.railEditor->GetBlocks() ) {
        if ( block.rail == state.currentRail ) { highestLevel = ( std::max )( highestLevel, block.level + 1 ); }
    }
    for ( const EnemySpawnData& spawn : context.enemyEditor->GetSpawnDatas() ) {
        if ( spawn.railIndex != state.currentRail ) continue;
        highestLevel = ( std::max )( highestLevel, ( int ) std::ceil(EnemyCenterHeight(spawn)) );
    }
    for ( const CoinData& coin : context.railEditor->GetCoins() ) {
        if ( coin.rail == state.currentRail ) { highestLevel = ( std::max )( highestLevel, ( int ) std::ceil(coin.height + 0.25f) ); }
    }
    int levels = std::clamp(( std::max )( state.visibleLevels, highestLevel ), 1, kMaxLevels);
    // 操作の途中は段数を変えない。段数が変わるとマス目全体が上下にずれ、止まっているマウスの下へ別のマスが来る
    //   （一番上のブロックを消した瞬間に段数が減って、1段下も続けて消えてしまう等）
    if ( IsInteracting() && lastLevels_ > 0 ) { levels = lastLevels_; }
    lastLevels_ = levels;
    layout.levels = levels;
    layout.width  = kPadLeft + ( float ) ( layout.lastCell + 1 ) * layout.cellPx + kPadRight;
    layout.height = kRulerHeight + ( float ) layout.levels * layout.cellPx + kGroundHeight + kInfoHeight;
#else
    ( void ) context; ( void ) state;
#endif
    return layout;
}

// ----------------------------------------------------------------
//  スクロール位置の要求
// ----------------------------------------------------------------
void StripCanvas::ApplyScrollRequests(StripState& state, const StripLayout& layout){
#ifdef USE_IMGUI
    if ( state.scrollToDist >= 0.0f ) {
        // 指定の距離が表示の中央に来る位置へ（範囲の外は ImGui が端で止める）。道（一番下）も見せる
        const float contentX = kPadLeft + ( state.scrollToDist + 0.5f ) * layout.cellPx;
        ImGui::SetScrollX(( std::max )( 0.0f, contentX - canvasViewWidth_ * 0.5f ));
        ImGui::SetScrollY(layout.height);
        state.scrollToDist = -1.0f;
        state.scrollResetPending = false;
    } else if ( state.scrollResetPending ) {
        // レールを開いた直後：先頭（距離0）と道（一番下）が見える位置へ
        ImGui::SetScrollX(0.0f);
        ImGui::SetScrollY(layout.height);
        state.scrollResetPending = false;
    }
#else
    ( void ) state; ( void ) layout;
#endif
}

// ----------------------------------------------------------------
//  展開図（横スクロールできるキャンバス）
// ----------------------------------------------------------------
StripCanvas::RailRequest StripCanvas::Draw(const StripContext& context, StripState& state){
    RailRequest railRequest;
#ifdef USE_IMGUI
    const SplineRail& rail = ( *context.rails )[state.currentRail];
    ImGuiIO& io = ImGui::GetIO();

    // --- 寸法を決める ---
    StripLayout layout = MeasureLayout(context, state);

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
    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY); // ホイールは拡大に使う（親の窓をスクロールさせない）

    canvasViewWidth_ = ImGui::GetWindowSize().x;
    ApplyScrollRequests(state, layout);

    // 見えている範囲の中央の距離（「カメラをここへ」ボタン用）
    {
        const ImVec2 childPos = ImGui::GetWindowPos();
        state.viewCenterDist = layout.XToDist(childPos.x + canvasViewWidth_ * 0.5f);
    }
    // 指している場所（下の説明行と Game View の枠表示に使う）
    Hover hover;
    if ( hovered ) {
        hover.valid  = true;
        hover.dist   = layout.XToDist(io.MousePos.x);
        hover.height = layout.YToHeight(io.MousePos.y);
    }

    // --- 描画（奥から順に）---
    //   何かを塗っている/動かしている途中は、レールのつながりのラベルに反応しない
    //   （ドラッグがラベルの上を通っただけでレールが移らないように）
    const bool linksActive = hovered && !IsInteracting() && !panning_;
    StripRenderer::DrawBackdrop(context, state, layout);
    const StripRenderer::LinkResult links = StripRenderer::DrawRailInfo(context, state, layout, linksActive);
    railRequest.rail = links.railSelect;
    railRequest.dist = links.railDist;
    StripRenderer::DrawMarkers(context, state, layout);
    StripRenderer::DrawBlocks(context, state, layout, blockTool_.MovingBlock());
    StripRenderer::DrawCoins(context, state, layout, coinTool_.DraggedCoin(), coinTool_.DraggedData());
    StripRenderer::DrawEnemies(context, state, layout);

    // --- 操作 ---
    //   レールのつながりのラベルの上では道具を動かさない（ラベルのクリックでレールを移る時に、
    //   前のレールへブロックや敵が置かれてしまわないように）
    const bool toolHovered = hovered && !links.overLabel;
    if ( context.editable ) {
        switch ( state.tool ) {
        case StripTool::Block:
            blockTool_.Update(context, state, layout, toolHovered);
            break;
        case StripTool::Enemy:
            if ( enemyTool_.Update(context, state, layout, toolHovered) ) { CancelInteractions(); }
            break;
        case StripTool::Coin:
            coinTool_.Update(context, state, layout, toolHovered);
            break;
        }
    }
    enemyTool_.DrawContextMenu(context, state);
    StripRenderer::DrawLevelLabels(layout);

    // 指している距離を Game View にも縦線で出す（3Dのどこを指しているかの目印）
    if ( hover.valid && hover.dist >= 0.0f && hover.dist <= layout.railLength ) {
        Vector3 railPoint = rail.GetPositionByDistance(hover.dist);
        DebugDraw::GetInstance()->Line(railPoint, { railPoint.x, railPoint.y + ( float ) layout.levels, railPoint.z },
                                       { 1.0f, 1.0f, 1.0f, 0.55f });
    }

    // --- キー操作（展開図の上にマウスがある時だけ）---
    HandleKeys(context, state, layout, hover);

    // --- 表示の移動と拡大 ---
    HandlePanZoom(state, layout, hovered);

    ImGui::EndChild();

    // --- 下の説明行：いま指している場所 ---
    DrawStatusLine(state, layout, hover);
#else
    ( void ) context; ( void ) state;
#endif
    return railRequest;
}

// ----------------------------------------------------------------
//  キー操作
// ----------------------------------------------------------------
void StripCanvas::HandleKeys(const StripContext& context, StripState& state, const StripLayout& layout, const Hover& hover){
#ifdef USE_IMGUI
    ImGuiIO& io = ImGui::GetIO();
    const bool hovered = hover.valid;

    // 敵の道具の間：展開図の上での Ctrl+Z / Ctrl+Y は敵の履歴へ回す（詳しくは StripEnemyTool::HandleUndoKeys）
    if ( hovered && context.editable && state.tool == StripTool::Enemy ) {
        if ( enemyTool_.HandleUndoKeys(context) ) { CancelInteractions(); }
    }
    // Esc：途中の「四角くまとめて」「移動」を、何も適用せずにやめる
    if ( context.editable && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape, false) ) {
        blockTool_.CancelRectOrMove();
    }
    // 右ボタンを押している間は反応しない（Game View で視点を回しながら E で上昇している時に、
    // マウスが展開図の上へ流れて道具が切り替わらないように）
    if ( hovered && !io.WantTextInput && !io.KeyCtrl && !ImGui::IsMouseDown(ImGuiMouseButton_Right) ) {
        if ( context.editable ) {
            StripTool requested = state.tool;
            if ( ImGui::IsKeyPressed(ImGuiKey_B, false) ) { requested = StripTool::Block; }
            if ( ImGui::IsKeyPressed(ImGuiKey_E, false) ) { requested = StripTool::Enemy; }
            if ( ImGui::IsKeyPressed(ImGuiKey_C, false) ) { requested = StripTool::Coin; }
            if ( requested != state.tool ) { state.tool = requested; CancelInteractions(); }

            // 数字キー：置く物の種類（ブロック 1〜9 / 敵 1〜3）
            for ( int number = 0; number < 9; ++number ) {
                if ( !ImGui::IsKeyPressed(( ImGuiKey ) ( ImGuiKey_1 + number ), false) ) continue;
                if ( state.tool == StripTool::Block ) {
                    context.railEditor->SetBlockPaintType(number);
                } else if ( state.tool == StripTool::Enemy && number < 3 ) {
                    state.enemyType = kEnemyTypes[number];
                }
            }
            // Z / X：ブロックを置く横の位置を1つずらす
            if ( state.tool == StripTool::Block ) {
                int side = state.sideLayer;
                if ( ImGui::IsKeyPressed(ImGuiKey_Z, false) ) { --side; }
                if ( ImGui::IsKeyPressed(ImGuiKey_X, false) ) { ++side; }
                side = std::clamp(side, -2, 2);
                if ( side != state.sideLayer ) { state.sideLayer = side; blockTool_.Cancel(); }
            }
            // F：カメラを指している場所へ（編集中だけ。プレイ中の F は吐き出しに使う）
            if ( ImGui::IsKeyPressed(ImGuiKey_F, false) ) {
                state.RequestFocus(state.currentRail,
                                   std::clamp(hover.dist, 0.0f, layout.railLength),
                                   std::clamp(hover.height, 0.0f, ( float ) kMaxLevels));
            }
        }
    }
#else
    ( void ) context; ( void ) state; ( void ) layout; ( void ) hover;
#endif
}

// ----------------------------------------------------------------
//  表示の移動と拡大
//   中ボタンドラッグ＝表示を動かす / ホイール＝マウスの位置を中心に拡大
// ----------------------------------------------------------------
void StripCanvas::HandlePanZoom(StripState& state, const StripLayout& layout, bool hovered){
#ifdef USE_IMGUI
    ImGuiIO& io = ImGui::GetIO();
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
        const float newCellPx = std::clamp(layout.cellPx * ( 1.0f + io.MouseWheel * 0.12f ), kMinCellPx, kMaxCellPx);
        if ( newCellPx != layout.cellPx ) {
            // マウスの下にある場所が、拡大した後も同じ場所に来るようにスクロールを合わせる（横も縦も）。
            //   縦は道より下を指している時は道の高さを基準にする（道が下へ流れて見えなくならないように）
            const float anchorDist   = layout.XToDist(io.MousePos.x);
            const float anchorHeight = std::clamp(layout.YToHeight(io.MousePos.y), 0.0f, ( float ) layout.levels);
            ImGui::SetScrollX(ImGui::GetScrollX() + ( anchorDist + 0.5f ) * ( newCellPx - layout.cellPx ));
            ImGui::SetScrollY(ImGui::GetScrollY() + ( ( float ) layout.levels - anchorHeight ) * ( newCellPx - layout.cellPx ));
            state.cellPx = newCellPx;
        }
    }
#else
    ( void ) state; ( void ) layout; ( void ) hovered;
#endif
}

// ----------------------------------------------------------------
//  下の説明行：いま指している場所
// ----------------------------------------------------------------
void StripCanvas::DrawStatusLine(const StripState& state, const StripLayout& layout, const Hover& hover) const{
#ifdef USE_IMGUI
    if ( hover.valid ) {
        int level = ( int ) std::floor(hover.height);
        if ( hover.height >= 0.0f && level < layout.levels ) {
            ImGui::Text("Rail %d ｜ 距離 %.1f m ｜ %d段目（高さ %.2f m）｜ 横の位置 %+d",
                state.currentRail, std::clamp(hover.dist, 0.0f, layout.railLength), level + 1, hover.height, state.sideLayer);
        } else {
            ImGui::Text("Rail %d ｜ 距離 %.1f m", state.currentRail, std::clamp(hover.dist, 0.0f, layout.railLength));
        }
    } else {
        ImGui::TextDisabled("ホイール=拡大 / 中ボタンドラッグ=表示を動かす / 指している場所は Game View にも白い線と枠で出ます");
    }
#else
    ( void ) state; ( void ) layout; ( void ) hover;
#endif
}
