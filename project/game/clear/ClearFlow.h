#pragma once
#include "engine/math/struct.h"
#include "game/clear/ClearBackdrop.h"
#include "game/clear/ClearCamera.h"
#include "game/clear/ClearContext.h"
#include "game/clear/ClearSequence.h"
#include "game/clear/ClearSettings.h"
#include "game/clear/GoalProps.h"
#include "game/clear/GoalGate.h"
#include "game/clear/ResultScreen.h"
#include "game/clear/StageTally.h"
#include "game/effect/PaperFx.h"

#include <functional>
#include <string>

namespace KazumimiEngine { class Camera; }
class EggCombo;
class EggSystem;
class GameOverFlow;
class Player;
class PlayerAvatar;
class RailField;

// =====================================================================
//  ClearFlow：クリア（ゴール）の流れを 1 つにまとめた物（クリア演出 企画書）。ゲームシーンはこれを決まった所で呼ぶだけ。
//   ・ゴールの門（GoalGate）を置き、くぐったことを判定する
//   ・くぐったら：操作と敵を止め（シーンがゲームの時間を進めない）、集計（StageTally）を確定して記録（StageRecords）へ足し、
//     場面の列（ClearSequence）を始める：門をくぐる → 「ゴール！」→ リザルト → 加算 → 幕 → ステージ選択へ
//   ・演出の間：恐竜は自動操縦と演技のクリップ、カメラは演出カメラ（ClearCamera）、小道具（GoalProps）・紙吹雪・リザルト画面
//   ・同じステージの 2 回目からは短縮版（「ゴール！」を縮める）
//   ・インスペクターの「クリア演出」：その場で試す・門の場所とカメラの終点を直して保存する
//  部品はそれぞれのクラスが持ち、ここは「いつ・どの順で呼ぶか」だけを決める：
//     StageTally（数える）/ GoalGate（門）/ ClearCamera（カメラ）/ GoalProps（文字と星）/ ResultScreen（リザルト）/ ClearSequence（場面の列）
//  演出の時間は、ゲームの時間（止まる）とは別の、止まらない時間で進める
// =====================================================================
class ClearFlow {
public:
    // シーンの部品（持たない。シーンの方が長生き）
    struct Context {
        Player*          player = nullptr;
        PlayerAvatar*    avatar = nullptr;
        const RailField* railField = nullptr;
        KazumimiEngine::Camera*          camera = nullptr;
        EggSystem*       eggSystem = nullptr;
        const EggCombo*  eggCombo = nullptr;        // 帯が「かくれ花」の時に、見つけた数を読む
        const GameOverFlow* health = nullptr;       // ♡の数（リザルトの「たいりょく」）
        std::function<void()> onGoal;               // くぐった瞬間の後始末（構え・ベロをやめる、♡を隠すなど。シーンが渡す）
    };

    ClearFlow();
    ~ClearFlow();

    // noiseSrv＝紙吹雪のモデルに使うテクスチャ
    void Initialize(const Context& context, const std::string& stage, uint32_t noiseSrv);
    void Finalize();

    // レール・マップを作り直した：輪を置き直す
    void OnRailsSynced();
    // プレイを始めた・やりなおした：数え直す。enemyTotal＝置いてある敵の数
    void OnPlayStart(int enemyTotal);
    // エディタへ戻った：演出をやめて、輪を閉じる
    void OnEditStart();

    // 集計係（シーンが、プレイの出来事の知らせをここへつなぐ）
    StageTally& Tally(){ return tally_; }

    // 演出中か（true の間、シーンはゲームの時間＝敵・動くレール・当たり判定を進めない）
    bool IsActive() const{ return active_; }
    const ClearSettings& Settings() const{ return settings_; } // clear.json の値（提出用・プレイ会用の切り替えも）
    // HUD を隠すか
    bool IsHudHidden() const{ return active_; }

    // ゲームの更新の後：輪をくぐったかを見る。くぐったら演出を始めて true
    bool CheckGoal();
    // 演出中、プレイヤーの更新の前：恐竜の入力の代わりと演技。deltaTime は止まらない時間
    void UpdateBeforePlayer(float deltaTime);
    // 演出中、プレイヤーの更新の後：場面を進める（カメラ・小道具・リザルト）
    void UpdateAfterPlayer(float deltaTime);
    // 毎フレーム（モードを問わず）：輪・紙吹雪の見た目
    void UpdateVisuals(float deltaTime);

    void DrawOpaque();                       // 輪
    void DrawInstanced(const KazumimiEngine::Camera* camera); // 紙吹雪
    void DrawTransparent();                  // 切り文字・星（透明な所のある板）
    // スプライト：リザルト画面を描いたら true（HUD の代わり）
    bool DrawUI();
    void DrawInspector(bool playing);

private:
    void Begin(float direction);   // 演出を始める。direction＝くぐった向き（レールの進む向きなら +1）
    void Abort();                  // 演出をやめて、恐竜を元に戻す
    void BuildStage(float direction); // 着地する所・カメラの方を決める
    void ApplyStageCamera();       // ステージごとの値から、演出カメラの終点を決め直す（インスペクターで直した値をその場で）
    ResultScreen::Data MakeResult(bool& outShort); // 集計を確定して、記録へ足す

    Context scene_;
    std::string stage_;

    // --- 部品 ---
    ClearSettings      settings_;
    ClearStageSettings stageSettings_;
    StageTally   tally_;
    GoalGate     gate_;
    ClearCamera  camera_;
    GoalProps    props_;
    ClearBackdrop backdrop_;       // クリアの間だけ、門の後ろに立てる書き割り
    ResultScreen result_;
    PaperFx      paperFx_;
    ClearSequence::Sequence sequence_;
    ClearContext context_;         // 場面が読み書きする物

    // --- 状態 ---
    bool    active_ = false;
    bool    hasPrevious_ = false;  // 前のフレームの足元を持っているか（くぐった判定）
    KazumimiEngine::Vector3 previousFoot_ {};
    bool    previewShort_ = false; // インスペクター：短縮版で試す
    KazumimiEngine::Vector3 cameraSide_ { 0.0f, 0.0f, -1.0f }; // 演出カメラを回す側（遊んでいる時のカメラがいた側。水平・長さ 1）
};
