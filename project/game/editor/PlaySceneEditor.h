#pragma once
// =====================================================================
//  PlaySceneEditor：ゲームプレイシーンのエディタ機能一式（ImGui 描画中に毎フレーム呼ぶ）。
//   ・パネル：敵エディタの窓 / 配置ビュー（レール展開図）/ ミニマップ
//   ・Game View の上の操作：敵・コイン・ブロックのつかみ移動、右クリックメニュー、
//     動くレールの見える化、ガイドハンドル、ブロック配置モード
//  各ツールの呼ぶ順番と「どれが優先か」（敵 > コイン > ブロック、ドラッグ中はメニューを開かない 等）
//  をここでまとめて決める。シーンは SceneEditContext を作って渡すだけ
// =====================================================================
#include "game/editor/SceneEditContext.h"
#include "game/editor/RailStripPanel.h"
#include "game/editor/EnemyGameViewTool.h"
#include "game/editor/CoinDragTool.h"
#include "game/editor/BlockDragTool.h"
#include "game/editor/PlaceContextMenu.h"
#include "game/editor/GuideHandleTool.h"
#include "game/editor/MinimapPanel.h"
#include "game/editor/BlockPaintTool.h"

class PlaySceneEditor {
public:
    // パネル（敵エディタの窓・配置ビュー）。表示中のパネルだけ描く
    void DrawPanels(const SceneEditContext& context);
    // Game View の上の操作とミニマップ。最後に敵の履歴を確定する
    void UpdateGameView(const SceneEditContext& context);

    // 配置ビューの「カメラをここへ」の要求（あれば true。height はレール面からの高さ）
    bool ConsumeStripFocusRequest(int& outRail, float& outDist, float& outHeight) {
        return stripPanel_.ConsumeFocusRequest(outRail, outDist, outHeight);
    }
    // 右クリックメニューの「ここからテストプレイ」の要求（あれば true）
    bool ConsumeTestPlayRequest(int& outRail, float& outDist) {
        return contextMenu_.ConsumeTestPlayRequest(outRail, outDist);
    }

private:
    void DrawEnemyEditorWindow(const SceneEditContext& context);
    void DrawStripPanel(const SceneEditContext& context);

    RailStripPanel    stripPanel_;
    EnemyGameViewTool enemyTool_;
    CoinDragTool      coinTool_;
    BlockDragTool     blockDragTool_;
    PlaceContextMenu  contextMenu_;
    GuideHandleTool   guideTool_;
    MinimapPanel      minimap_;
    BlockPaintTool    paintTool_;
};
