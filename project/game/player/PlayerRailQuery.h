#pragma once
// =====================================================================
//  PlayerRailQuery：プレイヤーの「次に乗るレール」を探す問い合わせ（探すだけで状態は変えない）。
//   ・FindJoin    … レールの端のすぐ前にある別レールの本体（動的ドッキング）
//   ・FindSwitch  … 押した方向に伸びている別タイプのレール（交差点での乗り換え）
//   ・FindBranch  … T字路の途中分岐（同じタイプ同士でも分岐できる）
//   ・FindLanding … 空中から降りてきた時に着地できるレール
//   ・IsOverHole  … 足元がどれかのレールの穴の真上か
//  見つけた後の乗り移り（高さの補正・進行符号・クールダウン）は Player が行う
// =====================================================================
#include "engine/math/struct.h"

#include <vector>

class SplineRail;

namespace PlayerRailQuery {
    // 見つかったレール上の地点
    struct Spot {
        int     rail = -1;
        float   dist = 0.0f;
        Vector3 pos {};
    };

    // 足元の位置が、どれかのレールの「穴」区間の真上にあるか。
    //   今乗っているレールだけでなく全レールの穴を見る（乗り換え地点で隣のレールに乗ったまま
    //   穴の上を通っても取りこぼさない）。範囲は穴の近くだけなので離れた所では落ちない
    bool IsOverHole(const std::vector<SplineRail>& rails, const Vector3& footPos);

    // レールの端 edgePos のすぐ前方にある別レールの本体（forwardDir=端から出て行く水平の向き。0なら向きを問わない）
    bool FindJoin(const std::vector<SplineRail>& rails, int currentRail,
                  const Vector3& edgePos, const Vector3& forwardDir, Spot& out);

    // 今のレールと反対タイプのレールのうち、足元の真上で交差していて、押した向き(switchDir=±1)へ伸びているもの
    bool FindSwitch(const std::vector<SplineRail>& rails, int currentRail, const Vector3& footPos,
                    bool currentHorizontal, int switchDir, Spot& out);

    // 今のレールの途中分岐（branchPoints）のうち、近くにあって押した向きに合うもの
    bool FindBranch(const std::vector<SplineRail>& rails, int currentRail, float currentDist,
                    int switchDir, Spot& out);

    // 空中の pos（前フレームの高さ prevY）から、降りてきて着地できるレール。ignoreRail は候補から外す
    bool FindLanding(const std::vector<SplineRail>& rails, const Vector3& pos, float prevY,
                     int ignoreRail, Spot& out);
}
