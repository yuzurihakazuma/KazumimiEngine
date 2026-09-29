#include "game/editor/strip/StripRenderer.h"

#include "engine/rail/SplineRail.h"
#include "engine/utils/Level/RailEditor.h"
#include "externals/imgui/imgui.h"
#include "game/editor/strip/StripCommon.h"
#include "game/enemy/EnemyEditor.h"
#include "game/stage/BlockSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace railstrip;

// ----------------------------------------------------------------
//  背景：マス目・目盛り・道
// ----------------------------------------------------------------
void StripRenderer::DrawBackdrop(const StripContext& context, const StripState& state, const StripLayout& layout){
#ifdef USE_IMGUI
    const SplineRail& rail = ( *context.rails )[state.currentRail];
    ImDrawList* draw = ImGui::GetWindowDrawList();

    const float left   = layout.originX;
    const float top    = layout.originY;
    const float right  = left + layout.width;
    const float bottom = top + layout.height;
    const float cellsLeft  = layout.DistToX(-0.5f);
    const float cellsRight = layout.DistToX(( float ) layout.lastCell + 0.5f);
    const float levelsTop  = layout.HeightToY(( float ) layout.levels);

    draw->AddRectFilled({ left, top }, { right, bottom }, IM_COL32(26, 28, 34, 255));

    // 段ごとの帯（1段おきに少し明るく）
    for ( int level = 0; level < layout.levels; ++level ) {
        if ( level % 2 == 1 ) continue;
        draw->AddRectFilled({ cellsLeft, layout.HeightToY(( float ) level + 1.0f) },
                            { cellsRight, layout.HeightToY(( float ) level) }, IM_COL32(255, 255, 255, 10));
    }
    // レールの外（始点より手前・終点より先）は暗くする
    draw->AddRectFilled({ cellsLeft, levelsTop }, { layout.DistToX(0.0f), layout.groundY }, IM_COL32(0, 0, 0, 90));
    draw->AddRectFilled({ layout.DistToX(layout.railLength), levelsTop }, { cellsRight, layout.groundY }, IM_COL32(0, 0, 0, 90));

    // マスの区切り線（5mごとに濃く）と目盛り
    for ( int cell = 0; cell <= layout.lastCell + 1; ++cell ) {
        float x = layout.DistToX(( float ) cell - 0.5f);
        bool strong = ( cell % 5 == 0 );
        draw->AddLine({ x, levelsTop }, { x, layout.groundY },
                      strong ? IM_COL32(255, 255, 255, 46) : IM_COL32(255, 255, 255, 18));
    }
    for ( int level = 0; level <= layout.levels; ++level ) {
        float y = layout.HeightToY(( float ) level);
        draw->AddLine({ cellsLeft, y }, { cellsRight, y }, IM_COL32(255, 255, 255, 22));
    }
    for ( int cell = 0; cell <= layout.lastCell; ++cell ) {
        bool labelled = ( cell % 5 == 0 ) || ( layout.cellPx >= 34.0f );
        float x = layout.DistToX(( float ) cell);
        draw->AddLine({ x, top + kRulerHeight - ( cell % 5 == 0 ? 7.0f : 3.0f ) }, { x, top + kRulerHeight },
                      IM_COL32(255, 255, 255, 120));
        if ( labelled ) {
            char text[16];
            std::snprintf(text, sizeof(text), ( cell % 5 == 0 ) ? "%dm" : "%d", cell);
            AddTextCentered(draw, { x, top + 7.0f }, IM_COL32(220, 220, 225, ( cell % 5 == 0 ) ? 255 : 130), text);
        }
    }

    // 道（レールの始点〜終点）。道なしのレールは枠だけにする
    const float groundLeft  = layout.DistToX(0.0f);
    const float groundRight = layout.DistToX(layout.railLength);
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
        float holeLeft  = layout.DistToX(std::clamp(hole.d0, 0.0f, layout.railLength));
        float holeRight = layout.DistToX(std::clamp(hole.d1, 0.0f, layout.railLength));
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
    ( void ) context; ( void ) state; ( void ) layout;
#endif
}

