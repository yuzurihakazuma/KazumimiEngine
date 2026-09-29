#include "game/editor/strip/StripCoinTool.h"

#include "engine/graphics/DebugDraw.h"
#include "engine/rail/SplineRail.h"
#include "engine/utils/Level/LevelEditor.h"
#include "engine/utils/Level/RailEditor.h"
#include "externals/imgui/imgui.h"
#include "game/editor/strip/StripCommon.h"

#include <algorithm>
#include <cmath>

using namespace railstrip;

// =====================================================================
//  道具：コイン
// =====================================================================
void StripCoinTool::Update(const StripContext& context, const StripState& state, const StripLayout& layout, bool hovered){
#ifdef USE_IMGUI
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    RailEditor* railEditor = context.railEditor;
    const int currentRail = state.currentRail;
    const SplineRail& rail = ( *context.rails )[currentRail];
    const auto& coins = railEditor->GetCoins();

    const float mouseDist = layout.XToDist(io.MousePos.x);
    const float snappedDist = std::clamp(io.KeyAlt ? mouseDist : SnapTo(mouseDist, 0.5f), 0.0f, layout.railLength);
    // 高さは見えている段の中まで（はみ出したコインは展開図でつかめなくなるため）
    const float coinHeightMax = ( std::min )( kCoinMaxHeight, ( float ) layout.levels - 0.25f );
    const float snappedHeight = std::clamp(
        io.KeyAlt ? layout.YToHeight(io.MousePos.y) : SnapTo(layout.YToHeight(io.MousePos.y), 0.25f),
        0.0f, coinHeightMax);
    const bool inEditArea = hovered && io.MousePos.y >= layout.originY + kRulerHeight && io.MousePos.y < layout.groundY;
    const float radius = CoinRadiusPx(layout.cellPx);

    // コインを変えた後の後始末：Game View のコインを作り直し、未保存の印を付ける
    auto commitCoins = [&]() {
        if ( context.onCoinsChanged ) { context.onCoinsChanged(); }
        if ( context.levelEditor ) { context.levelEditor->MarkDirty(); }
    };

    // ---- ドラッグ中（確定は離した時。ドラッグ中は移動先を描くだけ＝軽い）----
    if ( dragIndex_ >= 0 ) {
        if ( dragIndex_ >= ( int ) coins.size() ) { dragIndex_ = -1; return; }
        if ( ImGui::IsMouseDown(ImGuiMouseButton_Left) ) {
            if ( ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f) ) {
                dragData_.rail   = currentRail;
                dragData_.dist   = snappedDist;
                dragData_.height = snappedHeight;
            }
            Vector3 railPoint = rail.GetPositionByDistance(dragData_.dist);
            DebugDraw::GetInstance()->Sphere({ railPoint.x, railPoint.y + dragData_.height, railPoint.z },
                                             0.35f, { 1.0f, 0.9f, 0.2f, 1.0f }, 12);
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            ImGui::SetTooltip("距離 %.1f m / 高さ %.2f m", dragData_.dist, dragData_.height);
        } else {
            // つかんだ時と中身が変わっていたら書き込まない（ドラッグ中の Ctrl+Z 等で別のコインを上書きしない）
            const bool sameCoin = ( coins[dragIndex_] == dragOrig_ );
            const bool moved = !( dragData_ == dragOrig_ );
            if ( sameCoin && moved ) {
                railEditor->SetCoinAt(dragIndex_, dragData_.rail, dragData_.dist, dragData_.height);
                commitCoins();
            }
            dragIndex_ = -1;
        }
        return;
    }

    if ( !inEditArea ) return;

    // ---- マウスの下のコイン ----
    int overCoin = -1;
    float bestGap = radius + 5.0f;
    for ( int i = 0; i < ( int ) coins.size(); ++i ) {
        if ( coins[i].rail != currentRail ) continue;
        float dx = layout.DistToX(ShownDist(coins[i].dist, layout.railLength)) - io.MousePos.x;
        float dy = layout.HeightToY(coins[i].height) - io.MousePos.y;
        float gap = std::sqrt(dx * dx + dy * dy);
        if ( gap < bestGap ) { bestGap = gap; overCoin = i; }
    }

    if ( overCoin >= 0 ) {
        const CoinData& coin = coins[overCoin];
        draw->AddCircle({ layout.DistToX(ShownDist(coin.dist, layout.railLength)), layout.HeightToY(coin.height) }, radius + 3.0f,
                        IM_COL32(255, 255, 255, 255), 16, 2.0f);
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::SetTooltip("コイン（距離 %.1f m / 高さ %.2f m）\nドラッグ=移動 / 右クリック=削除", coin.dist, coin.height);
        Vector3 railPoint = rail.GetPositionByDistance(coin.dist);
        DebugDraw::GetInstance()->Sphere({ railPoint.x, railPoint.y + coin.height, railPoint.z },
                                         0.45f, { 1.0f, 0.95f, 0.3f, 1.0f }, 12);
        if ( ImGui::IsMouseClicked(ImGuiMouseButton_Left) ) {
            dragIndex_ = overCoin;
            dragData_  = coin;
            dragOrig_  = coin;
        } else if ( ImGui::IsMouseClicked(ImGuiMouseButton_Right) ) {
            railEditor->RemoveCoinAt(overCoin);
            commitCoins();
        }
    } else if ( !rail.visible ) {
        // 見えないレール（連結用の骨組み）には新しく置かない。今あるコインの移動・削除はできる
        if ( mouseDist >= -0.25f && mouseDist <= layout.railLength + 0.25f ) {
            ImGui::SetTooltip("%s", kInvisibleRailMessage);
        }
    } else if ( mouseDist >= -0.25f && mouseDist <= layout.railLength + 0.25f ) {
        // ---- 何もない所：置く場所の予告 → クリックで置く ----
        ImVec2 center = { layout.DistToX(snappedDist), layout.HeightToY(snappedHeight) };
        draw->AddCircleFilled(center, radius, IM_COL32(255, 214, 60, 110), 16);
        draw->AddCircle(center, radius, IM_COL32(255, 255, 255, 200), 16, 1.5f);
        Vector3 railPoint = rail.GetPositionByDistance(snappedDist);
        DebugDraw::GetInstance()->Sphere({ railPoint.x, railPoint.y + snappedHeight, railPoint.z },
                                         0.35f, { 1.0f, 0.9f, 0.2f, 1.0f }, 12);
        if ( ImGui::IsMouseClicked(ImGuiMouseButton_Left) ) {
            railEditor->AddCoinAt(currentRail, snappedDist);
            int added = ( int ) railEditor->GetCoins().size() - 1;
            railEditor->SetCoinAt(added, currentRail, snappedDist, snappedHeight);
            commitCoins();
        }
    }
#else
    ( void ) context; ( void ) state; ( void ) layout; ( void ) hovered;
#endif
}
