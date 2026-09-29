#include "EnemyGameViewTool.h"
#include "game/editor/SceneEditContext.h"
#include "game/enemy/Enemy.h"
#include "game/enemy/EnemyEditor.h"
#include "engine/rail/SplineRail.h"
#include "engine/graphics/DebugDraw.h"
#include "externals/imgui/imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
	// 種類別のピンの色（ドングリン=赤 / カミバナ=紫 / フワリン=水色）
	Vector4 PinColorOf(EnemyType type){
		if ( type == EnemyType::Zako )   { return { 1.0f, 0.35f, 0.25f, 1.0f }; }
		if ( type == EnemyType::Strong ) { return { 0.75f, 0.4f, 1.0f, 1.0f }; }
		return { 0.35f, 0.8f, 1.0f, 1.0f };
	}
}

void EnemyGameViewTool::Cancel(){
	dragIndex_ = -1;
	dragMoved_ = false;
	hoverIndex_ = -1;
}

void EnemyGameViewTool::Update(const SceneEditContext& context){
	if ( !context.enemyEditor ) { Cancel(); return; }
	EnemyEditor& enemyEditor = *context.enemyEditor;
	const auto& view = context.View();

	DrawPinsAndLabels(context);

	// マウス直下の敵を探す（スクリーン距離16px以内。ドラッグのつかみ判定）
	//   ブロック配置モード中はクリックをブロック側に譲る（敵をつかまない）
	hoverIndex_ = -1;
	if ( view.hovered && !view.gizmoActive && dragIndex_ < 0
		&& !context.editor->IsEditorBlockPaintMode() ) {
		hoverIndex_ = context.FindEnemyNearMouse(16.0f);
	}
	if ( hoverIndex_ >= 0 ) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); }

	// つかむ
	if ( hoverIndex_ >= 0 && ImGui::IsMouseClicked(0) ) {
		dragIndex_ = hoverIndex_;
		dragMoved_ = false;
		enemyEditor.SetSelectedEntry(hoverIndex_);
	}
	UpdateDrag(context);

	// 一覧でホバー/選択中の敵のハイライト（黄=ホバー / 水色=選択）
	const auto& spawnDatas = enemyEditor.GetSpawnDatas();
	auto drawMarker = [&](int index, const Vector4& color){
		if ( index < 0 || index >= ( int ) spawnDatas.size() ) return;
		Vector3 wp;
		if ( context.EnemyWorldPos(spawnDatas[index], wp) ) { DebugDraw::GetInstance()->Sphere(wp, 0.8f, color); }
	};
	drawMarker(enemyEditor.GetHoveredEntry(),  { 1.0f, 1.0f, 0.2f, 1.0f });
	drawMarker(enemyEditor.GetSelectedEntry(), { 0.3f, 0.8f, 1.0f, 1.0f });

	// 敵の上にいる間/ドラッグ中はレール編集のマウス操作を止める（両方掴む事故防止）。
	//   このツールが毎フレーム最初に値を決め、後のツールは true を足すだけ
	context.editor->SetExternalDragActive(IsDragging() || IsHovering());
}

