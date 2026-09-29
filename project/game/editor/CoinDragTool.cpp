#include "CoinDragTool.h"
#include "game/editor/SceneEditContext.h"
#include "engine/rail/SplineRail.h"
#include "engine/utils/Level/LevelEditor.h"
#include "engine/utils/Level/RailEditor.h"
#include "engine/graphics/DebugDraw.h"
#include "externals/imgui/imgui.h"

#include <algorithm>
#include <cmath>

void CoinDragTool::Update(const SceneEditContext& context, bool blockedByOthers){
	const auto& view = context.View();
	const auto& coins = context.editor->GetEditorCoins();

	// マウス直下のコイン（画面距離14px以内）。敵のつかみ判定を優先（敵の16px圏内ではコインをつかまない）
	hoverIndex_ = -1;
	if ( view.hovered && !view.gizmoActive && dragIndex_ < 0 && !blockedByOthers
		&& context.FindEnemyNearMouse(16.0f) < 0 ) {
		float bestPick = 14.0f;
		for ( int i = 0; i < ( int ) coins.size(); ++i ) {
			Vector3 wp;
			if ( !context.RailPoint(coins[i].rail, coins[i].dist, coins[i].height, wp) ) continue;
			float d = context.ScreenDistanceToMouse(wp);
			if ( d < bestPick ) { bestPick = d; hoverIndex_ = i; }
		}
	}
	if ( hoverIndex_ >= 0 ) {
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
		Vector3 wp;
		if ( context.RailPoint(coins[hoverIndex_].rail, coins[hoverIndex_].dist, coins[hoverIndex_].height, wp) ) {
			DebugDraw::GetInstance()->Sphere(wp, 0.45f, { 1.0f, 0.95f, 0.3f, 1.0f }, 12);
		}
		if ( ImGui::IsMouseClicked(0) ) {
			dragIndex_ = hoverIndex_;
			dragData_  = coins[hoverIndex_];
			dragOrig_  = coins[hoverIndex_];
		}
	}

	UpdateDrag(context);

	if ( IsHovering() || IsDragging() ) { context.editor->SetExternalDragActive(true); }
}

// ドラッグ中：レール沿い移動（通常）／高さ調整（Shift）。ゴーストのみ動かす
void CoinDragTool::UpdateDrag(const SceneEditContext& context){
	if ( dragIndex_ < 0 ) return;
	const auto& coins = context.editor->GetEditorCoins();
	if ( dragIndex_ >= ( int ) coins.size() ) {
		dragIndex_ = -1; // ドラッグ中に削除された等
		return;
	}
	if ( !ImGui::IsMouseDown(0) ) {
		Commit(context);
		dragIndex_ = -1;
		return;
	}
	ImGuiIO& io = ImGui::GetIO();
	// ただのクリック（3px未満）では動かさない＝クリックしただけでコインが
	// 微妙にずれて保存対象になる事故を防ぐ。右クリックメニューが開いている間
	// （ゲームビュー外にマウスがある間）も移動先を更新しない
	const bool dragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f) && context.View().hovered;
	if ( dragging && io.KeyShift ) {
		// Shift＋上下ドラッグ＝高さ調整（上へ動かすと高くなる）
		dragData_.height = std::clamp(dragData_.height - io.MouseDelta.y * 0.02f, 0.0f, 5.0f);
	} else if ( dragging ) {
		// マウスにいちばん近いレール上の点へ移動（別レールへの乗せ替えも可。今のレールに吸い付く）
		SceneEditContext::RailPick pick;
		if ( context.PickRailPoint(dragData_.height, 60.0f, dragData_.rail, 12.0f, pick) ) {
			dragData_.rail = pick.rail;
			dragData_.dist = pick.dist;
		}
	}
	// ゴースト：移動先の黄コイン＋レール線からの高さの見える化（縦線）
	Vector3 base, pos;
	if ( context.RailPoint(dragData_.rail, dragData_.dist, 0.0f, base)
		&& context.RailPoint(dragData_.rail, dragData_.dist, dragData_.height, pos) ) {
		DebugDraw::GetInstance()->Sphere(pos, 0.35f, { 1.0f, 0.9f, 0.2f, 1.0f }, 12);
		DebugDraw::GetInstance()->Line(base, pos, { 1.0f, 0.9f, 0.2f, 0.7f });
	}
	// 高さ調整中は視覚的に分かるようにカーソルを上下矢印へ
	ImGui::SetMouseCursor(io.KeyShift ? ImGuiMouseCursor_ResizeNS : ImGuiMouseCursor_Hand);
}

// 離した：確定。ただし
//   ・つかんだ時と中身が変わっていたら書き込まない（ドラッグ中の Ctrl+Z 等で
//     配列が入れ替わり、別のコインを上書きしてしまう事故を防ぐ）
//   ・実際に動いていない（ただのクリック）なら何もしない
void CoinDragTool::Commit(const SceneEditContext& context){
	const auto& coins = context.editor->GetEditorCoins();
	const bool sameCoin = ( dragIndex_ < ( int ) coins.size() ) && ( coins[dragIndex_] == dragOrig_ );
	const bool movedCoin = ( dragData_.rail != dragOrig_.rail )
		|| std::abs(dragData_.dist - dragOrig_.dist) > 0.01f
		|| std::abs(dragData_.height - dragOrig_.height) > 0.01f;
	if ( !sameCoin || !movedCoin || !context.levelEditor ) return;
	context.railEditor->SetCoinAt(dragIndex_, dragData_.rail, dragData_.dist, dragData_.height);
	context.ResyncCoins();
	context.levelEditor->MarkDirty(); // コイン移動も[未保存]・自動保存の対象にする
}
