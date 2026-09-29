#include "BlockPaintTool.h"
#include "game/editor/SceneEditContext.h"
#include "game/stage/BlockSystem.h"
#include "engine/rail/SplineRail.h"
#include "engine/utils/Level/RailEditor.h"
#include "engine/graphics/DebugDraw.h"
#include "engine/math/Matrix4x4.h"
#include "engine/math/VectorMath.h"
#include "externals/imgui/imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace MatrixMath;
using namespace VectorMath;

namespace {
	bool SameCell(const BlockPaintTool::Cell& a, const BlockPaintTool::Cell& b){
		return a.rail == b.rail && a.level == b.level
			&& std::abs(a.dist - b.dist) < 0.5f && std::abs(a.side - b.side) < 0.5f;
	}
}

// ---------------------------------------------------------------------
//  キー操作：B＝配置モードの切り替え / 数字 1〜8＝その段に固定 / 0＝段の固定を解除
// ---------------------------------------------------------------------
void BlockPaintTool::UpdateHotkeys(const SceneEditContext& context){
	if ( !context.railEditor ) return;
	RailEditor* railEditor = context.railEditor;
	ImGuiIO& io = ImGui::GetIO();
	// 右ドラッグ中（カメラ操作）・文字入力中・Ctrl併用（Ctrl+Z 等）は反応しない
	if ( !context.View().hovered || io.WantTextInput || io.KeyCtrl || ImGui::IsMouseDown(1) ) return;
	// 左ボタンを押している間（塗っている途中・範囲フィルの途中）は切り替えない
	if ( !ImGui::IsMouseDown(0) && ImGui::IsKeyPressed(ImGuiKey_B, false) ) {
		bool turnOn = !context.editor->IsEditorBlockPaintMode();
		railEditor->SetBlockPaintMode(turnOn);
		// 配置パネルを閉じているとモードが効かないので、入れる時は一緒に開く
		if ( turnOn ) { context.editor->SetPanelVisible(EditorManager::Panel_Items, true); }
	}
	if ( context.editor->IsEditorBlockPaintMode() ) {
		for ( int level = 0; level < 8; ++level ) {
			if ( ImGui::IsKeyPressed(( ImGuiKey ) ( ImGuiKey_1 + level ), false) ) {
				railEditor->SetBlockLevelLock(true, level);
			}
		}
		if ( ImGui::IsKeyPressed(ImGuiKey_0, false) ) { railEditor->SetBlockLevelLock(false, -1); }
	}
}

void BlockPaintTool::Cancel(){
	rectActive_ = false;
	lastPaint_.level = -1;
}

// ---------------------------------------------------------------------
//  毎フレーム：指している場所 → 何をするか → 目印・ゴースト → クリックの適用
// ---------------------------------------------------------------------
void BlockPaintTool::Update(const SceneEditContext& context){
	if ( !context.railEditor ) { Cancel(); return; }
	RailEditor* railEditor = context.railEditor;
	const auto& view = context.View();

	Frame frame;
	frame.paintType   = railEditor->GetBlockPaintType();
	frame.paintShape  = railEditor->GetBlockPaintShape();
	frame.eraseMode   = railEditor->IsBlockEraseMode() || ImGui::GetIO().KeyShift;
	frame.centerOnly  = railEditor->IsBlockCenterOnly();
	frame.levelLocked = railEditor->IsBlockLevelLocked();
	frame.lockedLevel = railEditor->GetBlockLockedLevel();
	frame.faceMode    = ( railEditor->GetBlockPickMode() == 0 );
	frame.mouseUsable = view.hovered && !view.gizmoActive;
	frame.strokeActive = ImGui::IsMouseDown(0) && lastPaint_.level >= 0;

	FindPointedCells(context, frame);
	DecideAction(context, frame);
	DrawGuides(context, frame);
	DrawGhost(context, frame);

	// 範囲フィル：ドラッグ中は終点を更新して、塗られる矩形を薄枠で予告する
	if ( frame.paintShape == 3 && rectActive_ && frame.hasPlace && frame.placeCell.rail == rectStart_.rail ) {
		rectEndDist_  = frame.placeCell.dist;
		rectEndLevel_ = frame.placeCell.level;
	}
	if ( frame.paintShape == 3 && rectActive_ ) { DrawRectFillPreview(context); }

	if ( frame.mouseUsable ) { ApplyInput(context, frame); }

	// 範囲フィルの確定：ボタンを離した瞬間に矩形をまとめて塗る/消す
	//   （マウスがレールから外れていても、最後に指していたセルまでを適用）
	if ( rectActive_ && !ImGui::IsMouseDown(0) ) { CommitRectFill(context); }

	// ボタンを離した時と、マウスが Game View から出た時はペイントの続きをリセット
	// （画面外で凍結した位置から遠くへ復帰した瞬間に大量補間しないように）
	if ( !ImGui::IsMouseDown(0) || !view.hovered ) { lastPaint_.level = -1; }

	// 配置モード中はレール編集のクリックを止める（ノード選択やスタンプと競合しない）
	if ( view.hovered ) { context.editor->SetExternalDragActive(true); }
}

