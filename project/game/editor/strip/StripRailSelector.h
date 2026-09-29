#pragma once
// =====================================================================
//  StripRailSelector：配置ビューで開くレールを決める
//   ・レールエディタ側の選択に追従する（作ったばかりでまだ開けないレールは「開く予定」として待つ）
//   ・敵エディタの一覧や Game View で別のレールの敵が選ばれたら、そのレールを開いて中央に見せる
//   ・開いていたレールが消えた/使えなくなったら、使える最初のレールへ移る
//   ・配置ビューの中で選んだレールを開き、レールエディタ側の選択も合わせる
//   レールが変わった時の「途中の操作をやめる」は呼び出し側（RailStripPanel）が行う（戻り値で知らせる）
// =====================================================================
#include "game/editor/strip/StripContext.h"
#include "game/editor/strip/StripState.h"

class StripRailSelector {
public:
    // 毎フレームの同期。戻り値 true = 開くレールが変わった等で、途中の操作をやめる必要がある
    bool Sync(const StripContext& context, StripState& state);
    // レールを開く。arrivalDist が 0 以上なら、その距離が表示の中央に来るようにする。
    //   戻り値 true = レールを切り替えた（途中の操作をやめる必要がある）
    bool Select(const StripContext& context, StripState& state, int railIndex, float arrivalDist = -1.0f);

private:
    int lastEditorRail_ = -1;    // レールエディタ側の選択（変わったら追従する）
    int pendingFollowRail_ = -1; // まだ開けない（作ったばかり等）ので、開けるようになったら開くレール
};
