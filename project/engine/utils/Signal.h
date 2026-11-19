#pragma once
#include <functional>
#include <utility>
#include <vector>

namespace KazumimiEngine {

// =====================================================================
//  Signal<Args...>：出来事の知らせ（オブザーバー パターン）。クラステンプレート。
//   出す側は「何が起きたか」だけを Emit で知らせ、受ける側は Connect で受け取る。
//   出す側は、誰が受け取っているかを知らなくてよい
//   （例：コンボが伸びた・ダメージを受けた・敵を倒した → 集計係が数える。プレイの処理と演出の処理を直接つながない）。
//
//     Signal<int> onComboChanged;                                  // 出す側が持つ（引数の型がテンプレート引数）
//     auto id = onComboChanged.Connect([&](int combo){ ... });     // 受ける側が登録する
//     onComboChanged.Emit(combo);                                  // 起きた時に出す
//     onComboChanged.Disconnect(id);                               // 受ける側が先に無くなる時は、必ず外す
//
//   引数なしの知らせは Signal<> と書く
// =====================================================================
template <class... Args>
class Signal {
public:
    using Slot = std::function<void(Args...)>;
    using Connection = int; // Connect が返す番号（0 は「登録していない」）

    // 受け取る関数を登録する。外す時に使う番号を返す
    Connection Connect(Slot slot){
        slots_.push_back({ nextConnection_, std::move(slot) });
        return nextConnection_++;
    }
    // 登録を外す（もう外れている番号は何もしない）
    void Disconnect(Connection connection){
        std::erase_if(slots_, [connection](const Entry& entry){ return entry.connection == connection; });
    }
    void Clear(){ slots_.clear(); }
    bool IsEmpty() const{ return slots_.empty(); }

    // 登録されている全員へ知らせる
    void Emit(Args... args) const{
        // 受け取った関数の中で Connect / Disconnect されても壊れないよう、写しを回す
        const std::vector<Entry> current = slots_;
        for ( const Entry& entry : current ) {
            if ( entry.slot ) { entry.slot(args...); }
        }
    }

private:
    // 登録してある受け取り手 1 つ（外す時の番号と、呼ぶ関数）
    struct Entry {
        Connection connection;
        Slot slot;
    };
    std::vector<Entry> slots_;
    Connection nextConnection_ = 1;
};

} // namespace KazumimiEngine
