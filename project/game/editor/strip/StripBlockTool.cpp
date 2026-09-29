#include "game/editor/strip/StripBlockTool.h"

#include "engine/rail/SplineRail.h"
#include "engine/utils/Level/RailEditor.h"
#include "externals/imgui/imgui.h"
#include "game/editor/strip/StripCommon.h"
#include "game/stage/BlockSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace railstrip;

// 途中の塗り・移動をやめる。レールや道具・横の位置を切り替えた時に呼ぶ
void StripBlockTool::Cancel(){
    stroke_ = StrokeKind::None;
    moveIndex_ = -1;
    moveDragged_ = false;
}

void StripBlockTool::CancelRectOrMove(){
    if ( stroke_ == StrokeKind::RectFill || stroke_ == StrokeKind::RectErase || stroke_ == StrokeKind::Move ) {
        Cancel();
    }
}

void StripBlockTool::RectBounds(int& distFrom, int& distTo, int& levelFrom, int& levelTo) const{
    distFrom  = ( std::min )( rectStartDist_, rectEndDist_ );
    distTo    = ( std::max )( rectStartDist_, rectEndDist_ );
    levelFrom = ( std::min )( rectStartLevel_, rectEndLevel_ );
    levelTo   = ( std::max )( rectStartLevel_, rectEndLevel_ );
}

// =====================================================================
//  ブロックの判定
// =====================================================================
int StripBlockTool::FindBlockAt(const StripContext& context, const StripState& state, float dist, int level) const{
    const auto& blocks = context.railEditor->GetBlocks();
    const float railLength = ( *context.rails )[state.currentRail].GetLength();
    int   found = -1;
    float bestGap = 1e9f;
    for ( int i = 0; i < ( int ) blocks.size(); ++i ) {
        const BlockData& block = blocks[i];
        if ( block.rail != state.currentRail || block.level != level ) continue;
        if ( std::abs(block.side - ( float ) state.sideLayer) > 0.5f ) continue;
        float gap = std::abs(ShownDist(block.dist, railLength) - dist);
        if ( gap > BlockSystem::VisualHalfAlong(block.type) ) continue; // 見えている形の中を指しているか
        if ( gap < bestGap ) { bestGap = gap; found = i; }
    }
    return found;
}

// RailEditor::AddBlock と同じ決まり：占有幅どうしが重なる場所には置けない
bool StripBlockTool::CanPlaceBlock(const StripContext& context, const StripState& state,
                                   float dist, int level, int type, int ignoreIndex) const{
    if ( level < 0 || level >= kMaxLevels ) return false;
    const SplineRail& rail = ( *context.rails )[state.currentRail];
    const float railLength = rail.GetLength();
    if ( dist < 0.0f || dist > railLength ) return false;
    if ( !context.railEditor->CanPlaceBlock(state.currentRail, dist, level, ( float ) state.sideLayer, type, ignoreIndex) ) {
        return false;
    }
    // ループ（始点と終点がつながったレール）：つなぎ目をまたいで重なる置き方も止める
    if ( rail.isLoop ) {
        const auto& blocks = context.railEditor->GetBlocks();
        const float newHalf = RailEditor::BlockOccupyHalfOf(type);
        for ( int i = 0; i < ( int ) blocks.size(); ++i ) {
            if ( i == ignoreIndex ) continue;
            const BlockData& block = blocks[i];
            if ( block.rail != state.currentRail || block.level != level ) continue;
            if ( std::abs(block.side - ( float ) state.sideLayer) >= 0.51f ) continue;
            float acrossSeam = std::abs(std::abs(block.dist - dist) - railLength);
            if ( acrossSeam < RailEditor::BlockOccupyHalfOf(block.type) + newHalf - 0.01f ) return false;
        }
    }
    return true;
}

