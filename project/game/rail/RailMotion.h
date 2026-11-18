#pragma once
// =====================================================================
//  RailMotion：動くレール（リフト・列車）と「後から出現する道」の時間の進め方。
//   レールの基準位置は変えず、animOffset（平行移動）と animYaw（列車式の転回）だけを動かす。
//   波形は motionType で選択：0=サイン往復 / 1=端で一時停止つき往復 / 2=円運動 / 3=ガイドレール追従。
//   motionPhase(0〜1)で複数レールの動きをずらせる（0.5=半周期ずれ）。
//   motionTrigger=1（乗ったら動き出す）のレールは、プレイヤーが乗るまで基準位置で待機する
// =====================================================================
#include "engine/math/struct.h"

#include <vector>

namespace KazumimiEngine { class SplineRail; }

namespace RailMotion {
    // ridingRail に渡すと全レールを強制発動する値（エディタのプレビュー用）
    constexpr int kStartAll = -2;

    // 時間を進める。ridingRail=プレイヤーが今乗っているレール番号（接地している時だけ。-1=誰も乗っていない）。
    //   playerRail＝プレイヤーのレール（跳んでいる間も。列車の「乗っている」に使う。-1＝分からない＝ridingRail と同じ）
    //   動く設定のレールが1本でもあれば true（呼び出し側が緑線の位置を追従させる）
    bool Advance(std::vector<KazumimiEngine::SplineRail>& rails, float dt, int ridingRail, int playerRail = -1);

    // 基準位置へ戻す（発動状態・出現状態・列車の向きもリセット。列車は元の駅に置く）。
    //   keepRolls＝渡し板「ころころ」を広げたままにする（落ちて戻った時。一度広げたらそのまま）
    void Reset(std::vector<KazumimiEngine::SplineRail>& rails, bool keepRolls = false);

    // ===== 列車 =====
    constexpr float kTrainEngineLength = 3.45f; // 機関車の長さ(m)（うしろのはし〜前のライト。game/rail/ToyTrain と同じ）
    constexpr float kTrainEngineBack   = 1.7f;  // 機関車の真ん中から、うしろのはしまで(m)
    constexpr float kTrainCoupling     = 0.3f;  // 客車と機関車のすき間(m)

    // 列車の駅と機関車の向き（ガイドの区間・足場の長さ・機関車の有無から）
    struct TrainLayout {
        float station[2] { 0.0f, 0.0f }; // 駅に止まった時の足場の真ん中（ガイド上の距離）。0＝区間のはじめ側
        float engineSign = 1.0f;          // 機関車のある側（ガイドの進む向きで +1 / -1）
        bool  valid = false;
    };
    TrainLayout MakeTrainLayout(const KazumimiEngine::SplineRail& train, const std::vector<KazumimiEngine::SplineRail>& rails);
    // 列車の元の駅と向きを決める（Sync の後に1回。ガイドが決まってから）
    void SetupTrain(KazumimiEngine::SplineRail& train, const std::vector<KazumimiEngine::SplineRail>& rails);
    // 渡し板のちょうつがいの側と、どちらの駅の板かを決める（SetupTrain の後に1回）
    void SetupBoard(KazumimiEngine::SplineRail& board, const std::vector<KazumimiEngine::SplineRail>& rails);
    // 渡し板が開いているべきか（列車がその駅に止まっている）
    bool BoardShouldOpen(const KazumimiEngine::SplineRail& board, const std::vector<KazumimiEngine::SplineRail>& rails);
    // 渡し板「ころころ」：転がり出すはしを決める（ほかの道とつながっている方。RailConnections の後に1回）
    void SetupRollBoard(KazumimiEngine::SplineRail& board);
    // ころころの巻いたマット（筒）の今の真ん中と半径。卵の当たりと見た目で同じ物を使う
    struct RollPose {
        KazumimiEngine::Vector3 hinge {};    // 転がり出すはし（板の上の面）
        KazumimiEngine::Vector3 along {};    // 転がる向き（水平・長さ1）
        float   length = 0.0f;
        KazumimiEngine::Vector3 center {};   // 筒の真ん中
        float   radius = 0.0f;
        float   rolled = 0.0f; // 転がった距離(m)
    };
    bool GetRollPose(const KazumimiEngine::SplineRail& board, RollPose& out);
}
