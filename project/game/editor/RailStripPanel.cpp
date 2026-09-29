#include "game/editor/RailStripPanel.h"

#include "externals/imgui/imgui.h"

bool RailStripPanel::ConsumeFocusRequest(int& outRail, float& outDist, float& outHeight){
    if ( !state_.focusPending ) return false;
    state_.focusPending = false;
    outRail = state_.focusRail; outDist = state_.focusDist; outHeight = state_.focusHeight;
    return true;
}

bool RailStripPanel::ConsumeOpenEnemyPanelRequest(){
    bool pending = state_.openEnemyPanelPending;
    state_.openEnemyPanelPending = false;
    return pending;
}

// 途中の操作（塗り・ドラッグ）を全部やめる。レールや道具を切り替えた時に呼ぶ
void RailStripPanel::CancelInteractions(){
    canvas_.CancelInteractions();
}

void RailStripPanel::SelectRail(const Context& context, int railIndex, float arrivalDist){
    if ( railSelector_.Select(context, state_, railIndex, arrivalDist) ) { CancelInteractions(); }
}

// =====================================================================
//  窓
// =====================================================================
void RailStripPanel::Draw(const Context& context){
#ifdef USE_IMGUI
    // 前のフレームに描かれていなかった（エディタを隠した・パネルを閉じた等）なら、途中の操作は捨てる。
    //   残しておくと、次に開いた瞬間に「ボタンを離した」扱いになって、古い範囲のまま塗られたり動いたりする
    const int frame = ImGui::GetFrameCount();
    if ( lastDrawFrame_ >= 0 && frame != lastDrawFrame_ + 1 ) { CancelInteractions(); }
    lastDrawFrame_ = frame;
    ImGui::SetNextWindowSize(ImVec2(980.0f, 330.0f), ImGuiCond_FirstUseEver);
    if ( !ImGui::Begin(kWindowTitle) ) {
        // 畳まれている/タブの裏にいる間は操作を続けない
        CancelInteractions();
        ImGui::End();
        return;
    }
    if ( !context.rails || !context.railEditor || !context.enemyEditor || context.rails->empty() ) {
        ImGui::TextDisabled("レールがありません（先にレールエディタでレールを作ってください）");
        ImGui::End();
        return;
    }
    if ( railSelector_.Sync(context, state_) ) { CancelInteractions(); }
    if ( state_.currentRail < 0 ) {
        ImGui::TextDisabled("点が2つ以上あるレールがありません（先にレールエディタでレールを作ってください）");
        ImGui::End();
        return;
    }
    if ( !context.editable ) { CancelInteractions(); }

    // 横に広い時：操作部を左の列にまとめ、展開図は右側で高さいっぱいに使う
    //   （このパネルは横長・低めの場所に置くことが多い。操作部を上に積むとマス目の段が見えなくなる）
    // 幅が狭い時：操作部を上に積む
    const float kControlsWidth = 336.0f;
    const bool sideBySide = ( ImGui::GetContentRegionAvail().x >= kControlsWidth + 420.0f );
    if ( sideBySide ) {
        ImGui::BeginChild("##stripControls", ImVec2(kControlsWidth, 0.0f));
        DrawControls(context);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginGroup();
        DrawCanvas(context);
        ImGui::EndGroup();
    } else {
        DrawControls(context);
        DrawCanvas(context);
    }
    ImGui::End();
#else
    ( void ) context;
#endif
}

// ----------------------------------------------------------------
//  操作部：上段 → 道具 → 表示の設定
//   出来事への対応（レールを開く・途中の操作をやめる）は、それぞれを描いた直後に行う
//   （次の部品や展開図が、切り替わった後の状態で描かれるように）
// ----------------------------------------------------------------
void RailStripPanel::DrawControls(const Context& context){
    const int requestedRail = controls_.DrawHeader(context, state_);
    if ( requestedRail >= 0 ) { SelectRail(context, requestedRail); }

    const StripTool toolBefore = state_.tool;
    controls_.DrawToolbar(context, state_);
    if ( state_.tool != toolBefore ) { CancelInteractions(); }

    // 横の位置を変えたら、ブロックの塗りは続けない
    if ( controls_.DrawViewOptions(state_) ) { canvas_.CancelBlockStroke(); }
}

// ----------------------------------------------------------------
//  展開図
// ----------------------------------------------------------------
void RailStripPanel::DrawCanvas(const Context& context){
    const StripCanvas::RailRequest request = canvas_.Draw(context, state_);
    // レールの切り替えは、このフレームの描画が全部終わってから（描画中の参照を壊さない）
    if ( request.rail >= 0 ) { SelectRail(context, request.rail, request.dist); }
}