// ---------------------------------------------------------------------
//  画面上の近さでセルを選ぶ。
//   まずマウスに一番近いレール位置を探し（0.25m刻み → 1mセルへ吸着）、
//   次にその断面の中から、マウスに一番近い「段 × 横ずれ」を選ぶ。
//   段や横ずれを1つに絞って呼ぶと、その高さ・その列だけを狙える
// ---------------------------------------------------------------------
bool BlockPaintTool::PickByScreen(const SceneEditContext& context, const PickOptions& options, Cell& outCell) const{
	const auto& rails = context.Rails();
	const Vector2 mouse = context.View().mousePos;
	int bestRail = -1; float bestDist = 0.0f; float bestScore = 220.0f;
	for ( int railIndex = 0; railIndex < ( int ) rails.size(); ++railIndex ) {
		if ( options.onlyRail >= 0 && railIndex != options.onlyRail ) continue;
		const SplineRail& rail = rails[railIndex];
		if ( rail.nodes.size() < 2 ) continue;
		if ( !rail.visible ) continue; // 見えない骨組み（リフトのガイド等）には置かない
		float length = rail.GetLength();
		int steps = std::clamp(( int ) ( length / 0.25f ), 2, 800);
		for ( int s = 0; s <= steps; ++s ) {
			float dist = length * ( float ) s / ( float ) steps;
			Vector3 railPoint = rail.GetPositionByDistance(dist);
			// 断面の縦線（選べる段の一番下〜一番上）とマウスの画面距離
			Vector2 screenLow, screenHigh;
			if ( !context.ProjectToScreen({ railPoint.x, railPoint.y + ( float ) options.levelMin + 0.5f, railPoint.z }, screenLow) ) continue;
			if ( !context.ProjectToScreen({ railPoint.x, railPoint.y + ( float ) options.levelMax + 0.5f, railPoint.z }, screenHigh) ) continue;
			float segX = screenHigh.x - screenLow.x, segY = screenHigh.y - screenLow.y;
			float segLengthSq = segX * segX + segY * segY;
			float t = ( segLengthSq > 1e-6f )
				? std::clamp(( ( mouse.x - screenLow.x ) * segX + ( mouse.y - screenLow.y ) * segY ) / segLengthSq, 0.0f, 1.0f)
				: 0.0f;
			float nearX = screenLow.x + segX * t, nearY = screenLow.y + segY * t;
			float score = std::sqrt(( nearX - mouse.x ) * ( nearX - mouse.x ) + ( nearY - mouse.y ) * ( nearY - mouse.y ));
			if ( options.levelMax > options.levelMin ) {
				// 縦線だけで選ぶと、画面上で柱が重なる「1本奥の並走レール」に吸着してしまう。
				// 足元（レール線）の近さも加点して、指している道のレールが勝つようにする
				float baseX = screenLow.x - mouse.x, baseY = screenLow.y - mouse.y;
				score += ( std::min )( std::sqrt(baseX * baseX + baseY * baseY), 160.0f ) * 0.5f;
			}
			if ( railIndex == options.stickRail ) { score -= 25.0f; }
			if ( score < bestScore ) { bestScore = score; bestRail = railIndex; bestDist = dist; }
		}
	}
	if ( bestRail < 0 ) return false;

	// 1mグリッドの「整数セル」へ吸着する。レール長で丸めずクランプすると
	// 端だけ半端な距離のセルが生まれ、隣と重なった二重ブロックが置けてしまう
	float railLength = rails[bestRail].GetLength();
	float cellDist = std::round(bestDist);
	if ( cellDist < 0.0f )       { cellDist = 0.0f; }
	if ( cellDist > railLength ) { cellDist = std::floor(railLength); }

	int   bestLevel = options.levelMin;
	float bestSide  = options.sideFree ? 0.0f : options.fixedSide;
	float bestCellPx = 1e9f;
	for ( int level = options.levelMin; level <= options.levelMax; ++level ) {
		for ( int sideStep = -2; sideStep <= 2; ++sideStep ) {
			if ( !options.sideFree && sideStep != 0 ) continue; // 横ずれ固定の時は1列だけ
			float side = options.sideFree ? ( float ) sideStep : options.fixedSide;
			Vector3 center;
			if ( !context.blockSystem->CellCenter(bestRail, cellDist, level, side, center) ) continue;
			Vector2 screenPos;
			if ( !context.ProjectToScreen(center, screenPos) ) continue;
			float dx = screenPos.x - mouse.x, dy = screenPos.y - mouse.y;
			float cellPx = std::sqrt(dx * dx + dy * dy) + std::abs(side) * 14.0f; // 横はやや選ばれにくく
			if ( options.preferExisting && context.editor->HasEditorBlock(bestRail, cellDist, level, side) ) {
				cellPx -= 12.0f;
			}
			if ( cellPx < bestCellPx ) { bestCellPx = cellPx; bestLevel = level; bestSide = side; }
		}
	}
	outCell = { bestRail, cellDist, bestLevel, bestSide };
	return true;
}

