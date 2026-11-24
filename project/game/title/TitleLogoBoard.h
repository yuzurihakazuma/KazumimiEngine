#pragma once
#include "engine/math/struct.h"
#include "game/title/TitleLayout.h"
#include "engine/utils/Signal.h"

#include <memory>

namespace KazumimiEngine { class Obj3d; }

// =====================================================================
//  TitleLogoBoard：タイトルのロゴ看板。
//   恐竜に投げられて山なりに飛び、地面に刺さってぐらぐら揺れてから止まる。
//   絵は resources/title/logo.png（TitleAssets が看板の面に貼る）
// =====================================================================
class TitleLogoBoard {
public:
    TitleLogoBoard();
    ~TitleLogoBoard();

    void Initialize();
    void Finalize();
    void Update(float deltaTime);
    void Draw();

    void Reset();                     // 投げられる前（見えない状態）へ戻す
    void Launch(const KazumimiEngine::Vector3& from); // from から投げられて、刺さる位置へ飛ぶ
    void PlaceLanded();               // 演出を飛ばして、刺さった状態にする
    bool HasLanded() const{ return state_ == State::Landed; }
    KazumimiEngine::Signal<>& OnLanded(){ return onLanded_; } // 飛んで来て刺さった瞬間の知らせ（演出を飛ばした時は出ない）
    // 倒れている角度と向き（紙芝居遷移の退場。向きは真後ろ=0、右奥が正）
    void SetFall(float angle, float direction){ fallAngle_ = angle; fallDirection_ = direction; }

    // 刺さる位置（書き換えは調整ウィンドウ用）
    const KazumimiEngine::Vector3& LandPosition() const{ return landPosition_; }
    void SetLandPosition(const KazumimiEngine::Vector3& position){ landPosition_ = position; }

private:
    enum class State { Hidden, Flying, Landed };

    std::unique_ptr<KazumimiEngine::Obj3d> board_;
    State   state_ = State::Hidden;
    KazumimiEngine::Vector3 landPosition_ = TitleLayout::kLogoLand; // 杭の先はここから地面に刺さる
    KazumimiEngine::Vector3 launchPosition_ {};
    float   time_ = 0.0f;          // 飛び始めてから／刺さってからの秒数
    KazumimiEngine::Signal<> onLanded_; // 刺さった瞬間
    float   fallAngle_ = 0.0f;
    float   fallDirection_ = 0.0f;
};