// マウスの位置から、実際に置く距離を決める。
//   まずマスの中心（整数の距離）。そこが塞がっている時だけ、半マスずらした位置を
//   マウスに近い側から試す。横長・台座（2m）を隣のブロックへぴったり付けて置けるようにするため
bool StripBlockTool::ResolvePlaceDist(const StripContext& context, const StripState& state, float mouseDist, int level,
                                      int type, int ignoreIndex, float& outDist) const{
    const float cell = std::floor(mouseDist + 0.5f);
    const float nearSide = ( mouseDist >= cell ) ? 1.0f : -1.0f;
    const float candidates[3] = { cell, cell + 0.5f * nearSide, cell - 0.5f * nearSide };
    for ( float candidate : candidates ) {
        if ( CanPlaceBlock(context, state, candidate, level, type, ignoreIndex) ) {
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
void StripBlockTool::ApplyBlockStroke(const StripContext& context, const StripState& state,
                                      int cellDist, int level, float probeDist){
    RailEditor* railEditor = context.railEditor;
    const int paintType = railEditor->GetBlockPaintType();
    if ( stroke_ == StrokeKind::Place ) {
        const float dist = ( float ) cellDist;
        if ( ( *context.rails )[state.currentRail].visible && CanPlaceBlock(context, state, dist, level, paintType) ) {
            railEditor->AddBlock(state.currentRail, dist, level, ( float ) state.sideLayer, paintType);
        }
    } else if ( stroke_ == StrokeKind::Replace || stroke_ == StrokeKind::Erase ) {
        int found = FindBlockAt(context, state, probeDist, level);
        if ( found >= 0 ) {
            if ( stroke_ == StrokeKind::Erase ) { railEditor->RemoveBlockAt(found); }
            else                                { railEditor->ReplaceBlockTypeAt(found, paintType); }
        }
    }
    strokeLastDist_  = cellDist;
    strokeLastLevel_ = level;
}

// =====================================================================
//  道具：ブロック
// =====================================================================
void StripBlockTool::Update(const StripContext& context, const StripState& state, const StripLayout& layout, bool hovered){
#ifdef USE_IMGUI
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    RailEditor* railEditor = context.railEditor;
    const int currentRail = state.currentRail;
    const SplineRail& rail = ( *context.rails )[currentRail];
    const int paintType = railEditor->GetBlockPaintType();
    // 見えないレール（連結用の骨組み）には新しく置かない。今ある物の移動・塗り替え・削除はできる
    //   （Game View のブロック配置も見えないレールには置かない。同じ決まりにそろえる）
    const bool placementAllowed = rail.visible;

    // マウスが指しているマス
    const float mouseDist = layout.XToDist(io.MousePos.x);
    int cellDist = ( int ) std::floor(mouseDist + 0.5f);
    int level    = ( int ) std::floor(layout.YToHeight(io.MousePos.y));
    const bool inLevels = hovered && io.MousePos.y >= layout.HeightToY(( float ) layout.levels)
                       && io.MousePos.y < layout.groundY;
    const bool cellValid = inLevels && cellDist >= 0 && cellDist <= layout.lastCell
                        && level >= 0 && level < layout.levels;

    // マスの枠（展開図）と、同じマスの枠（Game View）を描く
    auto drawCellFrame = [&](float dist, int cellLevel, int type, ImU32 color, const Vector4& worldColor, float inflate) {
        float half = BlockSystem::VisualHalfAlong(type);
        draw->AddRect({ layout.DistToX(dist - half), layout.HeightToY(( float ) cellLevel + 1.0f) },
                      { layout.DistToX(dist + half), layout.HeightToY(( float ) cellLevel) }, color, 2.0f, 0, 2.5f);
        if ( context.blockSystem ) {
            context.blockSystem->DrawCellGhost(currentRail, dist, cellLevel, ( float ) state.sideLayer, type, worldColor, inflate);
        }
    };

    // =============================================================
    //  何もしていない時：指している場所の予告と、操作の始まり
    // =============================================================
    if ( stroke_ == StrokeKind::None ) {
        if ( !cellValid ) return;
        const auto& blocks = railEditor->GetBlocks();
        const int under = FindBlockAt(context, state, mouseDist, level);
        // ブロックの「本体」を指しているか。ゆるい斜面は見た目が2mあるが、場所を取るのは中央の1mだけで、
        // 前後の裾のマスには別のブロックを置ける。裾を指した時は「置く」を優先する
        bool underSolid = false;
        if ( under >= 0 ) {
            float gap = std::abs(ShownDist(blocks[under].dist, layout.railLength) - mouseDist);
            underSolid = ( gap <= RailEditor::BlockOccupyHalfOf(blocks[under].type) );
        }
        float placeDist = ( float ) cellDist;
        const bool canPlace = placementAllowed
            && ResolvePlaceDist(context, state, mouseDist, level, paintType, -1, placeDist);
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
            const bool decoration = ( state.sideLayer != 0 ); // 道の脇＝当たらない飾り
            drawCellFrame(placeDist, level, paintType,
                decoration ? IM_COL32(90, 204, 255, 255) : IM_COL32(80, 255, 130, 255),
                decoration ? Vector4 { 0.35f, 0.8f, 1.0f, 1.0f } : Vector4 { 0.3f, 1.0f, 0.5f, 1.0f }, 0.0f);
        } else {
            drawCellFrame(( float ) cellDist, level, paintType, IM_COL32(255, 90, 80, 255), { 1.0f, 0.35f, 0.3f, 1.0f }, 0.0f);
            ImGui::SetTooltip("%s", placementAllowed
                ? "ここには置けません（隣のブロックと重なります）"
                : kInvisibleRailMessage);
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
            railEditor->AddBlock(currentRail, placeDist, level, ( float ) state.sideLayer, paintType);
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
            Cancel();
            return;
        }
        if ( ImGui::IsMouseDown(ImGuiMouseButton_Left) ) {
            if ( ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f) ) { moveDragged_ = true; }
            if ( moveDragged_ ) {
                const float clampedMouse = std::clamp(mouseDist, 0.0f, layout.railLength);
                float targetDist = ( float ) cellDist;
                moveValid_ = ResolvePlaceDist(context, state, clampedMouse, level, moveOrig_.type, moveIndex_, targetDist);
                moveDist_  = targetDist;
                moveLevel_ = level;
                // 移動先の予告（緑=置ける / 赤=他のブロックと重なる）と、元の場所からの線
                drawCellFrame(moveDist_, moveLevel_, moveOrig_.type,
                    moveValid_ ? IM_COL32(80, 255, 130, 255) : IM_COL32(255, 90, 80, 255),
                    moveValid_ ? Vector4 { 0.3f, 1.0f, 0.5f, 1.0f } : Vector4 { 1.0f, 0.35f, 0.3f, 1.0f }, 0.0f);
                draw->AddLine(
                    { layout.DistToX(ShownDist(moveOrig_.dist, layout.railLength)), layout.HeightToY(( float ) moveOrig_.level + 0.5f) },
                    { layout.DistToX(moveDist_), layout.HeightToY(( float ) moveLevel_ + 0.5f) },
                    IM_COL32(255, 240, 80, 200), 1.5f);
                ImGui::SetTooltip(moveValid_ ? "離すとここへ移動（Esc でやめる）" : "ここへは動かせません（他のブロックと重なります）");
            }
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        } else {
            // 離した：置ける場所なら移動する。置けない場所・クリックだけなら何も変えない
            if ( moveDragged_ && moveValid_ ) {
                if ( railEditor->MoveBlock(moveIndex_, currentRail, moveDist_, moveLevel_, moveOrig_.side) ) {
                    railEditor->UpdateBlockAnchors(*context.rails); // 離した瞬間に置いたので、錨もその場で入れる
                }
            }
            Cancel();
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
            int distFrom, distTo, levelFrom, levelTo;
            RectBounds(distFrom, distTo, levelFrom, levelTo);
            ImU32 color = ( stroke_ == StrokeKind::RectErase ) ? IM_COL32(255, 90, 80, 255) : IM_COL32(80, 255, 130, 255);
            ImVec2 boxMin = { layout.DistToX(( float ) distFrom - 0.5f), layout.HeightToY(( float ) levelTo + 1.0f) };
            ImVec2 boxMax = { layout.DistToX(( float ) distTo + 0.5f),   layout.HeightToY(( float ) levelFrom) };
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
                ApplyBlockStroke(context, state, stepDist, stepLevel, probe);
            }
        }
    } else {
        // 離した：四角の範囲をまとめて適用
        if ( rectStroke ) {
            int distFrom, distTo, levelFrom, levelTo;
            RectBounds(distFrom, distTo, levelFrom, levelTo);
            stroke_ = ( stroke_ == StrokeKind::RectErase ) ? StrokeKind::Erase : StrokeKind::Place;
            for ( int fillLevel = levelFrom; fillLevel <= levelTo; ++fillLevel ) {
                for ( int fillDist = distFrom; fillDist <= distTo; ++fillDist ) {
                    ApplyBlockStroke(context, state, fillDist, fillLevel, ( float ) fillDist);
                }
            }
            railEditor->UpdateBlockAnchors(*context.rails); // 離した瞬間に置いたので、錨もその場で入れる
        }
        stroke_ = StrokeKind::None;
    }
#else
    ( void ) context; ( void ) state; ( void ) layout; ( void ) hovered;
#endif
}