// ---------------------------------------------------------------------
//  マウスが指している場所を決める
//   placeCell = 置く先のセル / pointedCell = 指している既存ブロック（消す・塗り替える対象）
// ---------------------------------------------------------------------
void BlockPaintTool::FindPointedCells(const SceneEditContext& context, Frame& frame) const{
	if ( !frame.mouseUsable ) return;
	const auto& view = context.View();
	const auto& rails = context.Rails();

	if ( rectActive_ ) {
		// 範囲フィルのドラッグ中：始めたレール・横位置のまま、終点（距離×段）を選ぶ
		PickOptions options;
		options.levelMin  = frame.levelLocked ? frame.lockedLevel : 0;
		options.levelMax  = frame.levelLocked ? frame.lockedLevel : kMaxLevel;
		options.fixedSide = rectStart_.side;
		options.onlyRail  = rectStart_.rail;
		frame.hasPlace = PickByScreen(context, options, frame.placeCell);
		return;
	}
	if ( frame.strokeActive ) {
		// 塗っている途中：塗り始めたレール・段・横位置のまま、線を引くように進める。
		//   （置いたばかりのブロックの面を拾って、手前や上へ勝手に積み上がらないように）
		PickOptions options;
		options.levelMin  = lastPaint_.level;
		options.levelMax  = lastPaint_.level;
		options.fixedSide = lastPaint_.side;
		options.onlyRail  = lastPaint_.rail;
		frame.hasPlace = PickByScreen(context, options, frame.placeCell);
		return;
	}

	// マウスのレイが当たるブロック（指している既存ブロック）
	BlockSystem::RayHit rayHit;
	bool rayFound = false;
	if ( view.imgSize.x > 1.0f && view.imgSize.y > 1.0f ) {
		Matrix4x4 invViewProj = Inverse(view.viewProj);
		float ndcX = ( view.mousePos.x - view.imgMin.x ) / view.imgSize.x * 2.0f - 1.0f;
		float ndcY = 1.0f - ( view.mousePos.y - view.imgMin.y ) / view.imgSize.y * 2.0f;
		Vector3 nearPoint = NdcToWorld(ndcX, ndcY, 0.0f, invViewProj);
		Vector3 farPoint  = NdcToWorld(ndcX, ndcY, 1.0f, invViewProj);
		Vector3 rayDir = { farPoint.x - nearPoint.x, farPoint.y - nearPoint.y, farPoint.z - nearPoint.z };
		float rayLength = Length(rayDir);
		if ( rayLength > 1e-5f ) {
			rayDir = { rayDir.x / rayLength, rayDir.y / rayLength, rayDir.z / rayLength };
			rayFound = context.blockSystem->Raycast(nearPoint, rayDir, rayHit);
		}
	}
	if ( rayFound ) {
		frame.pointedCell = { rayHit.cell.rail, rayHit.cell.dist, rayHit.cell.level, rayHit.cell.side };
		frame.hasPointed = true;
	}

	if ( frame.faceMode && !frame.levelLocked && rayFound ) {
		// 面に積む：指した面の向こう隣のセルへ置く（上の面＝上に積む / 横の面＝隣に並べる）
		Cell adjacent = frame.pointedCell;
		bool valid = true;
		if ( rayHit.faceAxis == 1 ) {
			adjacent.level += rayHit.faceSign;
		} else if ( rayHit.faceAxis == 2 ) {
			// 進行方向の隣：2種類の占有幅を足したぶん離す（横長・台座にもぴったり並ぶ）
			float gap = RailEditor::BlockOccupyHalfOf(rayHit.cell.type) + RailEditor::BlockOccupyHalfOf(frame.paintType);
			adjacent.dist += ( float ) rayHit.faceSign * gap;
			float railLength = rails[adjacent.rail].GetLength();
			if ( adjacent.dist < 0.0f || adjacent.dist > railLength ) { valid = false; }
		} else {
			adjacent.side += ( float ) rayHit.faceSign;
			// 道の中心だけに置く設定の時は、横の面を指しても上に積む
			if ( ( frame.centerOnly && std::abs(adjacent.side) > 0.01f ) || std::abs(adjacent.side) > 2.01f ) {
				adjacent = frame.pointedCell;
				adjacent.level += 1;
			}
		}
		if ( adjacent.level < 0 || adjacent.level > kMaxLevel ) { valid = false; }
		if ( valid ) { frame.placeCell = adjacent; frame.hasPlace = true; }
	} else if ( frame.faceMode ) {
		// 面に積む（ブロックを指していない）：道の上＝1段目へ。段を固定中はその段へ
		PickOptions options;
		options.levelMin = frame.levelLocked ? frame.lockedLevel : 0;
		options.levelMax = options.levelMin;
		options.sideFree = !frame.centerOnly;
		frame.hasPlace = PickByScreen(context, options, frame.placeCell);
	} else {
		// 断面から選ぶ（従来の方式）：マウスの高さで段が決まる
		PickOptions options;
		options.levelMin = frame.levelLocked ? frame.lockedLevel : 0;
		options.levelMax = frame.levelLocked ? frame.lockedLevel : kMaxLevel;
		options.sideFree = !frame.centerOnly;
		options.preferExisting = frame.eraseMode || context.railEditor->IsBlockClickErase();
		frame.hasPlace = PickByScreen(context, options, frame.placeCell);
		// この方式では「選んだセルにあるブロック」が消す対象
		const Cell& cell = frame.placeCell;
		frame.hasPointed = frame.hasPlace && context.editor->HasEditorBlock(cell.rail, cell.dist, cell.level, cell.side);
		if ( frame.hasPointed ) { frame.pointedCell = frame.placeCell; }
	}
}

