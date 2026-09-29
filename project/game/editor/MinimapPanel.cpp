#include "MinimapPanel.h"
#include "game/editor/SceneEditContext.h"
#include "game/camera/EditorCameraUtil.h"
#include "game/enemy/EnemyEditor.h"
#include "game/player/Player.h"
#include "game/rail/RailField.h"
#include "engine/rail/SplineRail.h"
#include "engine/camera/Camera.h"
#include "engine/utils/Level/RailEditor.h"

#include <algorithm>
#include <cmath>

void MinimapPanel::Draw(const SceneEditContext& context){
	// Beginがfalse（畳まれている/非アクティブなドックタブ）の間は本体を描かない。
	//   描いてしまうと IsItemClicked がタイトルバーに誤反応し、畳んだ状態でタイトルを
	//   クリックするたびカメラが変な場所へ飛ぶ（LastItemDataがタイトルのまま残るため）
	if ( ImGui::Begin("ミニマップ (俯瞰)") ) {
		ImGui::TextDisabled("線に触れる=選択 / 点をドラッグ=移動 / 空クリック=カメラ / ホイール=ズーム");
		ImGui::SameLine();
		if ( ImGui::SmallButton("全体表示##mmFit") ) { zoom_ = 1.0f; panX_ = 0.0f; panY_ = 0.0f; }
		UpdateBounds(context);
		if ( !hasBounds_ ) {
			ImGui::TextDisabled("レールがありません");
		} else {
			DrawCanvas(context);
		}
	}
	ImGui::End();
}

// レール全体のXZ範囲（ノード基準＋余白）。ノードドラッグ中は凍結（表示が動くと点が飛ぶため）
void MinimapPanel::UpdateBounds(const SceneEditContext& context){
	if ( dragNode_ >= 0 ) return;
	bool hasBounds = false;
	float minX = 0.0f, maxX = 0.0f, minZ = 0.0f, maxZ = 0.0f;
	for ( const auto& rail : context.Rails() ) {
		if ( rail.nodes.size() < 2 || !rail.visible ) continue;
		for ( const auto& node : rail.nodes ) {
			if ( !hasBounds ) { minX = maxX = node.x; minZ = maxZ = node.z; hasBounds = true; continue; }
			minX = ( std::min )( minX, node.x ); maxX = ( std::max )( maxX, node.x );
			minZ = ( std::min )( minZ, node.z ); maxZ = ( std::max )( maxZ, node.z );
		}
	}
	hasBounds_ = hasBounds;
	if ( hasBounds ) {
		minX_ = minX - 4.0f; maxX_ = maxX + 4.0f;
		minZ_ = minZ - 4.0f; maxZ_ = maxZ + 4.0f;
	}
}

