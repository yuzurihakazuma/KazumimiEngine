#include "game/editor/strip/StripEnemyTool.h"

#include "engine/graphics/DebugDraw.h"
#include "engine/rail/SplineRail.h"
#include "engine/utils/Level/RailEditor.h"
#include "externals/imgui/imgui.h"
#include "game/editor/strip/StripCommon.h"
#include "game/enemy/EnemyEditor.h"

#include <algorithm>
#include <cmath>

using namespace railstrip;

namespace {
#ifdef USE_IMGUI
    // 動ける範囲の両端（決めていない側はレールの端）。■の当たりとつかんだ位置に使う
    void RangeEndsOf(const EnemySpawnData& spawn, float railLength, float& outMin, float& outMax){
        outMin = ( spawn.patrolMin >= 0.0f ) ? spawn.patrolMin : 0.0f;
        outMax = ( spawn.patrolMax >= 0.0f ) ? spawn.patrolMax : railLength;
    }
    // 浮く高さの上限：見えている段の中までにする（マウスを展開図の上へはみ出させても、見えない高さへ行かない）
    float HoverMaxFor(float bodyMid, int levels){
        return ( std::min )( kHoverMaxHeight,
            ( std::max )( 0.0f, std::floor(( ( float ) levels - bodyMid ) / 0.25f) * 0.25f ) );
    }
#endif
}

// 途中のドラッグをやめる
void StripEnemyTool::Cancel(){
    drag_ = Drag::None;
    dragIndex_ = -1;
    hoverAdjust_ = false;
    rangeAmbiguous_ = false;
}

void StripEnemyTool::SelectEnemy(const StripContext& context, StripState& state, int index){
    context.enemyEditor->SetSelectedEntry(index);
    RememberSelection(context, state);
}

void StripEnemyTool::RememberSelection(const StripContext& context, StripState& state){
    state.lastSelectedEnemy = context.enemyEditor->GetSelectedEntry();
}

// 敵の道具の間：展開図の上での Ctrl+Z / Ctrl+Y は敵の履歴へ回す（レールの履歴は動かさない）。
//   キーを押したフレームだけでなく、マウスが乗っている間ずっと譲る
//   （レール側は DirectInput、こちらは ImGui でキーを見ていて、押した瞬間のフレームがずれ得るため）
bool StripEnemyTool::HandleUndoKeys(const StripContext& context){
    bool changed = false;
#ifdef USE_IMGUI
    ImGuiIO& io = ImGui::GetIO();
    context.railEditor->SkipUndoHotkeyThisFrame();
    if ( !io.WantTextInput && io.KeyCtrl && !ImGui::IsMouseDown(ImGuiMouseButton_Left) ) {
        if ( ImGui::IsKeyPressed(ImGuiKey_Z, false) ) { context.enemyEditor->Undo(); changed = true; }
        if ( ImGui::IsKeyPressed(ImGuiKey_Y, false) ) { context.enemyEditor->Redo(); changed = true; }
    }
#else
    ( void ) context;
#endif
    return changed;
}

