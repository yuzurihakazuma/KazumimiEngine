#include "BlockDragTool.h"
#include "game/editor/SceneEditContext.h"
#include "game/stage/BlockSystem.h"
#include "engine/rail/SplineRail.h"
#include "engine/utils/Level/RailEditor.h"
#include "engine/graphics/DebugDraw.h"
#include "externals/imgui/imgui.h"

#include <algorithm>
#include <cmath>

bool BlockDragTool::CellCenter(const SceneEditContext& context, int rail, float dist, int level, float side, Vector3& out){
	// 実際のブロックと同じ箱（道に沿って傾いた箱）の中心
	return context.blockSystem && context.blockSystem->CellCenter(rail, dist, level, side, out);
}

void BlockDragTool::Update(const SceneEditContext& context, bool blockedByOthers){
	const auto& view = context.View();
	const auto& blocks = context.editor->GetEditorBlocks();

	// マウス直下のブロックを探す（画面距離22px以内。黄枠＋手カーソルで教える）
	//   敵とコインのつかみが優先（重なった時に小さい対象を取れなくならないように）
	int hoverIndex = -1;
	if ( view.hovered && !view.gizmoActive && dragIndex_ < 0 && !blockedByOthers
		&& context.FindEnemyNearMouse(16.0f) < 0 ) {
		float bestPick = 22.0f;
		for ( int i = 0; i < ( int ) blocks.size(); ++i ) {
			Vector3 center;
			if ( !CellCenter(context, blocks[i].rail, blocks[i].dist, blocks[i].level, blocks[i].side, center) ) continue;
			float d = context.ScreenDistanceToMouse(center);
			if ( d < bestPick ) { bestPick = d; hoverIndex = i; }
		}
	}
	if ( hoverIndex >= 0 ) {
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
		const BlockData& hovered = blocks[hoverIndex];
		context.blockSystem->DrawCellGhost(hovered.rail, hovered.dist, hovered.level, hovered.side,
		                                   hovered.type, { 1.0f, 0.95f, 0.3f, 1.0f }, 0.03f);
		if ( ImGui::IsMouseClicked(0) ) {
			dragIndex_     = hoverIndex;
			dragOrig_      = hovered;
			dragCell_      = hovered;
			dragDuplicate_ = ImGui::GetIO().KeyCtrl; // Ctrl+ドラッグ＝複製
		}
	}

	UpdateDrag(context);

	// つかみ候補/ドラッグ中はレール編集のマウス操作を止める（敵側の判定を上書きしない＝trueのみ）
	if ( hoverIndex >= 0 || IsDragging() ) { context.editor->SetExternalDragActive(true); }
}

