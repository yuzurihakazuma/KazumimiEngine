#pragma once
// =====================================================================
//  StripControls：配置ビューの操作部（キャンバス以外の ImGui の部品）
//   ・上段：レールの選択・前後のレールへ・カメラをここへ・選択を連動・レールの性質のタグ
//   ・道具の選択（ブロック/敵/コイン）・置く物の種類・元に戻す/やり直し・使い方の (?)
//   ・表示の設定（横の位置・他の位置も薄く表示・拡大・全体・段数）
//   StripState を書き換えるだけで、途中の操作（塗り・ドラッグ）には触らない。
//   操作をやめる必要がある出来事は戻り値で RailStripPanel に知らせる
// =====================================================================
#include "game/editor/strip/StripContext.h"
#include "game/editor/strip/StripState.h"

class StripControls {
public:
    // 上段。戻り値：選ばれたレール（-1=なし。開くのは呼び出し側）
    int  DrawHeader(const StripContext& context, StripState& state) const;
    // 道具の選択・置く物の種類・元に戻す（道具が変わったかは state.tool の変化で分かる）
    void DrawToolbar(const StripContext& context, StripState& state) const;
    // 表示の設定。戻り値 true = 横の位置のボタンが押された（ブロックの塗りをやめる）
    bool DrawViewOptions(StripState& state) const;
};