void MinimapPanel::DrawCanvas(const SceneEditContext& context){
	RailEditor* railEditor = context.railEditor;
	const bool editing = context.editing;

	ImVec2 avail = ImGui::GetContentRegionAvail();
	MapView map;
	map.canvasSize = { ( std::max )( avail.x, 120.0f ), ( std::max )( avail.y, 120.0f ) };
	ImGui::InvisibleButton("minimap_canvas", map.canvasSize);
	const bool hovered = ImGui::IsItemHovered();
	map.canvasMin = ImGui::GetItemRectMin();
	ImVec2 canvasMax { map.canvasMin.x + map.canvasSize.x, map.canvasMin.y + map.canvasSize.y };
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(map.canvasMin, canvasMax, IM_COL32(24, 30, 40, 235), 4.0f);
	draw->PushClipRect(map.canvasMin, canvasMax, true);

	float spanX = ( std::max )( maxX_ - minX_, 0.001f );
	float spanZ = ( std::max )( maxZ_ - minZ_, 0.001f );
	map.fitScale  = ( std::min )( ( map.canvasSize.x - 12.0f ) / spanX, ( map.canvasSize.y - 12.0f ) / spanZ );
	map.scale     = map.fitScale * zoom_;
	map.worldCx   = ( minX_ + maxX_ ) * 0.5f;
	map.worldCz   = ( minZ_ + maxZ_ ) * 0.5f;
	map.centerPxX = map.canvasMin.x + map.canvasSize.x * 0.5f + panX_;
	map.centerPxY = map.canvasMin.y + map.canvasSize.y * 0.5f + panY_;

	const int currentRail = railEditor ? railEditor->GetCurrentRailIndex() : -1;
	const int hoverRail = DrawRails(context, map, draw, editing && hovered, currentRail);
	DrawObjects(context, map, draw);
	int hoverNode = -1;
	if ( editing && railEditor && currentRail >= 0 ) {
		hoverNode = UpdateNodeHandles(context, map, draw, hovered, railEditor, currentRail);
	} else if ( dragNode_ >= 0 ) {
		dragNode_ = -1;
		dragRail_ = -1;
	}

	// レールに触れる＝選択（固定中は他レールへ切り替えない）
	if ( editing && railEditor && hovered && hoverNode < 0 && dragNode_ < 0 && hoverRail >= 0 ) {
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
		ImGui::SetTooltip("路線%d%s", hoverRail, hoverRail == currentRail ? "（選択中）" : "（クリックで選択）");
		if ( ImGui::IsMouseClicked(0) && !( railEditor->IsEditTargetLocked() && hoverRail != currentRail ) ) {
			railEditor->SelectWholeRail(hoverRail);
		}
	}
	draw->PopClipRect();

	UpdateZoomPan(map, hovered);

	// 空クリック：クリック地点が画面の中央に来るようにカメラを移動（向きと高さは今のまま）
	if ( ImGui::IsItemClicked() && context.camera && hoverRail < 0 && hoverNode < 0 && dragNode_ < 0 ) {
		ImVec2 mouse = ImGui::GetMousePos();
		EditorCameraUtil::CenterOnXZ(*context.camera, map.ToWorldX(mouse.x), map.ToWorldZ(mouse.y));
	}
}

int MinimapPanel::DrawRails(const SceneEditContext& context, const MapView& map, ImDrawList* draw,
                            bool canPick, int currentRail){
	const auto& rails = context.Rails();
	const ImVec2 mouse = ImGui::GetMousePos();
	int hoverRail = -1; float bestRailPx = 8.0f;
	for ( int railIndex = 0; railIndex < ( int ) rails.size(); ++railIndex ) {
		const SplineRail& rail = rails[railIndex];
		if ( rail.nodes.size() < 2 ) continue;
		bool isGuideSkeleton = false;
		if ( !rail.visible ) {
			// 非表示レールのうち、ガイドとして使われている骨組みだけは表示する
			for ( const auto& other : rails ) {
				if ( other.motionType == 3 && other.guideRail == railIndex ) { isGuideSkeleton = true; break; }
			}
			if ( !isGuideSkeleton ) continue;
		}
		float len = rail.GetLength();
		if ( len <= 0.0f ) continue;
		ImU32 lineColor; float thickness = 2.0f;
		if ( railIndex == currentRail ) { lineColor = IM_COL32(255, 220, 80, 255); thickness = 3.0f; }
		else if ( isGuideSkeleton )     { lineColor = IM_COL32(255, 165, 60, 190); thickness = 1.5f; }
		else if ( rail.HasMotion() )    { lineColor = IM_COL32(120, 180, 255, 220); }
		else                            { lineColor = IM_COL32(230, 230, 230, 170); }
		int steps = std::clamp(( int ) ( len * 0.5f ), 2, 64);
		ImVec2 prev {};
		for ( int s = 0; s <= steps; ++s ) {
			Vector3 p = rail.GetPositionByDistance(len * ( float ) s / ( float ) steps);
			ImVec2 c = map.ToCanvas(p.x + rail.animOffset.x, p.z + rail.animOffset.z);
			if ( s > 0 ) {
				draw->AddLine(prev, c, lineColor, thickness);
				// マウス→線分の距離でホバー判定（編集モード時のみ）
				if ( canPick ) {
					float vx = c.x - prev.x, vy = c.y - prev.y;
					float len2 = vx * vx + vy * vy;
					float t = ( len2 > 1e-5f )
						? std::clamp(( ( mouse.x - prev.x ) * vx + ( mouse.y - prev.y ) * vy ) / len2, 0.0f, 1.0f)
						: 0.0f;
					float dx = prev.x + vx * t - mouse.x, dy = prev.y + vy * t - mouse.y;
					float d = std::sqrt(dx * dx + dy * dy);
					if ( d < bestRailPx ) { bestRailPx = d; hoverRail = railIndex; }
				}
			}
			prev = c;
		}
	}
	return hoverRail;
}