// ドラッグ中：マウスに一番近いセルを移動先候補にしてゴースト表示
void BlockDragTool::UpdateDrag(const SceneEditContext& context){
	if ( dragIndex_ < 0 ) return;
	const auto& blocks = context.editor->GetEditorBlocks();
	if ( dragIndex_ >= ( int ) blocks.size() ) {
		dragIndex_ = -1; // ドラッグ中に削除された等
		return;
	}
	if ( !ImGui::IsMouseDown(0) ) {
		Commit(context);
		Cancel();
		return;
	}

	// レール＋距離のピック（つかんだ段の高さで投影して高い段でも狙いやすく）
	SceneEditContext::RailPick pick;
	if ( context.PickRailPoint(( float ) dragCell_.level + 0.5f, 90.0f, dragCell_.rail, 20.0f, pick) ) {
		float railLen  = context.Rails()[pick.rail].GetLength();
		float cellDist = std::round(pick.dist);
		if ( cellDist < 0.0f )    { cellDist = 0.0f; }
		if ( cellDist > railLen ) { cellDist = std::floor(railLen); }
		dragCell_.rail = pick.rail;
		dragCell_.dist = cellDist;
		// 段×横ずれ：断面のセル（8段×5列）からマウスに一番近いものを選ぶ
		int bestLevel = dragCell_.level; float bestSide = dragCell_.side;
		float bestCellPx = 1e9f;
		for ( int level = 0; level < 8; ++level ) {
			for ( int sideStep = -2; sideStep <= 2; ++sideStep ) {
				Vector3 center;
				if ( !CellCenter(context, pick.rail, cellDist, level, ( float ) sideStep, center) ) continue;
				float d = context.ScreenDistanceToMouse(center) + std::abs(( float ) sideStep) * 10.0f;
				if ( d < bestCellPx ) { bestCellPx = d; bestLevel = level; bestSide = ( float ) sideStep; }
			}
		}
		dragCell_.level = bestLevel;
		dragCell_.side  = bestSide;
	}
	// Ctrlは押し直しでも切り替え可能（ドラッグ中に複製⇔移動を変えられる）
	dragDuplicate_ = ImGui::GetIO().KeyCtrl;

	// ゴースト：緑=移動できる / 赤=他のブロックで塞がっている / 水色=複製
	int occupiedBy = -1;
	if ( context.railEditor ) {
		occupiedBy = context.railEditor->FindBlock(dragCell_.rail, dragCell_.dist, dragCell_.level, dragCell_.side);
	}
	bool cellBlocked = ( occupiedBy >= 0 && occupiedBy != dragIndex_ );
	Vector3 ghostCenter;
	if ( CellCenter(context, dragCell_.rail, dragCell_.dist, dragCell_.level, dragCell_.side, ghostCenter) ) {
		Vector4 ghostColor;
		if ( cellBlocked )         { ghostColor = { 1.0f, 0.35f, 0.3f, 1.0f }; }
		else if ( dragDuplicate_ ) { ghostColor = { 0.4f, 0.85f, 1.0f, 1.0f }; } // 複製＝水色
		else                       { ghostColor = { 0.3f, 1.0f, 0.5f, 1.0f }; }
		context.blockSystem->DrawCellGhost(dragCell_.rail, dragCell_.dist, dragCell_.level, dragCell_.side,
		                                   dragOrig_.type, ghostColor, 0.0f);
		// 元の場所：移動なら薄枠（無くなる予定）、複製ならしっかり枠（残る）＋対応線
		Vector3 origCenter;
		if ( CellCenter(context, dragOrig_.rail, dragOrig_.dist, dragOrig_.level, dragOrig_.side, origCenter) ) {
			Vector4 origColor = dragDuplicate_ ? Vector4 { 1.0f, 1.0f, 1.0f, 0.9f }
			                                   : Vector4 { 1.0f, 1.0f, 1.0f, 0.35f };
			context.blockSystem->DrawCellGhost(dragOrig_.rail, dragOrig_.dist, dragOrig_.level, dragOrig_.side,
			                                   dragOrig_.type, origColor, -0.025f);
			DebugDraw::GetInstance()->Line(origCenter, ghostCenter, { 1.0f, 0.95f, 0.3f, 0.8f });
		}
	}
	ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
}

// 離した：確定。移動＝元を消して新セルへ / 複製(Ctrl)＝元を残して新セルへコピー
void BlockDragTool::Commit(const SceneEditContext& context){
	const BlockData& orig = dragOrig_;
	bool moved = ( dragCell_.rail != orig.rail || dragCell_.dist != orig.dist
		|| dragCell_.level != orig.level || dragCell_.side != orig.side );
	if ( !moved || !context.railEditor ) return;
	RailEditor* railEditor = context.railEditor;
	if ( !dragDuplicate_ ) {
		railEditor->RemoveBlock(orig.rail, orig.dist, orig.level, orig.side);
	}
	railEditor->AddBlock(dragCell_.rail, dragCell_.dist, dragCell_.level, dragCell_.side, orig.type);
	if ( !dragDuplicate_ ) {
		// 実際に入ったか＝末尾要素の一致で判定（塞がっていて拒否されたら元の場所へ復帰）
		const auto& afterBlocks = railEditor->GetBlocks();
		bool placed = !afterBlocks.empty()
			&& afterBlocks.back().rail  == dragCell_.rail
			&& afterBlocks.back().dist  == dragCell_.dist
			&& afterBlocks.back().level == dragCell_.level
			&& afterBlocks.back().side  == dragCell_.side
			&& afterBlocks.back().type  == orig.type;
		if ( !placed ) {
			railEditor->AddBlock(orig.rail, orig.dist, orig.level, orig.side, orig.type);
		}
	}
}
