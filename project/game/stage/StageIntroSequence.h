#pragma once
// =====================================================================
//  StageIntroSequence：ステージ開始時のプレイヤーの演技（ステージ開始演出 実装企画書（プレイヤーの演技））。
//   ステージ開始カードが上へ抜けたあと、たまごザウルスが
//   「こっちを見て手を振る → ロールを押す → ジャンプでこっちを向く → キラキラ → 着地」と演技してから操作開始にする。
//   時刻は遷移の定義（resources/transition/stage_intro.json）の sceneCue が合図を送って決める（コードに秒数は持たない）：
//     introStart  … カードの裏でステージを読み終えた：イントロカメラへ。ゲームの時間（敵など）を止め、ロールを前に置く
//     introTurn   … カメラの方を向いて、手を振る（IntroWave）
//     introPush   … 進む方向を向いて、ロールを押す（IntroPush）。ロールが少しずつ前へ
//     introRollGo … ロールが転がり出す（加速して画面の外へ）。ロールの足元に土ぼこり
//     introAction … その場でジャンプ（IntroJump。高さはクリップが出す）。跳んだらカメラを向き、着地で土ぼこり
//     introSparkle… 頭の上にキラキラ
//     introReturn … カメラが後ろへ戻る。着地したら進む方向へ向き直る
//     introEnd    … 操作開始
//   イントロカメラとロールの置き場所はステージごと（resources/stage_intro/<stage>.json）
// =====================================================================
#include "engine/math/struct.h"

#include <memory>
#include <string>

class Camera;
class EggSystem;
class Obj3d;
class Player;
class PlayerAvatar;
class PlayCameraController;
class RailField;

class StageIntroSequence {
public:
    // 演技に使う、シーンの部品
    struct Context {
        Player*               player = nullptr;
        PlayerAvatar*         avatar = nullptr;     // 演技のクリップ・頭の位置
        const RailField*      railField = nullptr;  // スタートの位置と向き
        PlayCameraController* camCtrl = nullptr;    // 通常カメラの位置（戻り先）
        EggSystem*            eggSystem = nullptr;  // 土ぼこり（小さな粒）
    };

    StageIntroSequence();
    ~StageIntroSequence();

    void Initialize();                        // ロールの見た目を作る（モデルは読み込み済みであること）
    void LoadSettings(const std::string& stage);
    void Reset(const Context& context);       // 演技をやめて、プレイヤーを元に戻す（エディタへ切り替えた時など）

    void OnCue(const std::string& cue, const Context& context);

    // 演技の最中か（最中はゲームの時間を止め、プレイヤーの入力の代わりをする）
    bool IsActive() const{ return state_ != State::None; }
    // HUD を隠すか（カメラが戻り始めたら出す）
    bool IsHudHidden() const{ return state_ != State::None && state_ != State::Returning; }

    // プレイヤーの更新の前：入力の代わり（何も押さない）と、演技の途中の向き・土ぼこり
    void UpdateInput(const Context& context, float dt);
    // プレイヤーの更新の後：カメラ
    void UpdateCamera(Camera* camera, const Context& context, float dt);
    // ロール（操作開始の後も、画面の外へ出るまで転がり続ける）
    void UpdateRoll(float dt);
    void Draw();

private:
    enum class State { None, Waiting, Wave, Push, Action, Returning };
    enum class Roll { Hidden, Placed, Pushed, Rolling };

    void PlaceCamera(const Vector3& playerPosition, PlayCameraController& camCtrl);
    void SpawnDust(EggSystem* eggSystem, const Vector3& at, float spread) const;
    float YawToCamera(const Vector3& position) const;

    State   state_ = State::None;
    float   forwardYaw_ = 0.0f;     // 進む向き（スタートのレールの向き）
    Vector3 introCamera_ {};        // イントロカメラの位置
    bool    cameraPlaced_ = false;  // イントロカメラの位置を決めたか（プレイヤーがスタートに立ってから決める）
    float   actionTime_ = 0.0f;     // ジャンプを始めてからの秒数
    bool    tookOff_ = false;       // ジャンプで跳び上がったか
    bool    landed_ = false;        // ジャンプから着地したか
    bool    faceForwardOnLand_ = false; // 着地したら進む方向へ向き直る（カメラが戻り始めた時に、まだ空中だった）
    float   returnTime_ = 0.0f;     // カメラが戻り始めてからの秒数
    float   headBase_ = -1.0f;      // 立っている時の頭の高さ（足元から。負＝まだ測っていない）
    float   lookLift_ = 0.0f;       // ジャンプで頭が上がった分だけ、カメラの見る所も上げる（なめらかに）

    // ロール（巻いた布）
    std::unique_ptr<Obj3d> roll_;
    Roll    rollState_ = Roll::Hidden;
    Vector3 rollOrigin_ {};         // 置いた位置（道の上面の高さ）
    float   rollDistance_ = 0.0f;   // 転がった距離
    float   rollSpeed_ = 0.0f;
    float   rollAngle_ = 0.0f;      // 回った角度（転がった距離 ÷ 半径を足していく）

    // ステージごとの設定
    float distance_ = 3.8f, side_ = 1.3f, height_ = 0.75f, lookHeight_ = 1.0f; // イントロカメラ
    float rollAhead_ = 1.25f;   // ロールを置く所（プレイヤーから進む方向へ）
    float rollRadius_ = 0.45f;  // ロールの太さ（半径）
};
