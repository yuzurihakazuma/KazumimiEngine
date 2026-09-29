#pragma once
// =====================================================================
//  StripBlockTool：展開図の道具「ブロック」
//   左クリック/ドラッグ=置く（置いてあるブロックの上では塗り替え）・右=消す・
//   Shift+ドラッグ=四角くまとめて・Ctrl+ドラッグ=移動・Alt+クリック=スポイト。
//   編集先は RailEditor のブロック（元に戻す はレールの履歴に入る）。
//   置ける/置けないの判定（占有幅の重なり・ループのつなぎ目・半マスずらし）もここに持つ
// =====================================================================
#include "engine/utils/Level/LevelData.h"
#include "game/editor/strip/StripContext.h"
#include "game/editor/strip/StripLayout.h"
#include "game/editor/strip/StripState.h"

class StripBlockTool {
public:
    void Update(const StripContext& context, const StripState& state, const StripLayout& layout, bool hovered);

    // 途中の塗り・移動をやめる（何も適用しない）
    void Cancel();
    // Esc 用：「四角くまとめて」「移動」の途中ならやめる（ふつうの塗りは続ける）
    void CancelRectOrMove();

    // 何かを塗っている/動かしている途中か
    bool IsBusy() const { return stroke_ != StrokeKind::None; }
    // 移動中のブロック（実際に動かし始めた後だけ。-1=なし）。描画で元の場所を薄く残すのに使う
    int  MovingBlock() const { return ( stroke_ == StrokeKind::Move && moveDragged_ ) ? moveIndex_ : -1; }

private:
    enum class StrokeKind { None, Place, Replace, Erase, RectFill, RectErase, Move };

    // --- ブロックの判定 ---
    // 指している位置（距離・段）にあるブロックの番号（GetBlocks の番号。-1=なし）
    int  FindBlockAt(const StripContext& context, const StripState& state, float dist, int level) const;
    // そのマスに type のブロックを置けるか（隣のブロックと重ならないか）。
    //   ignoreIndex のブロックは無いものとして扱う（移動の時、動かしている本人とは比べない）
    bool CanPlaceBlock(const StripContext& context, const StripState& state,
                       float dist, int level, int type, int ignoreIndex = -1) const;
    // マウスの位置から、実際に置く距離を決める（置ける場所が無ければ false）。
    //   まずマスの中心、だめなら半マスずらした位置を試す（2m のブロックを隣へぴったり付けられる）
    bool ResolvePlaceDist(const StripContext& context, const StripState& state, float mouseDist, int level, int type,
                          int ignoreIndex, float& outDist) const;
    void ApplyBlockStroke(const StripContext& context, const StripState& state, int cellDist, int level, float probeDist);
    // 四角くまとめての範囲（始点と終点を小さい順に並べ直したもの）
    void RectBounds(int& distFrom, int& distTo, int& levelFrom, int& levelTo) const;

    // ブロックの塗り
    StrokeKind stroke_ = StrokeKind::None;
    int   strokeButton_ = 0;          // 塗りに使っているマウスボタン（0=左 / 1=右）
    int   strokeLastDist_ = 0, strokeLastLevel_ = 0;
    int   rectStartDist_ = 0, rectStartLevel_ = 0;
    int   rectEndDist_ = 0,   rectEndLevel_ = 0;

    // ブロックの移動（Ctrl＋ドラッグ）
    int       moveIndex_ = -1;        // 動かしているブロック（GetBlocks の番号）
    BlockData moveOrig_ {};           // つかんだ時の中身（途中で配列が変わっていないかの照合用）
    float     moveDist_ = 0.0f;       // 移動先の距離
    int       moveLevel_ = 0;         // 移動先の段
    bool      moveValid_ = false;     // 移動先に置けるか
    bool      moveDragged_ = false;   // 実際に動かし始めたか（クリックだけなら何もしない）
};
