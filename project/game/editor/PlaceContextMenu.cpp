#include "PlaceContextMenu.h"
#include "game/editor/SceneEditContext.h"
#include "game/enemy/Enemy.h"
#include "game/enemy/EnemyEditor.h"
#include "game/stage/BlockSystem.h"
#include "engine/utils/Level/LevelEditor.h"
#include "engine/utils/Level/RailEditor.h"
#include "externals/imgui/imgui.h"

#include <cmath>

namespace {
	const char* kPopupId = "GameViewPlaceMenu";
}

void PlaceContextMenu::Update(const SceneEditContext& context, bool anyDragActive){
	OpenIfRequested(context, anyDragActive);
	if ( !ImGui::BeginPopup(kPopupId) ) return;
	DrawPlaceItems(context);
	DrawExistingItems(context);
	// 表示系のトグル（どちらのUIモードでもパネルを開かず切り替えられる）
	ImGui::Separator();
	bool motionPreview = context.editor->GetEditorRailMotionPreview();
	if ( ImGui::MenuItem("動くレールをプレビュー再生", nullptr, motionPreview) ) {
		context.editor->SetEditorRailMotionPreview(!motionPreview);
	}
	ImGui::EndPopup();
}

bool PlaceContextMenu::ConsumeTestPlayRequest(int& outRail, float& outDist){
	if ( !testPlayRequested_ ) return false;
	testPlayRequested_ = false;
	outRail = testPlayRail_;
	outDist = testPlayDist_;
	return true;
}

// 動かさずに右クリックを離したら、配置先のレール位置と近くの既存配置物を調べてメニューを開く
void PlaceContextMenu::OpenIfRequested(const SceneEditContext& context, bool anyDragActive){
	const auto& view = context.View();
	ImGuiIO& io = ImGui::GetIO();
	const float dragX = io.MousePos.x - io.MouseClickedPos[1].x;
	const float dragY = io.MousePos.y - io.MouseClickedPos[1].y;
	const bool rightClickedStill = ImGui::IsMouseReleased(ImGuiMouseButton_Right)
		&& std::abs(dragX) < 4.0f && std::abs(dragY) < 4.0f;
	if ( !view.hovered || view.gizmoActive || !rightClickedStill || anyDragActive ) return;

	// マウスに一番近いレール上の点（0.5m刻みサンプル・60px以内）を配置先にする
	SceneEditContext::RailPick pick;
	const bool hasRail = context.PickRailPoint(0.0f, 60.0f, -1, 0.0f, pick);

	// ついで：クリック地点の近くにある既存の敵/ブロック/コインも調べる（操作メニュー用）
	const float kNearPx = 26.0f;
	enemyIndex_ = context.FindEnemyNearMouse(kNearPx);
	blockIndex_ = coinIndex_ = -1;
	{
		float best = kNearPx;
		const auto& blocks = context.editor->GetEditorBlocks();
		for ( int i = 0; i < ( int ) blocks.size(); ++i ) {
			// 横ずれは無視した近似中心（メニュー用の当たりなので十分）
			Vector3 center;
			if ( !context.RailPoint(blocks[i].rail, blocks[i].dist, ( float ) blocks[i].level * BlockSystem::kSize + 0.5f, center) ) continue;
			float d = context.ScreenDistanceToMouse(center);
			if ( d < best ) { best = d; blockIndex_ = i; }
		}
	}
	{
		float best = kNearPx;
		const auto& coins = context.editor->GetEditorCoins();
		for ( int i = 0; i < ( int ) coins.size(); ++i ) {
			Vector3 pos;
			if ( !context.RailPoint(coins[i].rail, coins[i].dist, coins[i].height, pos) ) continue;
			float d = context.ScreenDistanceToMouse(pos);
			if ( d < best ) { best = d; coinIndex_ = i; }
		}
	}
	if ( hasRail || enemyIndex_ >= 0 || blockIndex_ >= 0 || coinIndex_ >= 0 ) {
		placeRail_ = hasRail ? pick.rail : -1;
		placeDist_ = hasRail ? pick.dist : 0.0f;
		ImGui::OpenPopup(kPopupId);
	}
}

