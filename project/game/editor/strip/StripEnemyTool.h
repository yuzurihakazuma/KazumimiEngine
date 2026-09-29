#pragma once
// =====================================================================
//  StripEnemyTool：展開図の道具「敵」
//   何もない所を左クリック=置く / 敵をドラッグ=移動（フワリンは上下で浮く高さ）/
//   橙の■をドラッグ=動ける範囲 / 右クリック=メニュー / Delete=選んでいる敵を消す。
//   編集先は EnemyEditor（元に戻す は敵の履歴）。展開図の上での Ctrl+Z / Ctrl+Y も敵の履歴へ回す。
//   このパネルの中で敵の選択を変えた時は StripState::lastSelectedEnemy も合わせて更新する
//   （「外から選ばれた」と取り違えてレールを開き直さないように）
// =====================================================================
#include "game/editor/strip/StripContext.h"
#include "game/editor/strip/StripLayout.h"
#include "game/editor/strip/StripState.h"

class StripEnemyTool {
public:
    // 戻り値 true = ドラッグ中の敵が消えていた等で、途中の操作を全部やめる必要がある
    bool Update(const StripContext& context, StripState& state, const StripLayout& layout, bool hovered);
    // 右クリックメニュー（開いていなければ何もしない）
    void DrawContextMenu(const StripContext& context, StripState& state);
    // 展開図の上での Ctrl+Z / Ctrl+Y を敵の履歴へ回す（マウスが展開図の上にある間、毎フレーム呼ぶ）。
    //   戻り値 true = 戻す/やり直しをした（途中の操作を全部やめる必要がある）
    bool HandleUndoKeys(const StripContext& context);

    void Cancel();
    bool IsBusy() const { return drag_ != Drag::None; }

private:
    enum class Drag { None, Move, RangeMin, RangeMax };

    // 敵を選ぶ（このパネルの中で選んだ時用。選択の変化を「外から選ばれた」と取り違えないよう覚えておく）
    void SelectEnemy(const StripContext& context, StripState& state, int index);
    // 敵エディタ側で選択が変わる操作（追加・削除・複製）の後に、今の選択を覚え直す
    void RememberSelection(const StripContext& context, StripState& state);

    Drag  drag_ = Drag::None;
    int   dragIndex_ = -1;
    float grabMouseY_ = 0.0f;         // つかんだ時のマウスY（上下に動かしたかの判定）
    bool  hoverAdjust_ = false;       // 上下ドラッグで浮く高さを変えている
    float rangeGrabOffset_ = 0.0f;    // 動ける範囲の■をつかんだ時の、■とマウスの距離の差
    bool  rangeAmbiguous_ = false;    // つかんだ■の両端が重なっていた（どちらを動かすかはドラッグの向きで決める）
    int   contextEnemy_ = -1;         // 右クリックメニューの対象
};
