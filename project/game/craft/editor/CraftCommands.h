#pragma once
// =====================================================================
//  CraftCommands：箱庭エディタの「元に戻す / やり直し」。
//   すべての編集を「コマンド」1つにして履歴に積む。コマンドは「実行」と「取り消し」を持ち、
//   履歴の中のカーソルを左右に動かすだけで元に戻す・やり直しができる。
//
//   守るルール
//   ・コマンドはポインタではなく id で物を指す（消して戻すと実体のアドレスが変わるため）
//   ・エディタの画面はステージを直接書き換えない。必ずコマンドを作って履歴に渡す
//   ・元に戻した状態で新しい操作をすると、カーソルより右は捨てる
//   ・履歴は200個まで。保存した位置が捨てられたら「常に未保存」扱い
//   ・カーソルと保存した位置がずれていれば未保存（元に戻して保存時点まで戻れば「*」も消える）
// =====================================================================
#include "game/craft/CraftStage.h"

#include <memory>
#include <string>
#include <vector>

class CraftCommand {
public:
    virtual ~CraftCommand() = default;
    virtual void Execute(CraftStage& stage) = 0;
    virtual void Undo(CraftStage& stage) = 0;
    virtual std::string Label() const = 0; // 履歴の一覧に出す説明
};

// 置く・複製：作った物の全データ（id を含む）を覚え、取り消しはその id の物を消す
class CraftCreateCommand : public CraftCommand {
public:
    explicit CraftCreateCommand(const CraftObject& object) : object_(object) {}
    void Execute(CraftStage& stage) override{ stage.Add(object_); }
    void Undo(CraftStage& stage) override{ stage.Remove(object_.id); }
    std::string Label() const override{ return "置く: " + object_.asset; }
private:
    CraftObject object_;
};

// 削除：消した物の全データを覚え、取り消しは同じ id で作り直す
class CraftDeleteCommand : public CraftCommand {
public:
    explicit CraftDeleteCommand(const CraftObject& object) : object_(object) {}
    void Execute(CraftStage& stage) override{ stage.Remove(object_.id); }
    void Undo(CraftStage& stage) override{ stage.Add(object_); }
    std::string Label() const override{ return "削除: " + object_.asset; }
private:
    CraftObject object_;
};

// 位置・回転・拡縮（と層）の変更：変更前後の値を覚える
class CraftTransformCommand : public CraftCommand {
public:
    CraftTransformCommand(const CraftObject& before, const CraftObject& after) : before_(before), after_(after) {}
    void Execute(CraftStage& stage) override{ Apply(stage, after_); }
    void Undo(CraftStage& stage) override{ Apply(stage, before_); }
    std::string Label() const override{ return "変形: " + after_.asset; }
private:
    static void Apply(CraftStage& stage, const CraftObject& value);
    CraftObject before_, after_;
};

// ゲームオブジェクトなどの params（自由な設定）の変更：変更前後の値を覚える
class CraftParamsCommand : public CraftCommand {
public:
    CraftParamsCommand(uint64_t id, nlohmann::json before, nlohmann::json after, std::string asset)
        : id_(id), before_(std::move(before)), after_(std::move(after)), asset_(std::move(asset)) {}
    void Execute(CraftStage& stage) override{ Apply(stage, after_); }
    void Undo(CraftStage& stage) override{ Apply(stage, before_); }
    std::string Label() const override{ return "設定: " + asset_; }
private:
    void Apply(CraftStage& stage, const nlohmann::json& value){
        if ( CraftObject* object = stage.FindMutable(id_) ) { object->params = value; stage.Touch(); }
    }
    uint64_t id_;
    nlohmann::json before_, after_;
    std::string asset_;
};

// まとめ：子コマンドの列。取り消しは逆順（ブラシ1回・複数選択の削除などを1回の「元に戻す」にする）
class CraftGroupCommand : public CraftCommand {
public:
    explicit CraftGroupCommand(std::string label) : label_(std::move(label)) {}
    void Add(std::unique_ptr<CraftCommand> command){ children_.push_back(std::move(command)); }
    bool Empty() const{ return children_.empty(); }
    void Execute(CraftStage& stage) override{ for ( auto& child : children_ ) { child->Execute(stage); } }
    void Undo(CraftStage& stage) override{
        for ( auto it = children_.rbegin(); it != children_.rend(); ++it ) { ( *it )->Undo(stage); }
    }
    std::string Label() const override{ return label_ + "（" + std::to_string(children_.size()) + "件）"; }
private:
    std::string label_;
    std::vector<std::unique_ptr<CraftCommand>> children_;
};

class CraftHistory {
public:
    static constexpr int kMaxEntries = 200;

    // 実行して積む
    void Execute(CraftStage& stage, std::unique_ptr<CraftCommand> command);
    // 実行済みの操作を積む（ギズモのドラッグ・ブラシなど、操作中にもう反映している物）
    void PushDone(std::unique_ptr<CraftCommand> command);

    bool Undo(CraftStage& stage);
    bool Redo(CraftStage& stage);
    bool CanUndo() const{ return cursor_ > 0; }
    bool CanRedo() const{ return cursor_ < ( int ) entries_.size(); }

    void Clear();               // ステージを読み込んだら空にする
    void MarkSaved(){ savedCursor_ = cursor_; }
    bool IsDirty() const{ return savedCursor_ != cursor_; }

    // 履歴タブ用
    int  GetCursor() const{ return cursor_; }
    int  GetSavedCursor() const{ return savedCursor_; }
    const std::vector<std::unique_ptr<CraftCommand>>& GetEntries() const{ return entries_; }

private:
    std::vector<std::unique_ptr<CraftCommand>> entries_;
    int cursor_ = 0;      // ここより左が実行済み
    int savedCursor_ = 0; // 保存した時のカーソル（-1＝捨てられた＝常に未保存）
};
