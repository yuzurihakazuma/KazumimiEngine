#pragma once
// =====================================================================
//  ClearSequence：クリア演出の進行役が回す「場面の列」を組み立てる（クリア演出 企画書 4. 演出の流れ）。
//   輪をくぐった瞬間を 0 秒として：
//     輪をくぐる   … 殻が割れて開く。恐竜は高く跳び、カメラが横から正面へ回り込む
//     「ゴール！」 … 切り文字が下がり、星が飛び出す。恐竜は着地して決めポーズ → 喜びの踊り。紙吹雪
//     リザルトへ   … 景色が暗い緑に沈み、リザルトのマットが出る
//     リザルト     … 帯・3 行・花丸が順に出る（決定ボタンで早送り）
//     加算         … 決定ボタンで、今回のスタンプが所持数へ数え込まれる
//     幕           … 読み込み中のカード（段ボールの台紙に切り絵）で画面を覆い、次のシーンへ
//   場面は StepSequence<ClearContext> の IStep（始まる時・毎フレーム・終わる時）。長さは ClearTiming の秒数。
//   汽車に乗る・駅で降りる（企画書の段階 4）と、マップで道が伸びる所（ステージ選択のシーン）はここには無い
// =====================================================================
#include "engine/utils/StepSequence.h"
#include "game/clear/ClearContext.h"

namespace ClearSequence {
    using Sequence = StepSequence<ClearContext>;

    // context の timing（フル版・短縮版）で、場面の列を組み立てる
    void Build(Sequence& sequence, const ClearContext& context);

    // 恐竜の演技：レールの上の着地する所へ歩かせる自動操縦を入れ（着いたら何も押さない）、決めポーズの後は踊らせる。
    //   毎フレーム、プレイヤーの更新の前に呼ぶ
    void UpdateActor(ClearContext& context, float deltaTime);
}