// ブロック（茶の四角）／コイン（黄の丸）／敵（種類で色分け）／スタート・ゴール／プレイヤー・カメラ
void MinimapPanel::DrawObjects(const SceneEditContext& context, const MapView& map, ImDrawList* draw){
	Vector3 p;
	for ( const auto& block : context.editor->GetEditorBlocks() ) {
		if ( !context.RailPoint(block.rail, block.dist, 0.0f, p) ) continue;
		ImVec2 c = map.ToCanvas(p.x, p.z);
		draw->AddRectFilled({ c.x - 2.5f, c.y - 2.5f }, { c.x + 2.5f, c.y + 2.5f }, IM_COL32(205, 140, 70, 255));
	}
	for ( const auto& coin : context.editor->GetEditorCoins() ) {
		if ( !context.RailPoint(coin.rail, coin.dist, 0.0f, p) ) continue;
		draw->AddCircleFilled(map.ToCanvas(p.x, p.z), 2.5f, IM_COL32(255, 215, 60, 255));
	}
	if ( context.enemyEditor ) {
		for ( const auto& spawnData : context.enemyEditor->GetSpawnDatas() ) {
			if ( !context.RailPoint(spawnData.railIndex, spawnData.distance, 0.0f, p) ) continue;
			ImU32 pinColor;
			if      ( spawnData.type == EnemyType::Zako )   { pinColor = IM_COL32(255, 90, 70, 255); }
			else if ( spawnData.type == EnemyType::Strong ) { pinColor = IM_COL32(190, 100, 255, 255); }
			else                                            { pinColor = IM_COL32(90, 205, 255, 255); }
			draw->AddCircleFilled(map.ToCanvas(p.x, p.z), 3.5f, pinColor);
		}
	}
	// スタート（緑の輪）／ゴール（橙の輪）
	if ( context.railField ) {
		if ( context.RailPoint(context.railField->GetStartRail(), context.railField->GetStartDistance(), 0.0f, p) ) {
			draw->AddCircle(map.ToCanvas(p.x, p.z), 5.0f, IM_COL32(90, 255, 120, 255), 0, 2.0f);
		}
		if ( context.railField->HasGoal() ) {
			Vector3 goalPos = context.railField->GetGoalPos();
			draw->AddCircle(map.ToCanvas(goalPos.x, goalPos.z), 5.0f, IM_COL32(255, 170, 60, 255), 0, 2.0f);
		}
	}
	// プレイヤー（白の点）とカメラ（水色の菱形）
	if ( context.player ) {
		const Vector3& playerPos = context.player->GetPosition();
		draw->AddCircleFilled(map.ToCanvas(playerPos.x, playerPos.z), 3.0f, IM_COL32(255, 255, 255, 255));
	}
	if ( context.camera ) {
		Vector3 camPos = context.camera->GetWorldPosition();
		ImVec2 c = map.ToCanvas(camPos.x, camPos.z);
		draw->AddQuadFilled({ c.x, c.y - 4.0f }, { c.x + 4.0f, c.y },
		                    { c.x, c.y + 4.0f }, { c.x - 4.0f, c.y }, IM_COL32(120, 220, 255, 230));
	}
}

