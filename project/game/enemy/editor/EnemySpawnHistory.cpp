#include "game/enemy/editor/EnemySpawnHistory.h"

void EnemySpawnHistory::Reset(const Snapshot& current){
    undoStack_.clear();
    redoStack_.clear();
    lastCommitted_ = current;
}

void EnemySpawnHistory::Commit(const Snapshot& current){
    if ( current == lastCommitted_ ) return;
    undoStack_.push_back(lastCommitted_);
    if ( undoStack_.size() > kMaxHistory ) { undoStack_.erase(undoStack_.begin()); }
    redoStack_.clear();
    lastCommitted_ = current;
}

bool EnemySpawnHistory::Undo(Snapshot& current){
    // まだ履歴に積んでいない変更（名前を打っている途中に「元に戻す」を押した等）は、先に1手として積む。
    //   積まずに戻すと、その変更とひとつ前の操作が1回でまとめて戻ってしまう
    Commit(current);
    if ( undoStack_.empty() ) return false;
    redoStack_.push_back(current);
    current = undoStack_.back();
    undoStack_.pop_back();
    lastCommitted_ = current;
    return true;
}

bool EnemySpawnHistory::Redo(Snapshot& current){
    Commit(current); // 積んでいない変更があれば、それが最新の手になる（やり直す先は無くなる）
    if ( redoStack_.empty() ) return false;
    undoStack_.push_back(current);
    current = redoStack_.back();
    redoStack_.pop_back();
    lastCommitted_ = current;
    return true;
}