// 何をするかを決める（ゴーストの色・クリックの動作が同じ判断を使う）
void BlockPaintTool::DecideAction(const SceneEditContext& context, Frame& frame) const{
	if ( frame.strokeActive || rectActive_ ) return;
	RailEditor* railEditor = context.railEditor;
	const Cell& place = frame.placeCell;
	const bool placeOccupied = frame.hasPlace
		&& context.editor->HasEditorBlock(place.rail, place.dist, place.level, place.side);
	if ( frame.eraseMode ) {
		if ( frame.hasPointed ) { frame.action = Action::Erase; frame.target = frame.pointedCell; }
	} else if ( ImGui::GetIO().KeyAlt && frame.hasPointed ) {
		// Alt＝指しているブロックを、選んでいる種類へ塗り替える
		frame.action = Action::Replace; frame.target = frame.pointedCell;
	} else if ( frame.hasPlace && !placeOccupied ) {
		frame.action = Action::Place; frame.target = place;
	} else if ( frame.hasPlace && placeOccupied ) {
		frame.target = place;
		frame.action = railEditor->IsBlockClickErase() ? Action::Erase : Action::Replace;
	}
	// 塗り替え先が同じ種類なら何もしない
	if ( frame.action == Action::Replace ) {
		const Cell& target = frame.target;
		int found = railEditor->FindBlock(target.rail, target.dist, target.level, target.side);
		if ( found < 0 || railEditor->GetBlocks()[found].type == frame.paintType ) { frame.action = Action::None; }
	}
}