// 選択レールのノード：ミニマップ上で直接ドラッグしてXZ移動できる（高さは保持）
int MinimapPanel::UpdateNodeHandles(const SceneEditContext& context, const MapView& map, ImDrawList* draw,
                                    bool hovered, RailEditor* railEditor, int currentRail){
	const ImVec2 mouse = ImGui::GetMousePos();
	const int nodeCount = railEditor->GetNodeCountOf(currentRail);
	int hoverNode = -1;
	if ( hovered && dragNode_ < 0 ) {
		float bestNodePx = 9.0f;
		for ( int i = 0; i < nodeCount; ++i ) {
			Vector3 np;
			if ( !railEditor->GetNodePosOf(currentRail, i, np) ) continue;
			ImVec2 c = map.ToCanvas(np.x, np.z);
			float dx = c.x - mouse.x, dy = c.y - mouse.y;
			float d = std::sqrt(dx * dx + dy * dy);
			if ( d < bestNodePx ) { bestNodePx = d; hoverNode = i; }
		}
	}
	for ( int i = 0; i < nodeCount; ++i ) {
		Vector3 np;
		if ( !railEditor->GetNodePosOf(currentRail, i, np) ) continue;
		ImVec2 c = map.ToCanvas(np.x, np.z);
		const bool active = ( i == hoverNode || i == dragNode_ );
		draw->AddCircleFilled(c, active ? 5.0f : 3.5f,
			active ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 220, 80, 255));
		draw->AddCircle(c, active ? 5.0f : 3.5f, IM_COL32(40, 40, 20, 255), 0, 1.2f);
	}
	// つかむ
	if ( hovered && hoverNode >= 0 && ImGui::IsMouseClicked(0) ) {
		dragRail_ = currentRail;
		dragNode_ = hoverNode;
	}
	// ドラッグ中：マウスのワールドXZへ移動（表示は凍結済みなので飛ばない）
	if ( dragNode_ >= 0 ) {
		if ( dragRail_ != currentRail || dragNode_ >= nodeCount || !ImGui::IsMouseDown(0) ) {
			dragNode_ = -1;
			dragRail_ = -1;
		} else {
			float worldX = map.ToWorldX(mouse.x);
			float worldZ = map.ToWorldZ(mouse.y);
			Vector3 np;
			if ( railEditor->GetNodePosOf(currentRail, dragNode_, np) ) {
				railEditor->SetNodePosOf(currentRail, dragNode_, { worldX, np.y, worldZ });
				context.editor->SetGameViewGuideDragging(true); // 道の再生成を10Hzに間引く
				ImGui::SetTooltip("X=%.1f Z=%.1f（高さ%.1fは保持）", worldX, worldZ, np.y);
			}
		}
	}
	return hoverNode;
}

// ズーム（カーソル位置基準）＆中ボタンパン
void MinimapPanel::UpdateZoomPan(const MapView& map, bool hovered){
	ImGuiIO& io = ImGui::GetIO();
	const ImVec2 mouse = ImGui::GetMousePos();
	if ( hovered && dragNode_ < 0 && io.MouseWheel != 0.0f ) {
		float worldAtX = map.ToWorldX(mouse.x);
		float worldAtZ = map.ToWorldZ(mouse.y);
		zoom_ = std::clamp(zoom_ * std::exp(io.MouseWheel * 0.15f), 0.5f, 12.0f);
		float newScale = map.fitScale * zoom_;
		panX_ = mouse.x - ( map.canvasMin.x + map.canvasSize.x * 0.5f ) - ( worldAtX - map.worldCx ) * newScale;
		panY_ = mouse.y - ( map.canvasMin.y + map.canvasSize.y * 0.5f ) + ( worldAtZ - map.worldCz ) * newScale;
	}
	if ( hovered && ImGui::IsMouseDown(ImGuiMouseButton_Middle) ) {
		panX_ += io.MouseDelta.x;
		panY_ += io.MouseDelta.y;
	}
}
