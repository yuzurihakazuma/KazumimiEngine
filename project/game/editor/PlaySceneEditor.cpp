#include "PlaySceneEditor.h"
#include "game/editor/RailEditOverlay.h"
#include "game/enemy/EnemyEditor.h"
#include "game/player/Player.h"
#include "game/rail/RailField.h"
#include "engine/rail/SplineRail.h"

void PlaySceneEditor::DrawPanels(const SceneEditContext& context){
	if ( context.enemyEditor && context.editor->IsPanelVisible(EditorManager::Panel_Enemy) ) {
		DrawEnemyEditorWindow(context);
	}
	if ( context.enemyEditor && context.levelEditor && context.editor->IsPanelVisible(EditorManager::Panel_Layout) ) {
		DrawStripPanel(context);
	}
}

// 敵配置用エディタのUIウィンドウ。
//   レールエディタで選択中のノード位置（レール番号＋距離）を「選択ノードに配置」用に渡す
void PlaySceneEditor::DrawEnemyEditorWindow(const SceneEditContext& context){
	int pickRail = -1; float pickDist = 0.0f; bool hasPick = false;
	Vector3 pickNodePos {};
	if ( context.editor->GetEditorSelectedNode(pickRail, pickNodePos) ) {
		const auto& rails = context.Rails();
		if ( pickRail >= 0 && pickRail < ( int ) rails.size() && rails[pickRail].nodes.size() >= 2 ) {
			pickDist = rails[pickRail].GetClosestDistance(pickNodePos);
			hasPick = true;
		}
	}
	context.enemyEditor->DrawWindow(context.Rails(), pickRail, pickDist, hasPick);
}

// 配置ビュー（レール展開図）：ブロック・敵・コインを ImGui のパネルの中だけで配置する。
//   レール1本をまっすぐ伸ばした「距離 × 段」のマス目で編集し、結果は既存のデータへ直接入る
//   （＝Game View・保存・元に戻す はそのまま連動する）
void PlaySceneEditor::DrawStripPanel(const SceneEditContext& context){
	const bool playing = ( context.editor->GetMode() == EngineMode::Play );
	RailStripPanel::Context strip;
	strip.rails       = context.rails;
	strip.levelEditor = context.levelEditor;
	strip.railEditor  = context.railEditor;
	strip.enemyEditor = context.enemyEditor;
	strip.blockSystem = context.blockSystem;
	strip.editable    = context.editing;
	if ( context.railField ) {
		strip.startRail = context.railField->GetStartRail();
		strip.startDist = context.railField->GetStartDistance();
		strip.goalRail  = context.railField->HasGoal() ? context.railField->GetGoalRail() : -1;
		strip.goalDist  = context.railField->GetGoalDistance();
	}
	strip.hasPlayer = playing && context.player;
	if ( context.player ) {
		strip.playerRail = context.player->GetCurrentRail();
		strip.playerPos  = context.player->GetPosition();
	}
	strip.onCoinsChanged = [context]() { context.ResyncCoins(); };
	stripPanel_.Draw(strip);
	if ( stripPanel_.ConsumeOpenEnemyPanelRequest() ) {
		context.editor->SetPanelVisible(EditorManager::Panel_Enemy, true);
	}
}

void PlaySceneEditor::UpdateGameView(const SceneEditContext& context){
	EditorManager* editor = context.editor;
	const bool editing = context.editing;
	const bool paintMode = editor->IsEditorBlockPaintMode();

	// --- 敵のピン・名札・直接ドラッグ（エディット中は常に有効）---
	//   レール編集のマウス操作を止めるかどうか（SetExternalDragActive）は、ここで毎フレーム決め直す
	if ( editing && context.enemyEditor ) {
		enemyTool_.Update(context);
	} else {
		enemyTool_.Cancel();
		editor->SetExternalDragActive(false);
	}

	// --- コイン・ブロックのつかみ移動（ブロック配置モードOFF時）。敵 > コイン > ブロック の順に優先 ---
	if ( editing && !paintMode ) {
		coinTool_.Update(context, enemyTool_.IsDragging() || blockDragTool_.IsDragging());
		blockDragTool_.Update(context, enemyTool_.IsDragging() || coinTool_.IsDragging() || coinTool_.IsHovering());
	} else {
		coinTool_.Cancel();
		blockDragTool_.Cancel();
	}

	// --- 右クリックメニュー（ブロック配置モード中は右クリック＝削除なので出さない）---
	if ( editing && !paintMode ) {
		const bool anyDragActive = enemyTool_.IsDragging() || coinTool_.IsDragging() || blockDragTool_.IsDragging()
			|| guideTool_.IsDragging() || guideTool_.IsHovering();
		contextMenu_.Update(context, anyDragActive);
	}

	// --- レールの動きの見える化＋ガイドハンドル（エディット中は常時）---
	if ( editing ) {
		RailEditOverlay::DrawMotionRanges(context.Rails());
		guideTool_.Update(context);
		RailEditOverlay::DrawAppearRoads(context.Rails());
	}

	// --- ミニマップ ---
	if ( editor->IsPanelVisible(EditorManager::Panel_Minimap) ) { minimap_.Draw(context); }

	// --- ブロック配置のキー操作と配置モード ---
	//   アイコンモードで配置パネルを閉じている間はペイントも無効（見えない状態で誤配置しない）
	if ( editing && context.railEditor ) { paintTool_.UpdateHotkeys(context); }
	if ( editing && context.railEditor && editor->IsEditorBlockPaintMode()
		&& editor->IsPanelVisible(EditorManager::Panel_Items) ) {
		paintTool_.Update(context);
	} else {
		paintTool_.Cancel();
	}

	// 敵の履歴の確定。敵エディタの窓が閉じていても毎フレーム行う
	//   （Game View や配置ビューでの変更も「1操作＝1手」として元に戻せるように）
	if ( context.enemyEditor ) { context.enemyEditor->TickHistory(); }
}