// 目印：対象レールの強調（黄色でなぞる）と、1mセルの区切り線
void BlockPaintTool::DrawGuides(const SceneEditContext& context, const Frame& frame) const{
	const Cell* guideCell = nullptr;
	if ( frame.action != Action::None ) { guideCell = &frame.target; }
	else if ( frame.hasPlace )          { guideCell = &frame.placeCell; }
	const auto& rails = context.Rails();
	if ( !guideCell || guideCell->rail < 0 || guideCell->rail >= ( int ) rails.size() ) return;

	const SplineRail& rail = rails[guideCell->rail];
	float railLength = rail.GetLength();
	// どのレールに置かれるかが一目で分かるように、対象レールを黄色でなぞる
	int highlightSteps = std::clamp(( int ) railLength, 1, 200);
	Vector3 prevPoint = rail.GetPositionByDistance(0.0f);
	for ( int s = 1; s <= highlightSteps; ++s ) {
		Vector3 curPoint = rail.GetPositionByDistance(railLength * ( float ) s / ( float ) highlightSteps);
		DebugDraw::GetInstance()->Line({ prevPoint.x, prevPoint.y + 0.05f, prevPoint.z },
		                               { curPoint.x, curPoint.y + 0.05f, curPoint.z },
		                               { 1.0f, 0.9f, 0.2f, 1.0f });
		prevPoint = curPoint;
	}
	// 1mセルの区切り線を道の上へ描く（どこに吸着するかの見える化）。指している地点の前後±12セル
	float tickStart = ( std::max )( 0.5f, guideCell->dist - 12.5f );
	float tickEnd   = ( std::min )( railLength, guideCell->dist + 12.5f );
	for ( float boundary = std::floor(tickStart - 0.5f) + 0.5f; boundary <= tickEnd; boundary += 1.0f ) {
		if ( boundary < 0.0f ) continue;
		Vector3 tickBase = rail.GetPositionByDistance(boundary);
		Vector3 tickTan  = rail.GetTangentByDistance(boundary);
		float tickHoriz = std::sqrt(tickTan.x * tickTan.x + tickTan.z * tickTan.z);
		if ( tickHoriz < 1e-4f ) continue;
		Vector3 tickRight { tickTan.z / tickHoriz, 0.0f, -tickTan.x / tickHoriz };
		// 選択中セルの両端(±0.5m)は明るく、それ以外はうっすら
		bool nearSelected = std::abs(boundary - guideCell->dist) < 0.51f;
		Vector4 tickColor = nearSelected ? Vector4 { 1.0f, 0.95f, 0.4f, 0.9f }
		                                 : Vector4 { 1.0f, 1.0f, 1.0f, 0.30f };
		DebugDraw::GetInstance()->Line(
			{ tickBase.x - tickRight.x * 0.9f, tickBase.y + 0.06f, tickBase.z - tickRight.z * 0.9f },
			{ tickBase.x + tickRight.x * 0.9f, tickBase.y + 0.06f, tickBase.z + tickRight.z * 0.9f },
			tickColor);
	}
}

