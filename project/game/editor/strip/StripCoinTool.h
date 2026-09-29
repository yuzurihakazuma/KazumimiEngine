#pragma once
// =====================================================================
//  StripCoinTool：展開図の道具「コイン」
//   左クリック=置く（高さはマウスの位置）/ ドラッグ=移動 / 右クリック=消す。
//   編集先は RailEditor のコイン（元に戻す はレールの履歴）。
//   ドラッグ中は移動先を覚えて描くだけで、書き込むのは離した時（軽い・途中の Ctrl+Z で壊れない）
// =====================================================================
#include "engine/utils/Level/LevelData.h"
#include "game/editor/strip/StripContext.h"
#include "game/editor/strip/StripLayout.h"
#include "game/editor/strip/StripState.h"

class StripCoinTool {
public:
    void Update(const StripContext& context, const StripState& state, const StripLayout& layout, bool hovered);

    void Cancel() { dragIndex_ = -1; }
    bool IsBusy() const { return dragIndex_ >= 0; }
    // ドラッグ中のコイン（-1=なし）と、その移動先。描画で移動先の位置に描くのに使う
    int             DraggedCoin() const { return dragIndex_; }
    const CoinData& DraggedData() const { return dragData_; }

private:
    int      dragIndex_ = -1;
    CoinData dragData_ {};   // ドラッグ中の移動先
    CoinData dragOrig_ {};   // つかんだ時の中身（離した時、別のコインを上書きしないかの照合用）
};
