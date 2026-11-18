#pragma once
#include "engine/math/struct.h"
#include "game/clear/ClearSettings.h"
#include "game/clear/ResultScreen.h"

#include <string>

namespace KazumimiEngine { class Camera; }
class ClearCamera;
class EggSystem;
class GoalProps;
class GoalGate;
class PaperFx;
class Player;
class PlayerAvatar;
class RailField;

// =====================================================================
//  ClearContext：クリア演出の場面（ClearSequence）が読み書きする物をまとめた型。
//   ・シーンの部品（恐竜・カメラ・卵のエフェクト）… 持たない。シーンの方が長生き
//   ・クリア演出の部品（門・演出カメラ・小道具・リザルト画面・紙吹雪）… ClearFlow が持つ
//   ・調整値と、門をくぐった時に決まる舞台（着地する所・くぐった向き・カメラの方）
//   ・このフレームの入力と、場面から ClearFlow への知らせ
//   場面どうしは互いを知らず、ここを通してだけやりとりする
// =====================================================================
struct ClearContext {
    // --- シーンの部品 ---
    Player*          player = nullptr;
    PlayerAvatar*    avatar = nullptr;
    const RailField* railField = nullptr;
    KazumimiEngine::Camera*          camera = nullptr;
    EggSystem*       eggSystem = nullptr;

    // --- クリア演出の部品 ---
    GoalGate*     gate = nullptr;
    ClearCamera*  clearCamera = nullptr;
    GoalProps*    props = nullptr;
    ResultScreen* result = nullptr;
    PaperFx*      paperFx = nullptr;

    // --- 調整値 ---
    const ClearSettings*      settings = nullptr;
    const ClearStageSettings* stage = nullptr;
    ClearTiming timing;              // 今回の場面の長さ（フル版か短縮版）

    // --- 舞台（門をくぐった時に決まる）---
    KazumimiEngine::Vector3 landPoint {};            // 恐竜が着地する所（地面）
    KazumimiEngine::Vector3 forward { 1.0f, 0.0f, 0.0f };  // くぐった向き（水平・長さ 1）
    KazumimiEngine::Vector3 toCamera { 1.0f, 0.0f, 0.0f }; // 着地する所から演出カメラの方（水平・長さ 1）
    float   facingSign = 1.0f;       // 門の表（文字を下げる面）が、くぐった向きと同じなら +1
    int     landRail = -1;           // 着地する所のレール
    float   landDistance = 0.0f;     // そのレールの上の距離
    ResultScreen::Data resultData;   // リザルト画面に出す中身
    std::string nextScene;           // 終わったら切り替える先（SceneFactory の名前）

    // --- 進み具合（場面が書く）---
    float propsTime = 0.0f;          // 小道具を出し始めてからの秒数
    float sinceLanding = -1.0f;      // 着地してからの秒数（負＝まだ空中）
    bool  dancing = false;           // 喜びの踊りを始めた
    bool  resultOpen = false;        // リザルト画面を出している（HUD・小道具の音は止める）
    bool  leaving = false;           // 次のシーンへの切り替えを頼んだ

    // --- このフレームの入力 ---
    bool decide = false;             // 決定ボタンを押した瞬間
};
