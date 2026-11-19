#pragma once
#include "game/enemy/Enemy.h" // EnemySpawnData
#include <cstddef>
#include <vector>

// =====================================================================
//  EnemySpawnHistory : 敵の配置データ（EnemySpawnData の配列）の元に戻す／やり直し履歴
//   「1回の操作＝1手」にするため、操作の途中では積まず、呼び出し側が区切りの良い時
//   （手を離した時など）に Commit を呼ぶ。前回積んだ状態（控え）と比べて変わっていれば1手になる。
//   配置データそのものは持たない（正本は EnemySpawnModel。ここは引数でもらって比べる・書き戻すだけ）
// =====================================================================
class EnemySpawnHistory {
public:
    using Snapshot = std::vector<EnemySpawnData>;

    // 履歴を捨てて、今の状態を控えにする（初期化・マップ読込時。前のマップの敵へ戻せないように）
    void Reset(const Snapshot& current);

    // 前回から変わっていれば、今の状態を履歴の1手として積む
    void Commit(const Snapshot& current);

    // current を1手戻す／やり直す。実際に中身を入れ替えたら true
    bool Undo(Snapshot& current);
    bool Redo(Snapshot& current);

    // まだ履歴に積んでいない変更（文字を打っている途中など）も「戻せる」に数える
    bool CanUndo(const Snapshot& current) const { return !undoStack_.empty() || !IsClean(current); }
    bool CanRedo() const { return !redoStack_.empty(); }

    // 積んでいない変更が無いか／今の状態をそのまま控えにする（その変化を履歴の1手に数えない）
    bool IsClean(const Snapshot& current) const { return current == lastCommitted_; }
    void Rebase(const Snapshot& current) { lastCommitted_ = current; }

    // 控えと履歴の中身すべてに同じ書き換えをする（レール番号の付け替え用）。
    //   控えも同じに書き換えるので、この変化は1手に数えない
    template <class Fn>
    void ForEachSnapshot(Fn&& fn) {
        fn(lastCommitted_);
        for ( auto& snapshot : undoStack_ ) { fn(snapshot); }
        for ( auto& snapshot : redoStack_ ) { fn(snapshot); }
    }

private:
    std::vector<Snapshot> undoStack_;
    std::vector<Snapshot> redoStack_;
    Snapshot lastCommitted_; // 履歴に積んだ最後の状態
    static constexpr size_t kMaxHistory = 60;
};
