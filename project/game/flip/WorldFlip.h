#pragma once
// =====================================================================
//  WorldFlip：裏世界反転レール（企画書「裏世界反転レールシステム」）の試作。
//   ・レールに「両方 / 表 / 裏」の所属を持たせる（resources/map/<ステージ>_flip.json）。
//     今いない面のレールは乗れない・着地しない・道が沈んで消える（SplineRail::sideBlocked / sideAnim）
//   ・「？」スイッチ（紫の箱。表裏どちらからも見える）に卵を当てる
//     → くるっと回ってヒットストップ → カメラがプレイヤーのまわりを弧を描いて180°回り込む
//     → 回る間に、恐竜が相手の面の乗り換え点へ飛び移り、相手の面の道が生えてくる → クールダウン（灰色）
//     反転中はゲームの時間を止める（入力・敵・当たり判定も止まる）
//   ・裏にいる間に「両方」のレールへ着いたら（合流）、乗り換えなしで表へ戻す
//   ・裏ルートはカメラ側（表の手前、z がマイナスの方）に置く。表のカメラからは背中側なので見えない。
//     裏の間は、手前（z が backHideZ より小さい）の草・地面を消して、洞窟の床・奥の岩肌・岩に置き換え、光を暗くして恐竜のまわりだけ灯す
//   ・確かめ用：Game View にスイッチと乗り換え点の名札・矢印、裏のレールを青い線で（環境変数 CG2_DEBUG_KEYS で F8＝近くのスイッチで反転）
//   ・インスペクター「裏世界（反転）」：所属・スイッチ・乗り換え点・時間を直して保存
//  入力はもともとカメラの向き基準（PlayerInput）なので、裏でも「D＝画面の右」のまま
// =====================================================================
#include "engine/3d/obj/Obj3dCommon.h"
#include "engine/math/struct.h"

#include <memory>
#include <string>
#include <vector>

class Camera;
class EggSystem;
class Obj3d;
class Player;
class PlayCameraController;
class RailField;
class StageBackdrop;
struct SceneEditContext;

class WorldFlip {
public:
    enum class Side { Both = 0, Front = 1, Back = 2 };

    // シーンの部品（持たない）
    struct Context {
        Player*               player = nullptr;
        RailField*            railField = nullptr;
        PlayCameraController* camCtrl = nullptr;
        StageBackdrop*        backdrop = nullptr;
        EggSystem*            eggs = nullptr;
        Camera*               camera = nullptr;
    };

    WorldFlip();
    ~WorldFlip();

    void Initialize(const Context& context, const std::string& stage);
    void Finalize(); // 光を元に戻す

    void OnPlayStart(){ ResetToFront(); }
    void OnEditStart(){ ResetToFront(); }
    void ResetToFront(); // 表へ戻す（プレイ開始・落ちた・やりなおし）

    // プレイ中の進行のはじめ。反転中（ゲームの時間を止めている）なら true → シーンはそのフレームのゲームを進めない
    //   debugCameraActive：デバッグカメラを使っている間はカメラを動かさない
    bool Update(bool debugCameraActive);
    // 飛んでいる卵と「？」スイッチの当たり（卵を投げた後・卵の更新の前に呼ぶ）
    void CheckEggHits();
    // 見た目（毎フレーム、モードに関わらず）：スイッチ・レールの出方・背景・光
    void UpdateVisuals(bool playing);
    void Draw();

    bool IsFlipping() const{ return phase_ != Phase::None; }
    Side CurrentSide() const{ return side_; }
    Vector3 PlayerVisualOffset() const{ return visualOffset_; } // 恐竜の見た目のずれ（飛び移る途中）

    void DrawInspector(bool playing);
    // Game View に名札（スイッチ・乗り換え点・裏のレール）を出す（エディタの ImGui の中で呼ぶ）
    void DrawGameViewLabels(const SceneEditContext& context);

private:
    struct TransferPoint {
        int     rail = -1;
        Vector3 pos {};   // この近くのレールの位置へ移る
    };
    struct Switch {
        Vector3 pos {};             // 箱の底の真ん中
        TransferPoint front, back;  // 表側・裏側の乗り換え点
        float hitRadius = 1.1f;     // 卵の当たり半径（見た目より少し大きく）
        float spin = 0.0f;          // くるっと回る（当たった時）
        std::unique_ptr<Obj3d> obj;
    };
    enum class Phase { None, Hit, Turn };

    bool Load();
    bool Save() const;
    Side RailSide(int rail) const;
    void BeginFlip(int switchIndex);          // switchIndex<0＝乗り換えなし（合流で表へ戻る）
    void StartTurn();
    void FinishTurn();
    void ApplyRails(bool playing);
    void ApplyBackdrop(bool playing);
    void ApplyLight();
    void MakeSwitchObject(Switch& sw);

    Context context_;
    std::string stage_;
    std::vector<Switch> switches_;
    std::vector<int>    railSides_;           // レール番号 → 所属（無い所は両方）

    // 時間（実時間の秒）
    float hitStop_ = 0.25f;
    float flipTime_ = 1.0f;
    float cooldownTime_ = 1.5f;
    float radiusGrow_ = 0.35f;  // 回る途中に距離を何割広げるか（両方の世界が一瞬見える）
    float hopHeight_ = 2.0f;    // 飛び移る時の山の高さ(m)
    float backHideZ_ = -3.0f;   // 背景：これより手前（カメラ側）の物は、裏の間は消す（洞窟の床に置き換える）
    Vector3 backLightColor_ { 0.55f, 0.5f, 0.85f };
    float backLightScale_ = 0.45f;

    Side  side_ = Side::Front;  // 今の面（乗り換えた時点で相手の面になる）
    Phase phase_ = Phase::None;
    float time_ = 0.0f;
    float cooldown_ = 0.0f;
    int   activeSwitch_ = -1;
    bool  toBack_ = true;        // この反転の向き
    float progress_ = 0.0f;      // 0→1（回る途中）。表=0 / 裏=1 として見た目を混ぜる量は look_
    float look_ = 0.0f;          // 見た目の裏らしさ（0=表 → 1=裏）
    Vector3 jumpFrom_ {};        // 飛び移る前の恐竜の位置 − 移った先（見た目のずれの元）
    Vector3 visualOffset_ {};

    // 洞窟（裏の間だけ描く。表のカメラからは背中側なので見えない）
    std::unique_ptr<Obj3d> caveWall_;  // 奥の岩肌
    std::unique_ptr<Obj3d> caveFloor_; // 床
    std::vector<std::unique_ptr<Obj3d>> rocks_;
    void BuildCave();

    // 光（シーンに入った時の値を覚えて、裏の暗さと混ぜる。終わったら戻す）
    bool lightSaved_ = false;
    Obj3dCommon::DirectionalLightData savedLight_ {};
    Obj3dCommon::PointLight savedPoint_ {};
    Vector3 lanternColor_ { 1.0f, 0.75f, 0.45f }; // 恐竜のまわりの明かり
    float lanternIntensity_ = 1.6f;

    // エディタ
    int  selected_ = 0;
    bool showDebug_ = true;      // 当たり・乗り換え点・裏のレールを線で
    bool showInPlay_ = false;    // プレイ中も出す
};
