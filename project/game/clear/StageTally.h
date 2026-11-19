#pragma once
// =====================================================================
//  StageTally：集計係（クリア演出 企画書）。プレイ中の出来事を受け取って数えるだけ。
//   ・コンボが伸びた → いちばん長いコンボ
//   ・ダメージを受けた → 受けた回数
//   ・敵を倒した（踏んだ・卵を当てた・食べた）→ 倒した数
//   出来事は Signal（engine/utils/Signal.h）で届く。つなぐのはシーン（GamePlayScene）。
//   リザルト画面は結果を読むだけで、プレイの処理（卵・敵・体力）を直接は見ない
// =====================================================================

class StageTally {
public:
    // 数えた結果
    struct Result {
        int maxCombo = 0;         // 奥投げコンボのいちばん長い数
        int damageCount = 0;      // ダメージを受けた回数
        int enemiesDefeated = 0;  // 倒した敵の数
        int enemyTotal = 0;       // 置いてあった敵の数
    };

    // プレイ開始・やりなおし：最初から数え直す。enemyTotal＝ステージに置いてある敵の数
    void Reset(int enemyTotal);

    void NoteCombo(int combo);     // コンボが combo になった
    void NoteDamage();             // ダメージを受けた
    void NoteEnemyDefeated();      // 敵を 1 匹倒した

    const Result& GetResult() const{ return result_; }

private:
    Result result_;
};
