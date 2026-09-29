#include "GuideHandleTool.h"
#include "game/editor/SceneEditContext.h"
#include "game/editor/RailEditOverlay.h"
#include "engine/rail/SplineRail.h"
#include "engine/utils/Level/RailEditor.h"
#include "engine/camera/Camera.h"
#include "engine/graphics/DebugDraw.h"
#include "externals/imgui/imgui.h"

#include <algorithm>
#include <cmath>

int GuideHandleTool::ActiveGuideOf(const SceneEditContext& context){
	const auto& rails = context.Rails();
	RailEditor* railEditor = context.railEditor;
	if ( !railEditor ) return -1;
	int currentRail = railEditor->GetCurrentRailIndex();
	if ( currentRail < 0 || currentRail >= ( int ) rails.size() || rails[currentRail].motionType != 3 ) return -1;
	int guide = rails[currentRail].guideRail;
	if ( guide < 0 || guide >= ( int ) rails.size() || railEditor->GetNodeCountOf(guide) < 2 ) return -1;
	return guide;
}

void GuideHandleTool::Update(const SceneEditContext& context){
	const int activeGuide = ActiveGuideOf(context);
	hover_ = false;

	// 非選択の見えないガイドは細線で場所だけ示す（選択中のは下でハンドル付きで描く）
	RailEditOverlay::DrawHiddenGuides(context.Rails(), activeGuide);

	if ( activeGuide < 0 ) {
		dragNode_ = -1;
		context.editor->SetGameViewGuideDragging(false);
		return;
	}

	// 選択中の足場のガイド：エディタの節点データを直接描いて、その場で編集できるようにする
	//   （ドラッグ中も遅延なく追従するよう、RailField のコピーではなくエディタ側の値を使う）
	RailEditor* railEditor = context.railEditor;
	const auto& view = context.View();
	ImGuiIO& io = ImGui::GetIO();
	const int nodeCount = railEditor->GetNodeCountOf(activeGuide);

	// 折れ線（太めのオレンジ）
	Vector3 prevNode {};
	for ( int i = 0; i < nodeCount; ++i ) {
		Vector3 nodePos;
		if ( !railEditor->GetNodePosOf(activeGuide, i, nodePos) ) continue;
		if ( i > 0 ) { DebugDraw::GetInstance()->Line(prevNode, nodePos, { 1.0f, 0.65f, 0.25f, 0.95f }); }
		prevNode = nodePos;
	}
	// ホバー中のノード / 線分（画面距離で判定）
	int hoverNode = -1; float bestNodePx = 14.0f;
	for ( int i = 0; i < nodeCount; ++i ) {
		Vector3 nodePos;
		if ( !railEditor->GetNodePosOf(activeGuide, i, nodePos) ) continue;
		float d = context.ScreenDistanceToMouse(nodePos);
		if ( d < bestNodePx ) { bestNodePx = d; hoverNode = i; }
	}
	int hoverSeg = -1; float segT = 0.0f;
	if ( hoverNode < 0 && view.hovered ) {
		float bestSegPx = 12.0f;
		for ( int i = 1; i < nodeCount; ++i ) {
			Vector3 a3, b3;
			if ( !railEditor->GetNodePosOf(activeGuide, i - 1, a3) ) continue;
			if ( !railEditor->GetNodePosOf(activeGuide, i, b3) ) continue;
			Vector2 a, b;
			if ( !context.ProjectToScreen(a3, a) || !context.ProjectToScreen(b3, b) ) continue;
			float vx = b.x - a.x, vy = b.y - a.y;
			float len2 = vx * vx + vy * vy;
			float t = ( len2 > 1e-5f )
				? std::clamp(( ( view.mousePos.x - a.x ) * vx + ( view.mousePos.y - a.y ) * vy ) / len2, 0.0f, 1.0f)
				: 0.0f;
			float dx = a.x + vx * t - view.mousePos.x, dy = a.y + vy * t - view.mousePos.y;
			float d = std::sqrt(dx * dx + dy * dy);
			if ( d < bestSegPx ) { bestSegPx = d; hoverSeg = i; segT = t; }
		}
	}
	hover_ = view.hovered && ( hoverNode >= 0 || hoverSeg >= 0 );
	// ハンドル圏内 or ドラッグ中はレール編集のクリック処理を止める（選択が奪われない）
	if ( hover_ || dragNode_ >= 0 ) {
		context.editor->SetExternalDragActive(true);
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	}

	const bool canClick = view.hovered && !view.gizmoActive;
	// つかむ（Ctrl無しの左クリック）
	if ( canClick && hoverNode >= 0 && ImGui::IsMouseClicked(0) && !io.KeyCtrl ) {
		dragRail_ = activeGuide;
		dragNode_ = hoverNode;
	}
	// Ctrl+クリック＝線分上に点を追加
	if ( canClick && io.KeyCtrl && hoverSeg >= 1 && dragNode_ < 0 && ImGui::IsMouseClicked(0) ) {
		Vector3 a3, b3;
		if ( railEditor->GetNodePosOf(activeGuide, hoverSeg - 1, a3)
			&& railEditor->GetNodePosOf(activeGuide, hoverSeg, b3) ) {
			Vector3 inserted { a3.x + ( b3.x - a3.x ) * segT,
			                   a3.y + ( b3.y - a3.y ) * segT,
			                   a3.z + ( b3.z - a3.z ) * segT };
			railEditor->InsertNodeOf(activeGuide, hoverSeg, inserted);
		}
	}
	// 右クリック＝点を削除（最低2点は残す）
	if ( canClick && hoverNode >= 0 && ImGui::IsMouseClicked(1) && nodeCount > 2 ) {
		railEditor->DeleteNodeOf(activeGuide, hoverNode);
		hoverNode = -1;
	}

	// ドラッグ中：カメラに平行な面でノードを移動（マウスの動きにそのまま付いてくる）
	bool draggingNode = false;
	if ( dragNode_ >= 0 ) {
		if ( dragRail_ != activeGuide || dragNode_ >= railEditor->GetNodeCountOf(activeGuide) ) {
			dragNode_ = -1; // 対象が変わった/消えた
		} else if ( ImGui::IsMouseDown(0) ) {
			draggingNode = true;
			DragNode(context, activeGuide);
		} else {
			dragNode_ = -1; // 離した：確定（Undoは自動で1回分にまとまる）
		}
	}
	context.editor->SetGameViewGuideDragging(draggingNode);

	// ハンドル描画（ホバー/ドラッグ中は白く大きく）
	for ( int i = 0; i < nodeCount; ++i ) {
		Vector3 nodePos;
		if ( !railEditor->GetNodePosOf(activeGuide, i, nodePos) ) continue;
		const bool active = ( i == hoverNode || i == dragNode_ );
		DebugDraw::GetInstance()->Sphere(nodePos, active ? 0.30f : 0.20f,
			active ? Vector4 { 1.0f, 1.0f, 1.0f, 1.0f } : Vector4 { 1.0f, 0.65f, 0.25f, 0.95f }, 8);
	}
}

