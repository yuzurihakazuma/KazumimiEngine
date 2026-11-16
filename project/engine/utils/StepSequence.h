#pragma once
// =====================================================================
//  StepSequence<TContext>：順番に進む「場面」の列（ステート パターン）。クラステンプレート。
//   演出を「時間で進む場面の列」として作るための進行役。場面ごとに
//     Enter（始まる時にやること）・Update（毎フレームやること。true を返したら終わり）・Exit（終わる時にやること）
//   を持たせ、列が順番に回す。場面どうしは互いを知らない（次へ進むのは列の仕事）。
//
//   TContext＝場面が読み書きする物をまとめた型（シーンの部品へのポインタなど）。列ごとに自由に決める：
//     struct ClearContext { Player* player; Camera* camera; ... };
//     StepSequence<ClearContext> sequence;
//     sequence.Add<RingStep>("輪をくぐる", 2.0f);              // IStep を継承したクラスを、引数つきで作って足す
//     sequence.AddTimed("待つ", 0.5f);                          // 何もしないで待つだけの場面
//     sequence.AddCall("音を鳴らす", [](ClearContext& c){ ... }); // その場で 1 回呼ぶだけの場面
//     sequence.Start(context);
//     sequence.Update(context, deltaTime);                     // 毎フレーム
//
//   早送り：Skip で今の場面を終わらせて次へ進む（場面の側は Skip で「終わりの形」にする）
// =====================================================================
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

template <class TContext>
class StepSequence {
public:
    // 場面 1 つ。これを継承して中身を書く
    class IStep {
    public:
        virtual ~IStep() = default;
        virtual void Enter(TContext& /*context*/) {}
        // 毎フレーム。終わったら true
        virtual bool Update(TContext& context, float deltaTime) = 0;
        virtual void Exit(TContext& /*context*/) {}
        // 早送りされた：すぐ終わりの形にする（この後 Exit が呼ばれる）
        virtual void Skip(TContext& /*context*/) {}

        // この場面が始まってからの秒数（列が進める）
        float Elapsed() const{ return elapsed_; }

    private:
        friend class StepSequence;
        float elapsed_ = 0.0f;
    };

    // 決まった長さの場面：Elapsed が duration に届いたら終わる。中身は OnUpdate に書く
    class TimedStep : public IStep {
    public:
        explicit TimedStep(float duration) : duration_(duration) {}
        bool Update(TContext& context, float deltaTime) override{
            OnUpdate(context, deltaTime);
            return this->Elapsed() >= duration_;
        }
        float Duration() const{ return duration_; }
        // 進み具合 0〜1
        float Progress() const{
            if ( duration_ <= 0.0f ) { return 1.0f; }
            const float progress = this->Elapsed() / duration_;
            return progress < 0.0f ? 0.0f : ( progress > 1.0f ? 1.0f : progress );
        }

    protected:
        virtual void OnUpdate(TContext& /*context*/, float /*deltaTime*/) {}

    private:
        float duration_ = 0.0f;
    };

    // 場面を作って列の後ろに足す（関数テンプレート。TStep のコンストラクタの引数をそのまま渡す）
    template <class TStep, class... TArgs>
    TStep& Add(std::string name, TArgs&&... args){
        auto step = std::make_unique<TStep>(std::forward<TArgs>(args)...);
        TStep& reference = *step;
        entries_.push_back({ std::move(name), std::move(step) });
        return reference;
    }
    // 何もしないで待つだけの場面
    void AddTimed(std::string name, float duration){ Add<TimedStep>(std::move(name), duration); }
    // その場で 1 回呼ぶだけの場面（すぐ次へ進む）
    void AddCall(std::string name, std::function<void(TContext&)> call){
        Add<CallStep>(std::move(name), std::move(call));
    }
    // 関数で書く場面：update が true を返すまで続く
    void AddUntil(std::string name, std::function<bool(TContext&, float elapsed, float deltaTime)> update){
        Add<UntilStep>(std::move(name), std::move(update));
    }
    void Clear(){ entries_.clear(); current_ = kNotStarted; }

    // 最初の場面から始める
    void Start(TContext& context){
        current_ = 0;
        EnterCurrent(context);
    }
    // 途中でやめる（今の場面の Exit を呼ぶ）
    void Abort(TContext& context){
        if ( IsRunning() ) { entries_[current_].step->Exit(context); }
        current_ = kNotStarted;
    }
    // 毎フレーム。1 フレームのうちに終わった場面が続く時は、まとめて進める（長さ 0 の場面で 1 フレーム待たない）
    void Update(TContext& context, float deltaTime){
        float remaining = deltaTime;
        while ( IsRunning() ) {
            IStep& step = *entries_[current_].step;
            step.elapsed_ += remaining;
            if ( !step.Update(context, remaining) ) { return; }
            remaining = 0.0f; // 次の場面には、このフレームの時間を渡さない
            Advance(context);
        }
    }
    // 早送り：今の場面を終わらせて、次の場面へ
    void Skip(TContext& context){
        if ( !IsRunning() ) { return; }
        entries_[current_].step->Skip(context);
        Advance(context);
    }

    bool IsRunning() const{ return current_ >= 0 && current_ < static_cast<int>( entries_.size() ); }
    bool IsFinished() const{ return current_ >= static_cast<int>( entries_.size() ) && !entries_.empty(); }
    int  CurrentIndex() const{ return current_; }
    int  StepCount() const{ return static_cast<int>( entries_.size() ); }
    // 今の場面の名前（調整用の表示に使う。動いていなければ空）
    const std::string& CurrentName() const{
        static const std::string kNone;
        return IsRunning() ? entries_[current_].name : kNone;
    }
    float CurrentElapsed() const{ return IsRunning() ? entries_[current_].step->Elapsed() : 0.0f; }

private:
    static constexpr int kNotStarted = -1;

    class CallStep : public IStep {
    public:
        explicit CallStep(std::function<void(TContext&)> call) : call_(std::move(call)) {}
        void Enter(TContext& context) override{ if ( call_ ) { call_(context); } }
        bool Update(TContext& /*context*/, float /*deltaTime*/) override{ return true; }
    private:
        std::function<void(TContext&)> call_;
    };
    class UntilStep : public IStep {
    public:
        explicit UntilStep(std::function<bool(TContext&, float, float)> update) : update_(std::move(update)) {}
        bool Update(TContext& context, float deltaTime) override{
            return !update_ || update_(context, this->Elapsed(), deltaTime);
        }
    private:
        std::function<bool(TContext&, float, float)> update_;
    };

    void EnterCurrent(TContext& context){
        if ( !IsRunning() ) { return; }
        entries_[current_].step->elapsed_ = 0.0f;
        entries_[current_].step->Enter(context);
    }
    void Advance(TContext& context){
        entries_[current_].step->Exit(context);
        ++current_;
        EnterCurrent(context);
    }

    struct Entry {
        std::string name;
        std::unique_ptr<IStep> step;
    };
    std::vector<Entry> entries_;
    int current_ = kNotStarted;
};