// =====================================================================
//  道具：敵
// =====================================================================
bool StripEnemyTool::Update(const StripContext& context, StripState& state, const StripLayout& layout, bool hovered){
#ifdef USE_IMGUI
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    EnemyEditor* enemyEditor = context.enemyEditor;
    const auto& rails = *context.rails;
    const int currentRail = state.currentRail;
    const SplineRail& rail = rails[currentRail];
    auto& spawns = enemyEditor->MutableSpawnDatas();

    const float mouseDist   = layout.XToDist(io.MousePos.x);
    const float mouseHeight = layout.YToHeight(io.MousePos.y);
    // Alt を押している間は細かく（0.5m刻みに吸着しない）
    const float snappedDist = std::clamp(io.KeyAlt ? mouseDist : SnapTo(mouseDist, 0.5f), 0.0f, layout.railLength);
    const bool inEditArea = hovered && io.MousePos.y >= layout.originY + kRulerHeight && io.MousePos.y < layout.infoTop;
    const float rangeY = layout.groundY + kGroundHeight * 0.5f;

    // ---- ドラッグ中 ----
    if ( drag_ != Drag::None ) {
        if ( dragIndex_ < 0 || dragIndex_ >= ( int ) spawns.size() ) {
            return true; // ドラッグ中に消された等（途中の操作は呼び出し側で全部やめる）
        }
        if ( ImGui::IsMouseDown(ImGuiMouseButton_Left) ) {
            EnemySpawnData& spawn = spawns[dragIndex_];
            const EnemySpawnData before = spawn;
            if ( drag_ == Drag::Move ) {
                // ただのクリック（3px未満）では動かさない＝選んだだけで位置がずれない
                if ( ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f) ) {
                    enemyEditor->MoveEntry(dragIndex_, currentRail, snappedDist, rails); // 範囲つきは範囲ごと動く
                    // フワリン：上下に動かすと浮く高さが変わる（少し動かしただけでは変えない）
                    if ( spawn.type == EnemyType::Air ) {
                        if ( std::abs(io.MousePos.y - grabMouseY_) > layout.cellPx * 0.4f ) { hoverAdjust_ = true; }
                        if ( hoverAdjust_ ) {
                            const float bodyMid = EnemyBodyMid(spawn);
                            spawn.hoverHeight = std::clamp(SnapTo(mouseHeight - bodyMid, 0.25f), 0.0f,
                                                           HoverMaxFor(bodyMid, layout.levels));
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
                            drag_ = ( dragX > 0.0f ) ? Drag::RangeMax : Drag::RangeMin;
                            rangeAmbiguous_ = false;
                        }
                    }
                }
                if ( !rangeAmbiguous_ && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f) ) {
                    float handleDist = mouseDist + rangeGrabOffset_;
                    handleDist = std::clamp(io.KeyAlt ? handleDist : SnapTo(handleDist, 0.5f), 0.0f, layout.railLength);
                    if ( spawn.patrolMin < 0.0f ) { spawn.patrolMin = 0.0f; }
                    if ( spawn.patrolMax < 0.0f ) { spawn.patrolMax = layout.railLength; }
                    if ( drag_ == Drag::RangeMin ) { spawn.patrolMin = ( std::min )( handleDist, spawn.distance ); }
                    else                           { spawn.patrolMax = ( std::max )( handleDist, spawn.distance ); }
                }
            }
            if ( !( spawn == before ) ) {
                enemyEditor->MarkChanged(); // Game View の敵もその場で動く（実体は使い回すので軽い）
            }
            ImGui::SetMouseCursor(drag_ == Drag::Move ? ImGuiMouseCursor_Hand : ImGuiMouseCursor_ResizeEW);
            ImGui::SetTooltip("距離 %.1f m%s", spawn.distance,
                ( spawn.type == EnemyType::Air && hoverAdjust_ ) ? " / 浮く高さを変更中" : "");
        } else {
            Cancel();
        }
        return false;
    }

    if ( !inEditArea ) return false;

    // ---- マウスの下にあるもの：範囲の端（■）→ 敵の体 の順に探す ----
    const int selected = enemyEditor->GetSelectedEntry();
    int  overEnemy = -1;
    int  overHandleEnemy = -1;
    Drag overHandle = Drag::None;
    {
        float bestHandle = 8.0f;
        for ( int i = 0; i < ( int ) spawns.size(); ++i ) {
            const EnemySpawnData& spawn = spawns[i];
            if ( spawn.railIndex != currentRail || !EnemyHasRange(spawn) ) continue;
            float rangeMin, rangeMax;
            RangeEndsOf(spawn, layout.railLength, rangeMin, rangeMax);
            const float ends[2] = { rangeMin, rangeMax };
            for ( int side = 0; side < 2; ++side ) {
                float dx = layout.DistToX(ShownDist(ends[side], layout.railLength)) - io.MousePos.x;
                float dy = rangeY - io.MousePos.y;
                float gap = std::sqrt(dx * dx + dy * dy);
                if ( i == selected ) { gap -= 3.0f; } // 選んでいる敵の範囲を優先
                if ( gap < bestHandle ) {
                    bestHandle = gap;
                    overHandleEnemy = i;
                    overHandle = ( side == 0 ) ? Drag::RangeMin : Drag::RangeMax;
                }
            }
        }
        if ( overHandleEnemy < 0 ) {
            float bestBody = 1e9f;
            for ( int i = 0; i < ( int ) spawns.size(); ++i ) {
                const EnemySpawnData& spawn = spawns[i];
                if ( spawn.railIndex != currentRail ) continue;
                float radius = EnemyRadiusPx(spawn, layout.cellPx);
                // 描いている場所（範囲の外の敵は端に寄せて描いている）と同じ場所で当たりを取る
                float dx = layout.DistToX(ShownDist(spawn.distance, layout.railLength)) - io.MousePos.x;
                float dy = layout.HeightToY(EnemyCenterHeight(spawn)) - io.MousePos.y;
                float gap = std::sqrt(dx * dx + dy * dy);
                if ( gap <= radius + 5.0f && gap < bestBody ) { bestBody = gap; overEnemy = i; }
            }
        }
    }

    if ( overHandleEnemy >= 0 ) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        ImGui::SetTooltip("#%02d の動ける範囲（ドラッグで変更）", overHandleEnemy);
        if ( ImGui::IsMouseClicked(ImGuiMouseButton_Left) ) {
            SelectEnemy(context, state, overHandleEnemy);
            float rangeMin, rangeMax;
            RangeEndsOf(spawns[overHandleEnemy], layout.railLength, rangeMin, rangeMax);
            const float handleDist = ShownDist(( overHandle == Drag::RangeMin ) ? rangeMin : rangeMax, layout.railLength);
            rangeGrabOffset_ = handleDist - mouseDist;
            // 両端が重なっている時は、どちらをつかんだかをドラッグした向きで決める
            rangeAmbiguous_ = std::abs(rangeMax - rangeMin) < 0.01f;
            drag_ = overHandle;
            dragIndex_ = overHandleEnemy;
        }
        return false;
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
            SelectEnemy(context, state, overEnemy);
            drag_ = Drag::Move;
            dragIndex_ = overEnemy;
            hoverAdjust_ = false;
            grabMouseY_ = io.MousePos.y;
        } else if ( ImGui::IsMouseClicked(ImGuiMouseButton_Right) ) {
            SelectEnemy(context, state, overEnemy);
            contextEnemy_ = overEnemy;
            ImGui::OpenPopup("##stripEnemyMenu");
        }
    } else if ( !rail.visible ) {
        // 見えないレール（連結用の骨組み）には新しく置かない。今いる敵の移動・削除はできる
        if ( mouseDist >= -0.25f && mouseDist <= layout.railLength + 0.25f ) {
            ImGui::SetTooltip("%s", kInvisibleRailMessage);
        }
    } else if ( mouseDist >= -0.25f && mouseDist <= layout.railLength + 0.25f ) {
        // ---- 何もない所：置く場所の予告 → クリックで置く ----
        EnemySpawnData placing;
        placing.type      = state.enemyType;
        placing.railIndex = currentRail;
        // レール端1mには置かない（始点＝プレイヤーのスタート地点に敵が重なる事故を防ぐ）
        placing.distance  = std::clamp(snappedDist, ( std::min )( 1.0f, layout.railLength * 0.5f ),
                                       ( std::max )( layout.railLength - 1.0f, layout.railLength * 0.5f ));
        if ( placing.type == EnemyType::Air ) {
            // フワリンはマウスの高さに浮かせて置く（道のすぐ上を指した時は種類の既定の高さ）
            float hover = SnapTo(mouseHeight - EnemyBodyMid(placing), 0.25f);
            const float hoverMax = HoverMaxFor(EnemyBodyMid(placing), layout.levels);
            if ( hover >= 0.5f ) { placing.hoverHeight = ( std::min )( hover, hoverMax ); }
        }
        const float x = layout.DistToX(placing.distance);
        const float centerY = layout.HeightToY(EnemyCenterHeight(placing));
        const float radius = EnemyRadiusPx(placing, layout.cellPx); // 置く敵の大きさは既定（倍率1）
        draw->AddCircleFilled({ x, centerY }, radius, EnemyColor(placing.type, 110), 20);
        draw->AddCircle({ x, centerY }, radius, IM_COL32(255, 255, 255, 200), 20, 1.5f);
        Vector3 railPoint = rail.GetPositionByDistance(placing.distance);
        DebugDraw::GetInstance()->Sphere({ railPoint.x, railPoint.y + EnemyCenterHeight(placing), railPoint.z },
                                         0.5f, { 0.3f, 1.0f, 0.6f, 1.0f });
        if ( ImGui::IsMouseClicked(ImGuiMouseButton_Left) ) {
            enemyEditor->AddEntry(placing); // 置いた敵は選択状態になる＝敵エディタですぐ動きを決められる
            RememberSelection(context, state);
        }
    }

    // Delete：選んでいる敵（このレールの敵）を消す
    if ( ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !io.WantTextInput
        && selected >= 0 && selected < ( int ) spawns.size() && spawns[selected].railIndex == currentRail ) {
        enemyEditor->RemoveEntry(selected);
        RememberSelection(context, state);
    }
#else
    ( void ) context; ( void ) state; ( void ) layout; ( void ) hovered;
#endif
    return false;
}

// 敵の右クリックメニュー
void StripEnemyTool::DrawContextMenu(const StripContext& context, StripState& state){
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
        SelectEnemy(context, state, index);
        state.openEnemyPanelPending = true;
    }
    if ( ImGui::MenuItem("カメラをここへ") ) {
        state.RequestFocus(spawns[index].railIndex, spawns[index].distance, EnemyCenterHeight(spawns[index]));
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
        RememberSelection(context, state);
        contextEnemy_ = -1;
    } else if ( ImGui::MenuItem("削除") ) {
        enemyEditor->RemoveEntry(index);
        RememberSelection(context, state);
        contextEnemy_ = -1;
    }
    ImGui::EndPopup();
#else
    ( void ) context; ( void ) state;
#endif
}