// ゴースト（緑=置く / 水色=道の脇の飾り / 赤=消す / 黄=塗り替え）とマウス横の説明。
//   実際のブロックと同じ位置・向き・大きさで出る
void BlockPaintTool::DrawGhost(const SceneEditContext& context, Frame& frame) const{
	if ( frame.action == Action::None ) return;
	BlockSystem* blockSystem = context.blockSystem;
	RailEditor* railEditor = context.railEditor;
	Cell& target = frame.target;

	int ghostType = frame.paintType;
	Vector4 ghostColor = { 0.3f, 1.0f, 0.5f, 1.0f };
	float ghostInflate = 0.0f;
	const char* actionText = "置く";
	if ( frame.action == Action::Place && std::abs(target.side) > 0.01f ) {
		ghostColor = { 0.35f, 0.8f, 1.0f, 1.0f };
		actionText = "置く（道の脇＝当たらない飾り）";
	}
	if ( frame.action == Action::Erase || frame.action == Action::Replace ) {
		// 消す/塗り替える対象は、今あるブロックの形をひと回り大きく囲む
		int found = railEditor->FindBlock(target.rail, target.dist, target.level, target.side);
		if ( found >= 0 ) {
			const BlockData& existing = railEditor->GetBlocks()[found];
			target.dist = existing.dist; target.side = existing.side;
			ghostType = existing.type;
		}
		ghostInflate = 0.04f;
		if ( frame.action == Action::Erase ) { ghostColor = { 1.0f, 0.35f, 0.3f, 1.0f }; actionText = "消す"; }
		else                                 { ghostColor = { 1.0f, 0.9f, 0.25f, 1.0f }; actionText = "塗り替える"; }
	}
	blockSystem->DrawCellGhost(target.rail, target.dist, target.level, target.side, ghostType, ghostColor, ghostInflate);

	// 高さの目安：ゴーストから道の上（1段目）まで縦線を引く
	Vector3 ghostCenter, groundCenter;
	if ( target.level > 0
		&& blockSystem->CellCenter(target.rail, target.dist, target.level, target.side, ghostCenter)
		&& blockSystem->CellCenter(target.rail, target.dist, 0, target.side, groundCenter) ) {
		DebugDraw::GetInstance()->Line(ghostCenter, groundCenter, { ghostColor.x, ghostColor.y, ghostColor.z, 0.6f });
	}
	// 柱：下まで埋まる範囲を薄い枠で予告する
	if ( frame.paintShape == 1 && frame.action == Action::Place ) {
		Vector4 dim = { ghostColor.x, ghostColor.y, ghostColor.z, 0.35f };
		for ( int level = 0; level < target.level; ++level ) {
			blockSystem->DrawCellGhost(target.rail, target.dist, level, target.side, frame.paintType, dim, -0.05f);
		}
	}

	// マウスの横に「何が起きるか」と段を出す（クリックする前に分かる）
	char hint[96];
	snprintf(hint, sizeof(hint), "%s  %d段目%s", actionText, target.level + 1, frame.levelLocked ? "（固定）" : "");
	const Vector2 mouse = context.View().mousePos;
	ImDrawList* hintDraw = ImGui::GetForegroundDrawList();
	ImVec2 hintPos = { mouse.x + 18.0f, mouse.y + 14.0f };
	ImVec2 hintSize = ImGui::CalcTextSize(hint);
	hintDraw->AddRectFilled({ hintPos.x - 4.0f, hintPos.y - 2.0f },
		{ hintPos.x + hintSize.x + 4.0f, hintPos.y + hintSize.y + 2.0f }, IM_COL32(0, 0, 0, 170), 3.0f);
	hintDraw->AddText(hintPos, IM_COL32(255, 255, 255, 255), hint);
}

void BlockPaintTool::DrawRectFillPreview(const SceneEditContext& context) const{
	float distFrom = ( std::min )( rectStart_.dist, rectEndDist_ );
	float distTo   = ( std::min )( ( std::max )( rectStart_.dist, rectEndDist_ ), distFrom + 32.0f );
	int   levelFrom = ( std::min )( rectStart_.level, rectEndLevel_ );
	int   levelTo   = ( std::max )( rectStart_.level, rectEndLevel_ );
	Vector4 dim = rectErase_ ? Vector4 { 1.0f, 0.4f, 0.35f, 0.5f }
	                         : Vector4 { 0.35f, 1.0f, 0.55f, 0.5f };
	for ( float dist = distFrom; dist <= distTo + 0.5f; dist += 1.0f ) {
		for ( int level = levelFrom; level <= levelTo; ++level ) {
			context.blockSystem->DrawCellGhost(rectStart_.rail, dist, level, rectStart_.side, 0, dim, -0.04f);
		}
	}
}

