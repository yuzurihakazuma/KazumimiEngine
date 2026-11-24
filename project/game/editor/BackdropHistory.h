#pragma once
#include "game/stage/StageBackdrop.h"

#include <memory>
#include <vector>

// =====================================================================
//  BackdropCommand：背景エディタの操作 1 回ぶん（コマンド パターン）。
//   「やり直す」＝ Execute、「元に戻す」＝ Undo。操作の中身（どの物をどう変えたか）は、派生クラスが持つ
// =====================================================================
class BackdropCommand {
public:
    virtual ~BackdropCommand() = default;
    // 操作を（もう一度）行う
    virtual void Execute(StageBackdrop& backdrop) const = 0;
    // 操作を取り消す
    virtual void Undo(StageBackdrop& backdrop) const = 0;
};

// =====================================================================
//  BackdropHistory：背景エディタの履歴（コマンドの並び）。
//   使い方：操作の前に BeginOperation → 背景を書き換える（何フレームかけてもよい）。
//   次の BeginOperation / Undo / Redo の時に、その間の変化を調べてコマンドを 1 つ作って積む
//   （物の数が変わらない操作は「変わった物だけ」、増減した操作は「前後の並び」を覚える）。
//   何も変わらなかった操作は積まない
// =====================================================================
class BackdropHistory {
public:
    // 操作の前に呼ぶ：今の並びを「操作の前」として覚える。やり直しの列は捨てる
    void BeginOperation(const StageBackdrop& backdrop);
    // 始めた操作を、記録しない事にする（クリックだけで終わった、など）
    void DiscardOperation(){ pending_ = false; }
    // 1 つ元に戻す／やり直す（できなければ false）
    bool Undo(StageBackdrop& backdrop);
    bool Redo(StageBackdrop& backdrop);
    bool CanUndo() const{ return pending_ || !undo_.empty(); }
    bool CanRedo() const{ return !redo_.empty(); }
    // 全部捨てる（自動配置を作り直した時。物の番号が合わなくなるので）
    void Clear();

private:
    // 始めた操作を確定する：操作の前と今の並びを比べて、コマンドにして積む
    void Seal(const StageBackdrop& backdrop);

    static constexpr size_t kLimit = 80; // 覚えておく操作の数
    std::vector<std::unique_ptr<BackdropCommand>> undo_, redo_;
    std::vector<BackdropItem> before_; // 始めた操作の、前の並び
    bool pending_ = false;             // 始めた操作が、まだコマンドになっていない
};
