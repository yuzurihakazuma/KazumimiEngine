#pragma once

namespace KazumimiEngine {

// =====================================================================
//  Singleton<T>：プログラムに 1 つだけの物（シングルトン）の、共通の土台。クラステンプレート（CRTP）。
//   前は、どのマネージャも同じ 3 つを自分で書いていた：
//     static X* GetInstance(){ static X instance; return &instance; }   // 取り出す
//     X(const X&) = delete;  X& operator=(const X&) = delete;           // 写しを作らせない
//   これをここに 1 回だけ書き、使う側は継承するだけにする：
//
//     class Input : public Singleton<Input> {
//         friend class Singleton<Input>;   // 土台だけが作れるようにする（コンストラクタは private のまま）
//     private:
//         Input() = default;
//         ...
//     };
//     Input::GetInstance()->Update();      // 使い方は今までと同じ
//
//   実体は、最初に GetInstance を呼んだ時に作られ、プログラムの終わりに壊される（今までと同じ）。
//   GPU の物を持つマネージャは、デバイスより先に放すため、今までどおり自分の Finalize で片付ける
// =====================================================================

template <class T>
class Singleton {
public:
    // ただ 1 つの実体を取り出す
    static T* GetInstance(){
        static T instance;
        return &instance;
    }

    // 写しは作れない
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

protected:
    Singleton() = default;
    ~Singleton() = default;
};

} // namespace KazumimiEngine