// ----------------------------------------------------------------
//  下の情報帯：道の高さの変化・他のレールとのつながり
// ----------------------------------------------------------------
StripRenderer::LinkResult StripRenderer::DrawRailInfo(const StripContext& context, const StripState& state,
                                                      const StripLayout& layout, bool linksActive){
    LinkResult result;
#ifdef USE_IMGUI
    const auto& rails = *context.rails;
    const SplineRail& rail = rails[state.currentRail];
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
            points[i] = { layout.DistToX(dist), bandBottom - t * ( bandBottom - bandTop ) };
        }
        draw->AddPolyline(points.data(), sampleCount, IM_COL32(120, 200, 255, 220), ImDrawFlags_None, 1.5f);
        char text[64];
        if ( range > 0.05f ) { std::snprintf(text, sizeof(text), "道の高さ %.1f〜%.1f m", lowest, highest); }
        else                 { std::snprintf(text, sizeof(text), "道の高さ %.1f m（平ら）", lowest); }
        draw->AddText({ layout.DistToX(0.0f) + 4.0f, layout.infoTop + kInfoHeight - 17.0f },
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
        // 何かを塗っている/動かしている途中はラベルに反応しない（ドラッグがラベルの上を通っただけでレールが移らないように）。
        //   その判定は呼び出し側で済ませて linksActive として渡される
        bool over = linksActive
            && io.MousePos.x >= boxMin.x && io.MousePos.x <= boxMax.x
            && io.MousePos.y >= boxMin.y && io.MousePos.y <= boxMax.y;
        if ( over ) { result.overLabel = true; }
        draw->AddRectFilled(boxMin, boxMax, over ? IM_COL32(60, 110, 170, 255) : IM_COL32(44, 60, 84, 255), 3.0f);
        draw->AddText({ boxMin.x + 4.0f, boxMin.y + 1.0f }, IM_COL32(230, 240, 255, 255), text);
        if ( over ) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            ImGui::SetTooltip("クリックで Rail %d を開く", targetRail);
            if ( ImGui::IsMouseClicked(ImGuiMouseButton_Left) ) {
                result.railSelect = targetRail;
                result.railDist   = arrivalDist;
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
    linkLabel(layout.DistToX(0.0f) - 2.0f - kPadLeft * 0.5f + 6.0f, layout.groundY - 8.0f, rail.frontConnIndex,
              arrivalOf(rail.frontConnIndex, rail.frontConnToFront), "<", "");
    linkLabel(layout.DistToX(layout.railLength) + kPadRight * 0.5f, layout.groundY - 8.0f, rail.backConnIndex,
              arrivalOf(rail.backConnIndex, rail.backConnToFront), "", ">");
    // 途中の分かれ道
    for ( const SplineRail::BranchPoint& branch : rail.branchPoints ) {
        float x = layout.DistToX(std::clamp(branch.distance, 0.0f, layout.railLength));
        draw->AddTriangleFilled({ x, layout.groundY + kGroundHeight - 1.0f },
                                { x - 5.0f, layout.groundY + 2.0f }, { x + 5.0f, layout.groundY + 2.0f },
                                IM_COL32(120, 200, 255, 255));
        linkLabel(x, linkY, branch.targetRail, branch.targetDist, "→", "");
    }
#else
    ( void ) context; ( void ) state; ( void ) layout; ( void ) linksActive;
#endif
    return result;
}

// ----------------------------------------------------------------
//  スタート・ゴール・プレイヤー
// ----------------------------------------------------------------
void StripRenderer::DrawMarkers(const StripContext& context, const StripState& state, const StripLayout& layout){
#ifdef USE_IMGUI
    ImDrawList* draw = ImGui::GetWindowDrawList();
    auto flag = [&](float dist, ImU32 color, const char* text) {
        float x = layout.DistToX(std::clamp(dist, 0.0f, layout.railLength));
        float poleTop = layout.HeightToY(1.6f);
        draw->AddLine({ x, layout.groundY }, { x, poleTop }, color, 2.0f);
        draw->AddTriangleFilled({ x, poleTop }, { x + 14.0f, poleTop + 6.0f }, { x, poleTop + 12.0f }, color);
        draw->AddText({ x + 3.0f, poleTop - 15.0f }, color, text);
    };
    if ( context.startRail == state.currentRail ) { flag(context.startDist, IM_COL32(110, 230, 120, 255), "スタート"); }
    if ( context.goalRail  == state.currentRail ) { flag(context.goalDist,  IM_COL32(255, 210, 80, 255),  "ゴール"); }

    // プレイ中のプレイヤー位置（このレールに乗っている時だけ）
    if ( context.hasPlayer && context.playerRail == state.currentRail ) {
        const SplineRail& rail = ( *context.rails )[state.currentRail];
        float dist = rail.GetClosestDistance(context.playerPos);
        float height = context.playerPos.y - rail.GetPositionByDistance(dist).y;
        float x = layout.DistToX(dist);
        float footY = layout.HeightToY(height);
        float headY = layout.HeightToY(height + 1.0f);
        float halfWidth = 0.35f * layout.cellPx;
        draw->AddRectFilled({ x - halfWidth, headY }, { x + halfWidth, footY }, IM_COL32(90, 220, 110, 200), halfWidth);
        draw->AddText({ x - 6.0f, headY - 15.0f }, IM_COL32(150, 255, 170, 255), "P");
    }
#else
    ( void ) context; ( void ) state; ( void ) layout;
#endif
}

// ----------------------------------------------------------------
//  ブロック
// ----------------------------------------------------------------
void StripRenderer::DrawBlocks(const StripContext& context, const StripState& state, const StripLayout& layout,
                               int movingBlock){
#ifdef USE_IMGUI
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const auto& blocks = context.railEditor->GetBlocks();

    // 他の横位置のブロックを先に薄く描き、編集中の横位置のブロックを上に描く
    for ( int pass = 0; pass < 2; ++pass ) {
        for ( int blockIndex = 0; blockIndex < ( int ) blocks.size(); ++blockIndex ) {
            const BlockData& block = blocks[blockIndex];
            if ( block.rail != state.currentRail ) continue;
            if ( block.level < 0 || block.level >= layout.levels ) continue;
            const bool onLayer = std::abs(block.side - ( float ) state.sideLayer) < 0.5f;
            if ( ( pass == 1 ) != onLayer ) continue;
            if ( !onLayer && !state.showOtherSides ) continue;

            // 移動中のブロックは元の場所に薄く残す（どこから動かしているかが分かる）
            const bool moving = ( blockIndex == movingBlock );
            const int alpha = ( !onLayer ) ? 60 : ( moving ? 90 : 255 );
            const float half = BlockSystem::VisualHalfAlong(block.type);
            const float shownDist = ShownDist(block.dist, layout.railLength);
            const float x0 = layout.DistToX(shownDist - half) + 1.0f;
            const float x1 = layout.DistToX(shownDist + half) - 1.0f;
            const float yBottom = layout.HeightToY(( float ) block.level) - 1.0f;
            const float yTop    = layout.HeightToY(( float ) block.level + 1.0f) + 1.0f;
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
    ( void ) context; ( void ) state; ( void ) layout; ( void ) movingBlock;
#endif
}

// ----------------------------------------------------------------
//  コイン
// ----------------------------------------------------------------
void StripRenderer::DrawCoins(const StripContext& context, const StripState& state, const StripLayout& layout,
                              int draggedCoin, const CoinData& draggedData){
#ifdef USE_IMGUI
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const auto& coins = context.railEditor->GetCoins();
    const float radius = CoinRadiusPx(layout.cellPx);
    for ( int i = 0; i < ( int ) coins.size(); ++i ) {
        // ドラッグ中のコインは、移動先の位置に描く
        const CoinData& coin = ( i == draggedCoin ) ? draggedData : coins[i];
        if ( coin.rail != state.currentRail ) continue;
        ImVec2 center = { layout.DistToX(ShownDist(coin.dist, layout.railLength)), layout.HeightToY(coin.height) };
        draw->AddCircleFilled(center, radius, IM_COL32(255, 214, 60, 255), 16);
        draw->AddCircle(center, radius, IM_COL32(150, 100, 10, 255), 16, 1.5f);
    }
#else
    ( void ) context; ( void ) state; ( void ) layout; ( void ) draggedCoin; ( void ) draggedData;
#endif
}

// ----------------------------------------------------------------
//  敵（体・番号と名前・動ける範囲）
// ----------------------------------------------------------------
void StripRenderer::DrawEnemies(const StripContext& context, const StripState& state, const StripLayout& layout){
#ifdef USE_IMGUI
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const auto& spawns = context.enemyEditor->GetSpawnDatas();
    const int selected = context.enemyEditor->GetSelectedEntry();
    const int listHovered = context.enemyEditor->GetHoveredEntry();
    const float rangeY = layout.groundY + kGroundHeight * 0.5f;

    for ( int i = 0; i < ( int ) spawns.size(); ++i ) {
        const EnemySpawnData& spawn = spawns[i];
        if ( spawn.railIndex != state.currentRail ) continue;
        const bool isSelected = ( i == selected );
        const float shownDist = ShownDist(spawn.distance, layout.railLength);
        const float x = layout.DistToX(shownDist);
        const float centerY = layout.HeightToY(EnemyCenterHeight(spawn));
        const float radius = EnemyRadiusPx(spawn, layout.cellPx);
        const ImU32 color = EnemyColor(spawn.type, 255);

        // 動ける範囲（橙）。範囲を決めていない巡回はレール全体を細い線で
        if ( EnemyHasRange(spawn) ) {
            float rangeMin = ( spawn.patrolMin >= 0.0f ) ? ( std::min )( spawn.patrolMin, layout.railLength ) : 0.0f;
            float rangeMax = ( spawn.patrolMax >= 0.0f ) ? std::clamp(spawn.patrolMax, rangeMin, layout.railLength) : layout.railLength;
            float xMin = layout.DistToX(rangeMin), xMax = layout.DistToX(rangeMax);
            draw->AddLine({ xMin, rangeY }, { xMax, rangeY }, IM_COL32(255, 156, 40, isSelected ? 255 : 170), isSelected ? 4.0f : 3.0f);
            const float handle = isSelected ? 6.0f : 4.5f;
            draw->AddRectFilled({ xMin - handle, rangeY - handle }, { xMin + handle, rangeY + handle }, IM_COL32(255, 156, 40, 255));
            draw->AddRectFilled({ xMax - handle, rangeY - handle }, { xMax + handle, rangeY + handle }, IM_COL32(255, 156, 40, 255));
        } else if ( spawn.patrol ) {
            draw->AddLine({ layout.DistToX(0.0f), rangeY }, { layout.DistToX(layout.railLength), rangeY },
                          IM_COL32(255, 156, 40, 90), 2.0f);
        }
        // 追いかける敵：気づく距離を道の上の赤い線で
        if ( spawn.chaseRange > 0.0f ) {
            float xMin = layout.DistToX(( std::max )( shownDist - spawn.chaseRange, 0.0f ));
            float xMax = layout.DistToX(( std::min )( shownDist + spawn.chaseRange, layout.railLength ));
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
    ( void ) context; ( void ) state; ( void ) layout;
#endif
}

// 段の番号。横へスクロールしても見えるよう、表示の左端に固定して描く
void StripRenderer::DrawLevelLabels(const StripLayout& layout){
#ifdef USE_IMGUI
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float left = ImGui::GetWindowPos().x + 2.0f;
    draw->AddRectFilled({ left, layout.HeightToY(( float ) layout.levels) }, { left + 24.0f, layout.groundY },
                        IM_COL32(26, 28, 34, 210));
    for ( int level = 0; level < layout.levels; ++level ) {
        char text[8];
        std::snprintf(text, sizeof(text), "%d", level + 1);
        AddTextCentered(draw, { left + 12.0f, layout.HeightToY(( float ) level + 0.5f) }, IM_COL32(200, 200, 210, 255), text);
    }
#else
    ( void ) layout;
#endif
}
