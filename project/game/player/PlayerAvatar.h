#pragma once
// =====================================================================
//  PlayerAvatar：プレイヤーの見た目（リグ付きマスコット resources/player/player.gltf）。
//   ・Play中はプレイヤーに追従、Edit中はスタート地点に立たせてプレビューする
//   ・状況でクリップを切り替える（ベロ > 卵の構え > 投げた直後 > 産卵 > 空中 > 歩き > 待機）
//   ・卵の構え中は手（Itemジョイント）に卵を持たせる
//   ・敵にぶつかった後の無敵中は点滅させる
//  当たり判定や移動は Player が持つ。ここは「どう見せるか」だけ
// =====================================================================
#include "engine/math/struct.h"

#include <cstdint>
#include <memory>

class SkinnedObj3d;
class Obj3d;
class Player;
class RailField;
class SwallowAbility;
class AimThrowController;
class EggSystem;

class PlayerAvatar {
public:
    PlayerAvatar();
    ~PlayerAvatar();

    // モデルの生成（カメラ確定後に呼ぶ。envMapSrv は環境マップの SRV）
    void Initialize(uint32_t envMapSrv);

    // 毎フレームの見た目の更新
    struct Context {
        bool                playing = false;    // Play中か（false=スタート地点のプレビュー）
        const Player*       player = nullptr;
        const RailField*    railField = nullptr;
        const SwallowAbility* swallow = nullptr;
        AimThrowController* aimThrow = nullptr; // 構えのキャンセル通知を取り出すので非const
        const EggSystem*    eggSystem = nullptr;
    };
    void Update(const Context& context);

    // playing=true かつ無敵中なら点滅させる
    void Draw(bool playing, const Player* player);

    // 足元と道の上面を合わせる補正（デバッグUIのスライダー用）
    float* ModelYOffsetPtr() { return &modelYOffset_; }

private:
    // クリップ選択。speedRatio は実際に動いた速さ÷通常の歩く速さ
    void SelectClip(const Context& context, float speedRatio);
    void ResetTongueAndHeldEgg();

    std::unique_ptr<SkinnedObj3d> model_;
    std::unique_ptr<Obj3d>        heldEgg_;    // 構え中に Item ジョイントへ持たせる卵の見た目
    bool    heldEggVisible_ = false;

    Vector3 prevPos_ {};                // 前フレームの見た目位置（移動速度の算出用）
    bool    prevPosValid_ = false;
    float   modelYOffset_ = 0.0f;       // モデル原点（足元）を道の上面に乗せる補正（道の上面＝レール線に統一したので既定0）
    bool    tongueSeeked_ = false;      // 捕獲時の収納パートへのシークを1回だけ行うためのフラグ
    float   throwTimer_ = 0.0f;         // 投げリリース後の復帰モーション再生の残り時間
    int     blinkFrame_ = 0;            // 無敵中の点滅用
};