void GuideHandleTool::DragNode(const SceneEditContext& context, int guide){
	ImGuiIO& io = ImGui::GetIO();
	RailEditor* railEditor = context.railEditor;
	Vector3 nodePos;
	if ( !railEditor->GetNodePosOf(guide, dragNode_, nodePos) || !context.camera ) return;
	if ( io.MouseDelta.x == 0.0f && io.MouseDelta.y == 0.0f ) return;

	// カメラの右/上ベクトル
	Vector3 camRotation = context.camera->GetRotation();
	float cosPitch = std::cos(camRotation.x), sinPitch = std::sin(camRotation.x);
	float sinYaw = std::sin(camRotation.y), cosYaw = std::cos(camRotation.y);
	Vector3 camForward { cosPitch * sinYaw, -sinPitch, cosPitch * cosYaw };
	Vector3 camRight { cosYaw, 0.0f, -sinYaw };
	Vector3 camUp {
		camForward.y * camRight.z - camForward.z * camRight.y,
		camForward.z * camRight.x - camForward.x * camRight.z,
		camForward.x * camRight.y - camForward.y * camRight.x };
	// 「1m動かすと画面で何px動くか」を実測してマウス移動量をメートルへ換算
	Vector2 sp0, spR, spU;
	if ( !context.ProjectToScreen(nodePos, sp0)
		|| !context.ProjectToScreen({ nodePos.x + camRight.x, nodePos.y + camRight.y, nodePos.z + camRight.z }, spR)
		|| !context.ProjectToScreen({ nodePos.x + camUp.x, nodePos.y + camUp.y, nodePos.z + camUp.z }, spU) ) {
		return;
	}
	float rx = spR.x - sp0.x, ry = spR.y - sp0.y;
	float ux = spU.x - sp0.x, uy = spU.y - sp0.y;
	// 1mが2px未満にしか映らない極端な状況では換算を頭打ちにする
	//（分母が小さすぎると1pxで何十mも飛ぶ暴走になるため）
	float lenR2 = ( std::max )( rx * rx + ry * ry, 4.0f );
	float lenU2 = ( std::max )( ux * ux + uy * uy, 4.0f );
	float meterR = ( io.MouseDelta.x * rx + io.MouseDelta.y * ry ) / lenR2;
	float meterU = ( io.MouseDelta.x * ux + io.MouseDelta.y * uy ) / lenU2;
	if ( io.KeyShift ) { meterR = 0.0f; } // Shift=縦（高さ）だけ
	if ( io.KeyCtrl )  { meterU = 0.0f; } // Ctrl=横だけ
	Vector3 newPos { nodePos.x + camRight.x * meterR + camUp.x * meterU,
	                 nodePos.y + camRight.y * meterR + camUp.y * meterU,
	                 nodePos.z + camRight.z * meterR + camUp.z * meterU };
	railEditor->SetNodePosOf(guide, dragNode_, newPos);
	ImGui::SetTooltip("X=%.1f Y=%.1f Z=%.1f%s", newPos.x, newPos.y, newPos.z,
		io.KeyShift ? "（縦だけ）" : ( io.KeyCtrl ? "（横だけ）" : "" ));
}
