#pragma once
// =====================================================================
//  RailMotion：動くレール（リフト・列車）と「後から出現する道」の時間の進め方。
//   レールの基準位置は変えず、animOffset（平行移動）と animYaw（列車式の転回）だけを動かす。
//   波形は motionType で選択：0=サイン往復 / 1=端で一時停止つき往復 / 2=円運動 / 3=ガイドレール追従。
//   motionPhase(0〜1)で複数レールの動きをずらせる（0.5=半周期ずれ）。
//   motionTrigger=1（乗ったら動き出す）のレールは、プレイヤーが乗るまで基準位置で待機する
// =====================================================================
#include <vector>

class SplineRail;

namespace RailMotion {
    // ridingRail に渡すと全レールを強制発動する値（エディタのプレビュー用）
    constexpr int kStartAll = -2;

    // 時間を進める。ridingRail=プレイヤーが今乗っているレール番号（-1=誰も乗っていない）。
    //   動く設定のレールが1本でもあれば true（呼び出し側が緑線の位置を追従させる）
    bool Advance(std::vector<SplineRail>& rails, float dt, int ridingRail);

    // 基準位置へ戻す（発動状態・出現状態・列車の向きもリセット）
    void Reset(std::vector<SplineRail>& rails);
}
