#include "game/craft/editor/CraftCommands.h"

void CraftTransformCommand::Apply(CraftStage& stage, const CraftObject& value){
    if ( CraftObject* object = stage.FindMutable(value.id) ) {
        object->position = value.position;
        object->rotationDeg = value.rotationDeg;
        object->scale = value.scale;
        object->layer = value.layer;
        stage.Touch();
    }
}

void CraftHistory::Execute(CraftStage& stage, std::unique_ptr<CraftCommand> command){
    if ( !command ) { return; }
    command->Execute(stage);
    PushDone(std::move(command));
}

void CraftHistory::PushDone(std::unique_ptr<CraftCommand> command){
    if ( !command ) { return; }
    // 元に戻した後の新しい操作：カーソルより右（やり直し候補）は捨てる
    if ( cursor_ < ( int ) entries_.size() ) {
        entries_.erase(entries_.begin() + cursor_, entries_.end());
        if ( savedCursor_ > cursor_ ) { savedCursor_ = -1; } // 保存した時点へはもう戻れない
    }
    entries_.push_back(std::move(command));
    ++cursor_;
    // 200個を超えたら古いものから捨てる
    while ( ( int ) entries_.size() > kMaxEntries ) {
        entries_.erase(entries_.begin());
        --cursor_;
        if ( savedCursor_ >= 0 ) { --savedCursor_; }
        if ( savedCursor_ < 0 ) { savedCursor_ = -1; } // 保存した位置が捨てられた：常に未保存
    }
}

bool CraftHistory::Undo(CraftStage& stage){
    if ( !CanUndo() ) { return false; }
    --cursor_;
    entries_[cursor_]->Undo(stage);
    return true;
}

bool CraftHistory::Redo(CraftStage& stage){
    if ( !CanRedo() ) { return false; }
    entries_[cursor_]->Execute(stage);
    ++cursor_;
    return true;
}

void CraftHistory::Clear(){
    entries_.clear();
    cursor_ = 0;
    savedCursor_ = 0;
}