// 新規配置（レールの近くを右クリックした時）
void PlaceContextMenu::DrawPlaceItems(const SceneEditContext& context){
	if ( placeRail_ < 0 || placeRail_ >= ( int ) context.Rails().size() ) return;
	ImGui::TextDisabled("路線%d の %.1fm 地点", placeRail_, placeDist_);
	ImGui::Separator();
	if ( ImGui::MenuItem("ここからテストプレイ") ) {
		// この地点を1回だけスタート地点にして即Play（落下リスポーンも同地点）
		testPlayRequested_ = true;
		testPlayRail_ = placeRail_;
		testPlayDist_ = placeDist_;
		context.editor->RequestPlay();
	}
	if ( context.enemyEditor && ImGui::BeginMenu("敵を配置") ) {
		// 種類を選んで置く。置いた敵は選択状態になる＝敵エディタですぐ動きを決められる
		const EnemyType placeTypes[3] = { EnemyType::Zako, EnemyType::Strong, EnemyType::Air };
		for ( EnemyType placeType : placeTypes ) {
			if ( ImGui::MenuItem(EnemyEditor::GetTypeName(placeType)) ) {
				EnemySpawnData placed;
				placed.type      = placeType;
				placed.railIndex = placeRail_;
				placed.distance  = placeDist_;
				context.enemyEditor->AddEntry(placed); // 選択＋リスポーン＆保存データへ反映
			}
		}
		ImGui::EndMenu();
	}
	if ( ImGui::MenuItem("コインを配置") && context.levelEditor ) {
		context.railEditor->AddCoinAt(placeRail_, placeDist_);
		context.ResyncCoins();
		context.levelEditor->MarkDirty(); // コイン追加も[未保存]・自動保存の対象にする
	}
	if ( ImGui::MenuItem("ブロックを配置（選択中の種類）") ) {
		context.editor->AddEditorBlock(placeRail_, std::round(placeDist_), 0, 0.0f,
		                               context.editor->GetEditorBlockPaintType());
	}
}

// 近くにあった既存配置物の操作
void PlaceContextMenu::DrawExistingItems(const SceneEditContext& context){
	EnemyEditor* enemyEditor = context.enemyEditor;
	if ( enemyIndex_ >= 0 && enemyEditor && enemyIndex_ < ( int ) enemyEditor->GetSpawnDatas().size() ) {
		ImGui::Separator();
		ImGui::TextDisabled("#%02d %s", enemyIndex_,
			EnemyEditor::DisplayName(enemyEditor->GetSpawnDatas()[enemyIndex_]).c_str());
		if ( ImGui::MenuItem("この敵の設定を開く") ) {
			enemyEditor->SetSelectedEntry(enemyIndex_);
			context.editor->SetPanelVisible(EditorManager::Panel_Enemy, true);
		}
		if ( ImGui::MenuItem("この敵を削除") ) {
			enemyEditor->RemoveEntry(enemyIndex_); // 選択中の番号の付け替えも行う
			enemyIndex_ = -1;
		}
		if ( enemyIndex_ >= 0 && ImGui::MenuItem("敵の種類を切替（ドングリン→カミバナ→フワリン）") ) {
			auto& spawnData = enemyEditor->MutableSpawnDatas()[enemyIndex_];
			spawnData.type = ( EnemyType ) ( ( ( int ) spawnData.type + 1 ) % 3 ); // 3種を順番に切替
			enemyEditor->MarkChanged();
		}
	}
	if ( blockIndex_ >= 0 && blockIndex_ < ( int ) context.editor->GetEditorBlocks().size() ) {
		ImGui::Separator();
		if ( ImGui::MenuItem("このブロックを削除") ) {
			const BlockData block = context.editor->GetEditorBlocks()[blockIndex_];
			context.editor->RemoveEditorBlock(block.rail, block.dist, block.level, block.side);
			blockIndex_ = -1;
		}
	}
	if ( coinIndex_ >= 0 && coinIndex_ < ( int ) context.editor->GetEditorCoins().size() ) {
		ImGui::Separator();
		if ( ImGui::MenuItem("このコインを削除") ) {
			if ( context.levelEditor ) {
				context.railEditor->RemoveCoinAt(coinIndex_);
				context.ResyncCoins();
				context.levelEditor->MarkDirty(); // コイン削除も[未保存]・自動保存の対象にする
			}
			coinIndex_ = -1;
		}
	}
}
