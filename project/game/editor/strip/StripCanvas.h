#pragma once
// =====================================================================
//  StripCanvas：展開図のキャンバス（横スクロールできる子ウィンドウ）
//   ・このフレームの寸法（StripLayout）を決める（拡大率・表示する段数・「全体」ボタン）
//   ・子ウィンドウとキャンバス全体のボタンを作り、スクロール位置の要求（先頭へ/指定の距離へ）を反映する
//   ・StripRenderer で描き、選んでいる道具（ブロック/敵/コイン）を動かす
//   ・キー操作（道具・種類・横の位置・F・Esc・敵の Ctrl+Z）、中ボタンドラッグとホイールでの表示の移動・拡大
//   ・下の説明行（いま指している場所）
//   3つの道具はここが持つ。途中の操作（塗り・ドラッグ・表示の移動）をまとめてやめる窓口もここ
// =====================================================================
#include "game/editor/strip/StripBlockTool.h"
#include "game/editor/strip/StripCoinTool.h"
#include "game/editor/strip/StripContext.h"
#include "game/editor/strip/StripEnemyTool.h"
#include "game/editor/strip/StripLayout.h"
#include "game/editor/strip/StripState.h"

class StripCanvas {
public:
    // 展開図の中のリンク（つながっているレールのラベル）で選ばれたレール
    struct RailRequest {
        int   rail = -1;     // -1=なし
        float dist = -1.0f;  // そのレールで最初に見せる距離（-1=先頭）
    };

    // 戻り値のレールは、このフレームの描画が全部終わってから呼び出し側が開く（描画中の参照を壊さない）
    RailRequest Draw(const StripContext& context, StripState& state);

    // 途中の操作（塗り・ドラッグ・表示の移動）を全部やめる。レールや道具を切り替えた時に呼ぶ
    void CancelInteractions();
    // ブロックの塗り・移動だけをやめる（横の位置を切り替えた時）
    void CancelBlockStroke() { blockTool_.Cancel(); }

private:
    // マウスが指している場所（展開図の上にある時だけ valid）
    struct Hover {
        bool  valid = false;
        float dist = 0.0f;
        float height = 0.0f;
    };

    // 何かを塗っている/動かしている途中か（表示の移動は含まない）
    bool IsInteracting() const;
    // 寸法を決める（キャンバスの左上の位置以外。子ウィンドウを作る前に大きさが要るため）
    StripLayout MeasureLayout(const StripContext& context, StripState& state);
    // スクロール位置の要求（指定の距離を中央へ / 先頭へ）を反映する
    void ApplyScrollRequests(StripState& state, const StripLayout& layout);
    void HandleKeys(const StripContext& context, StripState& state, const StripLayout& layout, const Hover& hover);
    void HandlePanZoom(StripState& state, const StripLayout& layout, bool hovered);
    void DrawStatusLine(const StripState& state, const StripLayout& layout, const Hover& hover) const;

    StripBlockTool blockTool_;
    StripEnemyTool enemyTool_;
    StripCoinTool  coinTool_;

    bool  panning_ = false;           // 中ボタンドラッグで表示を動かしている
    float canvasViewWidth_ = 0.0f;    // 展開図の見えている幅（「全体」ボタンの拡大率の計算用。前のフレームの値）
    int   lastLevels_ = 0;            // 前のフレームの段数（操作の途中で段数が変わらないようにする）
};