// 種類別の色分けピン＋名札、追いかける距離の輪、行動範囲の橙線
void EnemyGameViewTool::DrawPinsAndLabels(const SceneEditContext& context){
	const EnemyEditor& enemyEditor = *context.enemyEditor;
	const auto& spawnDatas = enemyEditor.GetSpawnDatas();
	const auto& rails = context.Rails();
	const auto& view = context.View();

	// 名札は Game View の窓の中に描く（一番手前に描くと、Game View に重なったパネルや
	//   右クリックメニューの上にまで名札が乗ってしまう）。同じフレームで窓に入り直して追記する
	const bool gameViewOpen = ImGui::Begin("Game View");
	ImDrawList* labelDraw = gameViewOpen ? ImGui::GetWindowDrawList() : nullptr;
	if ( labelDraw ) {
		labelDraw->PushClipRect({ view.imgMin.x, view.imgMin.y },
			{ view.imgMin.x + view.imgSize.x, view.imgMin.y + view.imgSize.y }, true);
	}
	for ( int enemyIndex = 0; enemyIndex < ( int ) spawnDatas.size(); ++enemyIndex ) {
		const auto& spawnData = spawnDatas[enemyIndex];
		Vector3 wp;
		if ( !context.EnemyWorldPos(spawnData, wp) ) continue;
		if ( labelDraw ) {
			Vector2 labelPos;
			if ( context.ProjectToScreen({ wp.x, wp.y + 1.45f, wp.z }, labelPos)
				&& labelPos.x >= view.imgMin.x && labelPos.x <= view.imgMin.x + view.imgSize.x
				&& labelPos.y >= view.imgMin.y && labelPos.y <= view.imgMin.y + view.imgSize.y ) {
				char label[96];
				snprintf(label, sizeof(label), "#%02d %s", enemyIndex, EnemyEditor::DisplayName(spawnData).c_str());
				ImVec2 textSize = ImGui::CalcTextSize(label);
				ImVec2 textPos = { labelPos.x - textSize.x * 0.5f, labelPos.y - textSize.y };
				bool highlighted = ( enemyIndex == enemyEditor.GetSelectedEntry()
					|| enemyIndex == enemyEditor.GetHoveredEntry() );
				labelDraw->AddRectFilled({ textPos.x - 3.0f, textPos.y - 1.0f },
					{ textPos.x + textSize.x + 3.0f, textPos.y + textSize.y + 1.0f },
					highlighted ? IM_COL32(20, 70, 120, 220) : IM_COL32(0, 0, 0, 150), 3.0f);
				labelDraw->AddText(textPos, IM_COL32(255, 255, 255, 255), label);
			}
		}
		Vector4 pinColor = PinColorOf(spawnData.type);
		DebugDraw::GetInstance()->Line({ wp.x, wp.y + 0.4f, wp.z }, { wp.x, wp.y + 1.1f, wp.z }, pinColor);
		DebugDraw::GetInstance()->Sphere({ wp.x, wp.y + 1.2f, wp.z }, 0.16f, pinColor, 10);

		const SplineRail& rail = rails[spawnData.railIndex];
		// 追いかける敵：気づく距離を薄い赤の輪で見せる
		if ( spawnData.chaseRange > 0.0f ) {
			Vector3 railPoint = rail.GetPositionByDistance(spawnData.distance);
			const int kRingSegments = 32;
			Vector3 prevPoint {};
			for ( int s = 0; s <= kRingSegments; ++s ) {
				float angle = 6.2831853f * ( float ) s / ( float ) kRingSegments;
				Vector3 ringPoint = { railPoint.x + std::cos(angle) * spawnData.chaseRange,
				                      railPoint.y + 0.1f,
				                      railPoint.z + std::sin(angle) * spawnData.chaseRange };
				if ( s > 0 ) { DebugDraw::GetInstance()->Line(prevPoint, ringPoint, { 1.0f, 0.3f, 0.3f, 0.55f }); }
				prevPoint = ringPoint;
			}
		}
		// 行動範囲：レールに沿った橙線（範囲指定なしのパトロールはレール全体）
		const bool rangedMover = ( spawnData.chaseRange > 0.0f )
			&& ( spawnData.patrolMin >= 0.0f || spawnData.patrolMax >= 0.0f );
		if ( spawnData.patrol || rangedMover ) {
			float len = rail.GetLength();
			float lo = ( spawnData.patrolMin >= 0.0f ) ? ( std::min )( spawnData.patrolMin, len ) : 0.0f;
			float hi = ( spawnData.patrolMax >= 0.0f ) ? std::clamp(spawnData.patrolMax, lo, len) : len;
			int steps = std::clamp(( int ) ( ( hi - lo ) / 0.5f ), 1, 200);
			Vector3 prev = rail.GetPositionByDistance(lo);
			for ( int s = 1; s <= steps; ++s ) {
				Vector3 cur = rail.GetPositionByDistance(lo + ( hi - lo ) * ( float ) s / ( float ) steps);
				DebugDraw::GetInstance()->Line({ prev.x, prev.y + 0.15f, prev.z },
				                               { cur.x,  cur.y + 0.15f,  cur.z }, { 1.0f, 0.6f, 0.15f, 1.0f });
				prev = cur;
			}
		}
	}
	if ( labelDraw ) { labelDraw->PopClipRect(); }
	ImGui::End(); // Game View（名札の追記）。Begin が false でも必ず呼ぶ
}

// つかんだ敵をドラッグでレール上を移動（別レールへの乗せ替えも可）→ 離して確定
void EnemyGameViewTool::UpdateDrag(const SceneEditContext& context){
	if ( dragIndex_ < 0 ) return;
	EnemyEditor& enemyEditor = *context.enemyEditor;
	const auto& spawnDatas = enemyEditor.GetSpawnDatas();
	if ( dragIndex_ >= ( int ) spawnDatas.size() ) {
		dragIndex_ = -1; // ドラッグ中に削除された等
		return;
	}
	if ( !ImGui::IsMouseDown(0) ) {
		// マウスを離した：動かしていたら確定（リスポーン＋マップ保存データへ反映）
		if ( dragMoved_ ) { enemyEditor.MarkChanged(); }
		dragMoved_ = false;
		dragIndex_ = -1;
		return;
	}
	// ただのクリック（3px未満）では動かさない＝選んだだけで位置がずれない
	const bool dragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f) && context.View().hovered;
	if ( dragging ) {
		// マウスにいちばん近いレール上の点。今のレールに吸い付く（見えないレールでも今いるレールだけは候補）
		const int currentRail = spawnDatas[dragIndex_].railIndex;
		SceneEditContext::RailPick pick;
		if ( context.PickRailPoint(Enemy::PickHeightOf(spawnDatas[dragIndex_]), 40.0f, currentRail, 12.0f,
		                           pick, currentRail) ) {
			// 行動範囲つきの敵は範囲ごと一緒に動く
			enemyEditor.MoveEntry(dragIndex_, pick.rail, pick.dist, context.Rails());
			dragMoved_ = true;
		}
	}
	// ゴースト表示（確定はマウスを離した時。ドラッグ中の毎フレームリスポーンを避ける）
	Vector3 wp;
	if ( context.EnemyWorldPos(spawnDatas[dragIndex_], wp) ) {
		DebugDraw::GetInstance()->Sphere(wp, 0.9f, { 0.3f, 1.0f, 0.6f, 1.0f });
	}
	ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
}
