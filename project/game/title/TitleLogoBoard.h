#pragma once
// =====================================================================
//  TitleLogoBoard：タイトルのロゴ看板。
//   恐竜に投げられて山なりに飛び、地面に刺さってぐらぐら揺れてから止まる。
//   絵は resources/title/logo.png（TitleAssets が看板の面に貼る）
// =====================================================================
#include "engine/math/struct.h"
#include "game/title/TitleLayout.h"

#include <memory>

class Obj3d;

class TitleLogoBoard {
public:
    TitleLogoBoard();
    ~TitleLogoBoard();

    void Initialize();
    void Finalize();
    void Update(float deltaTime);
    void Draw();

    void Reset();                     // 投げられる前（見えない状態）へ戻す
    void Launch(const Vector3& from); // from から投げられて、刺さる位置へ飛ぶ
    void PlaceLanded();               // 演出を飛ばして、刺さった状態にする
    bool HasLanded() const{ return state_ == State::Landed; }
    bool ConsumeLanded();             // 刺さった瞬間に1回だけ true

    Vector3& RefLandPosition(){ return landPosition_; } // 刺さる位置（調整ウィンドウ用）

private:
    enum class State { Hidden, Flying, Landed };

    std::unique_ptr<Obj3d> board_;
    State   state_ = State::Hidden;
    Vector3 landPosition_ = TitleLayout::kLogoLand; // 杭の先はここから地面に刺さる
    Vector3 launchPosition_ {};
    float   time_ = 0.0f;          // 飛び始めてから／刺さってからの秒数
    bool    landedPending_ = false;
};
