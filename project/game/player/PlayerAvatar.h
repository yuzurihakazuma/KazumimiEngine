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
#include <string>

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

    // ステージ開始演出のクリップ（IntroWave / IntroPush / IntroJump）。指定している間は、状態（空中・歩き・待機）から
    //   選ぶより先にこれを1回だけ再生し、最後の形で止まる。同じクリップでも頭から流し直す
    void PlayIntroClip(const std::string& clip);
    void ClearIntroClip();
    const std::string& GetIntroClip() const{ return introClip_; }
    // ボーンの世界での位置（キラキラを頭の上に出すなど）
    bool GetJointWorldPosition(const std::string& joint, Vector3& out) const;
    // 上から落ちてくる演出（ワールドマップから飛び込んだ時）：見た目だけ持ち上げ・着地でつぶす。
    //   lift＝足元からの高さ(m)、squash＝縦の縮み（1=そのまま。横はその分ふくらむ）
    void SetDropPose(float lift, float squash){ dropLift_ = lift; dropSquash_ = squash; }
    // やられポーズ：pitch＝あおむけに倒れる角度・roll＝左右にふらつく角度（rad。0＝立っている）、
    //   faceYaw を使う時（hasYaw）は、その向きを向く（こっちを向く）
    void SetDefeatRotation(float pitch, float roll, bool hasYaw, float faceYaw){
        defeatTilt_ = pitch; defeatRoll_ = roll; defeatHasYaw_ = hasYaw; defeatYaw_ = faceYaw;
    }

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
    std::string introClip_;             // ステージ開始演出のクリップ（空＝指定なし）
    bool    introRestart_ = false;      // 次の更新で、演出のクリップを頭から流す
    float   dropLift_ = 0.0f;           // 上から落ちてくる演出の高さ
    float   dropSquash_ = 1.0f;         // 着地のつぶれ
    float   defeatTilt_ = 0.0f;         // やられポーズの傾き（あおむけ）
    float   defeatRoll_ = 0.0f;         // やられポーズの左右のふらつき
    bool    defeatHasYaw_ = false;      // やられポーズの向きを使う
    float   defeatYaw_ = 0.0f;
};
