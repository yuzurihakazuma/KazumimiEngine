#pragma once
// =====================================================================
//  StripRenderer：展開図の描画（奥から：背景 → 下の情報帯 → 目印 → ブロック → コイン → 敵、最後に段の番号）
//   状態を持たない。描く物はすべて引数（StripContext / StripState / StripLayout）から読む。
//   道具が操作中の物（移動中のブロック・ドラッグ中のコイン）は、道具から受け取った番号で描き分ける。
//   下の情報帯の「つながっているレール」のラベルだけはクリックを受け付けるので、結果を戻り値で返す
// =====================================================================
#include "engine/utils/Level/LevelData.h"
#include "game/editor/strip/StripContext.h"
#include "game/editor/strip/StripLayout.h"
#include "game/editor/strip/StripState.h"

class StripRenderer {
public:
    // つながっているレールのラベルの結果
    struct LinkResult {
        bool  overLabel = false;  // マウスがラベルの上にある（道具を動かさない）
        int   railSelect = -1;    // クリックで選ばれたレール（-1=なし。切り替えは描画の後で）
        float railDist = -1.0f;   // そのレールで最初に見せる距離（-1=先頭）
    };

    // 背景：マス目・目盛り・道
    static void DrawBackdrop(const StripContext& context, const StripState& state, const StripLayout& layout);
    // 下の情報帯：道の高さの変化・他のレールとのつながり。
    //   linksActive=false の間はラベルに反応しない（マウスが外にある・何かを操作している途中）
    static LinkResult DrawRailInfo(const StripContext& context, const StripState& state, const StripLayout& layout,
                                   bool linksActive);
    // スタート・ゴール・プレイヤー
    static void DrawMarkers(const StripContext& context, const StripState& state, const StripLayout& layout);
    // ブロック。movingBlock は移動中のブロック（元の場所に薄く残す。-1=なし）
    static void DrawBlocks(const StripContext& context, const StripState& state, const StripLayout& layout,
                           int movingBlock);
    // コイン。draggedCoin のコインは draggedData（移動先）の位置に描く（-1=なし）
    static void DrawCoins(const StripContext& context, const StripState& state, const StripLayout& layout,
                          int draggedCoin, const CoinData& draggedData);
    // 敵（体・番号と名前・動ける範囲）
    static void DrawEnemies(const StripContext& context, const StripState& state, const StripLayout& layout);
    // 段の番号。横へスクロールしても見えるよう、表示の左端に固定して描く
    static void DrawLevelLabels(const StripLayout& layout);
};
