#pragma once
// =====================================================================
//  StripState：配置ビュー（レール展開図）の部品どうしで共有する状態
//   開いているレール・道具・表示の設定・シーンへの要求など、
//   操作部（StripControls）・キャンバス（StripCanvas）・各道具・レールの選択（StripRailSelector）の
//   どれか2つ以上が読み書きするものだけをここに置く。1つの部品だけが使う状態はその部品が持つ。
//   RailStripPanel が1つ持ち、各部品へ参照で渡す
// =====================================================================
#include "game/enemy/Enemy.h"

enum class StripTool { Block, Enemy, Coin };

struct StripState {
    // --- 開いているレール ---
    int   currentRail = 0;
    bool  followEditorRail = true;   // レールエディタの選択に合わせる
    int   lastSelectedEnemy = -1;    // 前のフレームの敵の選択（外で選択が変わったことの検出用）

    // --- 道具 ---
    StripTool tool = StripTool::Block;
    EnemyType enemyType = EnemyType::Zako;
    int   sideLayer = 0;             // 編集する横の位置（-2〜+2。0=道の中心）

    // --- 表示 ---
    bool  showOtherSides = true;     // 他の横位置のブロックも薄く表示
    float cellPx = 28.0f;            // 拡大率（1m のピクセル数）
    int   visibleLevels = 5;         // 表示する段数
    bool  scrollResetPending = true; // レールを切り替えた直後：表示位置を先頭へ戻す
    float scrollToDist = -1.0f;      // この距離を表示の中央へ持ってくる（-1=なし）
    bool  fitRequested = false;      // 「全体」ボタン：レール全体が入る拡大率にする
    float viewCenterDist = 0.0f;     // 見えている範囲の中央の距離（「カメラをここへ」ボタン用）

    // --- シーンへの要求 ---
    bool  focusPending = false;
    int   focusRail = 0;
    float focusDist = 0.0f, focusHeight = 0.0f;
    bool  openEnemyPanelPending = false;

    // 「カメラをここへ」を要求する（シーンが ConsumeFocusRequest で取り出す）
    void RequestFocus(int rail, float dist, float height){
        focusPending = true;
        focusRail    = rail;
        focusDist    = dist;
        focusHeight  = height;
    }
};
