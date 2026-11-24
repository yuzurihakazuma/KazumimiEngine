#include "game/editor/BackdropHistory.h"

#include <utility>

namespace {
    // 物の数は変わらず、いくつかの物の中身だけが変わった操作（動かす・回す・大きさ・地面を塗る など）。
    //   変わった物の番号と、前後の中身だけを持つ
    class EditItemsCommand final : public BackdropCommand {
    public:
        // 変わった物 1 つ（番号と、前後の中身）
        struct Change {
            int index;
            BackdropItem before;
            BackdropItem after;
        };
        explicit EditItemsCommand(std::vector<Change> changes) : changes_(std::move(changes)) {}
        void Execute(StageBackdrop& backdrop) const override { Apply(backdrop, true); }
        void Undo(StageBackdrop& backdrop) const override { Apply(backdrop, false); }

    private:
        void Apply(StageBackdrop& backdrop, bool forward) const {
            std::vector<BackdropItem> items = backdrop.Items();
            for ( const Change& change : changes_ ) {
                if ( change.index < 0 || change.index >= static_cast<int>( items.size() ) ) { continue; }
                items[static_cast<size_t>( change.index )] = forward ? change.after : change.before;
            }
            backdrop.SetItems(items);
        }
        std::vector<Change> changes_;
    };

    // 物が増えた・減った操作（置く・消す・複製・ブラシ・線で引く道 など）。番号がずれるので、前後の並びを持つ
    class ReplaceItemsCommand final : public BackdropCommand {
    public:
        ReplaceItemsCommand(std::vector<BackdropItem> before, std::vector<BackdropItem> after)
            : before_(std::move(before)), after_(std::move(after)) {}
        void Execute(StageBackdrop& backdrop) const override { backdrop.SetItems(after_); }
        void Undo(StageBackdrop& backdrop) const override { backdrop.SetItems(before_); }

    private:
        std::vector<BackdropItem> before_;
        std::vector<BackdropItem> after_;
    };

    // 操作の前後の並びから、コマンドを作る（何も変わっていなければ nullptr）
    std::unique_ptr<BackdropCommand> MakeCommand(std::vector<BackdropItem> before, const std::vector<BackdropItem>& after){
        if ( before.size() != after.size() ) {
            return std::make_unique<ReplaceItemsCommand>(std::move(before), after);
        }
        std::vector<EditItemsCommand::Change> changes;
        for ( size_t i = 0; i < before.size(); ++i ) {
            if ( !( before[i] == after[i] ) ) { changes.push_back({ static_cast<int>( i ), before[i], after[i] }); }
        }
        if ( changes.empty() ) { return nullptr; }
        return std::make_unique<EditItemsCommand>(std::move(changes));
    }
}

void BackdropHistory::BeginOperation(const StageBackdrop& backdrop){
    Seal(backdrop);
    before_ = backdrop.Items();
    pending_ = true;
    redo_.clear();
}

void BackdropHistory::Seal(const StageBackdrop& backdrop){
    if ( !pending_ ) { return; }
    pending_ = false;
    std::unique_ptr<BackdropCommand> command = MakeCommand(std::move(before_), backdrop.Items());
    before_.clear();
    if ( !command ) { return; } // 何も変わらなかった操作は積まない
    undo_.push_back(std::move(command));
    if ( undo_.size() > kLimit ) { undo_.erase(undo_.begin()); }
}

bool BackdropHistory::Undo(StageBackdrop& backdrop){
    Seal(backdrop);
    if ( undo_.empty() ) { return false; }
    std::unique_ptr<BackdropCommand> command = std::move(undo_.back());
    undo_.pop_back();
    command->Undo(backdrop);
    redo_.push_back(std::move(command));
    return true;
}

bool BackdropHistory::Redo(StageBackdrop& backdrop){
    Seal(backdrop);
    if ( redo_.empty() ) { return false; }
    std::unique_ptr<BackdropCommand> command = std::move(redo_.back());
    redo_.pop_back();
    command->Execute(backdrop);
    undo_.push_back(std::move(command));
    return true;
}

void BackdropHistory::Clear(){
    undo_.clear();
    redo_.clear();
    before_.clear();
    pending_ = false;
}
