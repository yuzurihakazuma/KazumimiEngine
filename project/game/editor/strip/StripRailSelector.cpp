#include "game/editor/strip/StripRailSelector.h"

#include "engine/rail/SplineRail.h"
#include "engine/utils/Level/RailEditor.h"
#include "game/editor/strip/StripCommon.h"
#include "game/enemy/EnemyEditor.h"

using namespace railstrip;

// =====================================================================
//  レールの選択
//   途中の操作（塗り・ドラッグ）は、ここでは読みも書きもしない。やめる必要がある時は戻り値で知らせ、
//   呼び出し側がこの関数の後でまとめてやめる（途中でやめても最後にやめても結果は同じ）
// =====================================================================
bool StripRailSelector::Sync(const StripContext& context, StripState& state){
    const auto& rails = *context.rails;
    const int railCount = ( int ) rails.size();
    bool cancel = false;

    // レールエディタ側で選択が変わったら、こちらも同じレールを開く。
    //   作ったばかりのレールは、まだゲーム側のレールに入っていない（次のフレームで作られる）か、
    //   点が足りないことがある。その時は「開く予定」として覚えておき、開けるようになったら開く
    int editorRail = context.railEditor->GetCurrentRailIndex();
    if ( editorRail != lastEditorRail_ ) {
        lastEditorRail_ = editorRail; // 見た値はいつも覚える（元のレールへ選び直した時も変化として拾える）
        pendingFollowRail_ = ( state.followEditorRail && editorRail != state.currentRail ) ? editorRail : -1;
    }
    if ( pendingFollowRail_ >= 0 ) {
        if ( !state.followEditorRail || pendingFollowRail_ != editorRail ) {
            pendingFollowRail_ = -1; // 連動を切った／エディタ側が別のレールへ移った
        } else if ( pendingFollowRail_ < railCount && RailUsable(rails[pendingFollowRail_]) ) {
            if ( pendingFollowRail_ != state.currentRail ) {
                state.currentRail = pendingFollowRail_;
                state.scrollResetPending = true;
                cancel = true;
            }
            pendingFollowRail_ = -1;
        }
    }

    // 敵エディタの一覧や Game View で別のレールの敵が選ばれたら、その敵のいるレールを開いて中央に見せる
    //   （レールエディタ側の選択は変えない。このパネルの中で選んだ時は lastSelectedEnemy が先に更新済み）
    const int selectedEnemy = context.enemyEditor->GetSelectedEntry();
    if ( selectedEnemy != state.lastSelectedEnemy ) {
        state.lastSelectedEnemy = selectedEnemy;
        const auto& spawns = context.enemyEditor->GetSpawnDatas();
        if ( state.followEditorRail && selectedEnemy >= 0 && selectedEnemy < ( int ) spawns.size() ) {
            const EnemySpawnData& spawn = spawns[selectedEnemy];
            if ( spawn.railIndex >= 0 && spawn.railIndex < railCount && RailUsable(rails[spawn.railIndex]) ) {
                if ( spawn.railIndex != state.currentRail ) {
                    state.currentRail = spawn.railIndex;
                    cancel = true;
                }
                state.scrollToDist = ShownDist(spawn.distance, rails[state.currentRail].GetLength());
            }
        }
    }

    // 開いていたレールが消えた/短くなりすぎた時は、使える最初のレールへ移る
    if ( state.currentRail < 0 || state.currentRail >= railCount || !RailUsable(rails[state.currentRail]) ) {
        state.currentRail = -1;
        for ( int i = 0; i < railCount; ++i ) {
            if ( RailUsable(rails[i]) ) { state.currentRail = i; break; }
        }
        state.scrollResetPending = true;
        state.scrollToDist = -1.0f;
        cancel = true;
    }
    return cancel;
}

bool StripRailSelector::Select(const StripContext& context, StripState& state, int railIndex, float arrivalDist){
    const auto& rails = *context.rails;
    if ( railIndex < 0 || railIndex >= ( int ) rails.size() || !RailUsable(rails[railIndex]) ) return false;
    if ( railIndex == state.currentRail ) return false;
    state.currentRail = railIndex;
    // つながりをたどって来た時は着いた場所を、それ以外はレールの先頭を見せる
    if ( arrivalDist >= 0.0f ) { state.scrollToDist = ShownDist(arrivalDist, rails[railIndex].GetLength()); }
    else                       { state.scrollResetPending = true; state.scrollToDist = -1.0f; }
    // レールエディタと Game View の選択も合わせる（編集対象を固定している間は触らない）
    if ( state.followEditorRail && !context.railEditor->IsEditTargetLocked() ) {
        context.railEditor->SetCurrentRail(railIndex);
        context.railEditor->ClearMultiSelection(); // 前のレールの点が選ばれたまま残らないように
    }
    lastEditorRail_ = context.railEditor->GetCurrentRailIndex();
    pendingFollowRail_ = -1; // ここで選んだので、待っていた「開く予定」は取り消す
    return true;
}