// クリック・ドラッグの適用。押した瞬間に「置く/消す」を決めて、押しっぱなしで連続適用（ペイント）
void BlockPaintTool::ApplyInput(const SceneEditContext& context, const Frame& frame){
	ImGuiIO& io = ImGui::GetIO();
	// 右クリック：動かさずに離した時だけ、指しているブロックを消す。
	//   押した瞬間に消すと、右ドラッグで視点を回し始めただけでブロックが消えてしまう
	const float rightDragX = io.MousePos.x - io.MouseClickedPos[1].x;
	const float rightDragY = io.MousePos.y - io.MouseClickedPos[1].y;
	const bool rightClickedStill = ImGui::IsMouseReleased(ImGuiMouseButton_Right)
		&& std::abs(rightDragX) < 4.0f && std::abs(rightDragY) < 4.0f;

	if ( rightClickedStill && !frame.strokeActive && !rectActive_ ) {
		const Cell* erase = frame.hasPointed ? &frame.pointedCell : ( frame.hasPlace ? &frame.placeCell : nullptr );
		if ( erase ) { context.editor->RemoveEditorBlock(erase->rail, erase->dist, erase->level, erase->side); }
		return;
	}
	if ( ImGui::IsMouseClicked(0) && frame.action != Action::None ) {
		const Cell& target = frame.target;
		if ( frame.action == Action::Replace ) {
			context.railEditor->ReplaceBlockType(target.rail, target.dist, target.level, target.side, frame.paintType);
		} else if ( frame.paintShape == 3 ) {
			// 範囲フィル：始点を記録（適用はボタンを離した時にまとめて）
			rectActive_   = true;
			rectErase_    = ( frame.action == Action::Erase );
			rectStart_    = target;
			rectEndDist_  = target.dist;
			rectEndLevel_ = target.level;
		} else {
			erasing_ = ( frame.action == Action::Erase );
			ApplyPaint(context, target);
		}
		return;
	}
	// 塗っている途中：クリックの手ぶれ（数px）では連続配置しない
	if ( !frame.strokeActive || !frame.hasPlace || !ImGui::IsMouseDragging(0, 6.0f) ) return;
	if ( SameCell(lastPaint_, frame.placeCell) ) return;
	if ( frame.paintShape == 2 && !erasing_ ) {
		// 階段：1マス進むごとに1段ずつ高くする（ドラッグするだけで階段が生える）
		Cell stairCell = frame.placeCell;
		stairCell.level = std::clamp(lastPaint_.level + 1, 0, kMaxLevel);
		ApplyPaint(context, stairCell);
		return;
	}
	// 速くドラッグするとフレーム間でセルが飛ぶ。間のセルも埋めて、
	// 線を引くように途切れず塗れるようにする。
	// ただし1フレームの補間は8マスまで（画面外→遠くへ復帰した時などに
	// レール全長ぶん一気に塗ってしまう事故を防ぐ）
	if ( std::abs(frame.placeCell.dist - lastPaint_.dist) <= 8.5f ) {
		float from = lastPaint_.dist, to = frame.placeCell.dist;
		float step = ( to >= from ) ? 1.0f : -1.0f;
		for ( float dist = from + step; std::abs(dist - to) > 0.5f; dist += step ) {
			Cell between = frame.placeCell;
			between.dist = dist;
			ApplyPaint(context, between);
		}
	}
	ApplyPaint(context, frame.placeCell);
}

void BlockPaintTool::ApplyPaint(const SceneEditContext& context, const Cell& cell){
	EditorManager* editor = context.editor;
	const int paintType  = context.railEditor->GetBlockPaintType();
	const int paintShape = context.railEditor->GetBlockPaintShape();
	if ( erasing_ ) {
		if ( paintShape == 1 ) {
			// 柱消し：そのセルの縦一列をまとめて消す
			for ( int level = 0; level <= kMaxLevel; ++level ) {
				editor->RemoveEditorBlock(cell.rail, cell.dist, level, cell.side);
			}
		} else {
			editor->RemoveEditorBlock(cell.rail, cell.dist, cell.level, cell.side);
		}
	} else {
		if ( paintShape == 1 ) {
			// 柱：クリックした段から地面まで縦に埋める（塔・壁の土台が1クリック）
			for ( int level = 0; level <= cell.level; ++level ) {
				editor->AddEditorBlock(cell.rail, cell.dist, level, cell.side, paintType);
			}
		} else {
			editor->AddEditorBlock(cell.rail, cell.dist, cell.level, cell.side, paintType);
		}
	}
	lastPaint_ = cell;
}

void BlockPaintTool::CommitRectFill(const SceneEditContext& context){
	const int paintType = context.railEditor->GetBlockPaintType();
	float distFrom = ( std::min )( rectStart_.dist, rectEndDist_ );
	float distTo   = ( std::min )( ( std::max )( rectStart_.dist, rectEndDist_ ), distFrom + 32.0f ); // 事故防止の上限
	int   levelFrom = ( std::min )( rectStart_.level, rectEndLevel_ );
	int   levelTo   = ( std::max )( rectStart_.level, rectEndLevel_ );
	for ( float dist = distFrom; dist <= distTo + 0.5f; dist += 1.0f ) {
		for ( int level = levelFrom; level <= levelTo; ++level ) {
			if ( rectErase_ ) { context.editor->RemoveEditorBlock(rectStart_.rail, dist, level, rectStart_.side); }
			else              { context.editor->AddEditorBlock(rectStart_.rail, dist, level, rectStart_.side, paintType); }
		}
	}
	rectActive_ = false;
}
