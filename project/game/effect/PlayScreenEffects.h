#pragma once
// =====================================================================
//  PlayScreenEffects：プレイ中の画面全体のポストエフェクトの切り替え。
//   ・プレイ中だけの「クラフト世界観」の見た目（ジオラマ風・紙の質感・絵本風）
//   ・落下ミス→リスポーン時のアイリスワイプ（円が閉じて→開く。閉じている間は色も抜く）
//   ・起動直後のウォームアップ（初めて使う演出の遅延構築を先に済ませ、初撃破のフリーズを無くす）
//  どれも PostEffect のON/OFFと値の設定だけ。ヒット時の歪み・グローは HitFeel が持つ
// =====================================================================
#include "engine/math/struct.h"

class Camera;
class CombatSystem;

class PlayScreenEffects {
public:
    // Edit→Play：プレイ用の見た目をONにする（値はエディタで調整して決めた本番ルック）
    void EnablePlayLook();
    // Play→Edit：プレイ用の見た目とアイリスを全部OFFにする（編集画面はそのまま見えるように）
    void DisablePlayLook();

    // 落下ミスの瞬間に呼ぶ：アイリスを閉じるところから始める
    void StartMissIris();
    // アイリスの進行。focus は円の中心にするワールド位置（プレイヤーの胸元）
    void UpdateIris(const Vector3& focus, const Camera& camera);

    // 起動直後の数フレームだけ、初回使用の遅い演出を画面に影響しない形で一度通す。毎フレーム呼ぶ
    //   SDF溶け演出は初めて描画された瞬間にシェーダー/パイプラインの遅延構築で1秒以上固まり、
    //   ヒット時ポストエフェクト（歪み+グロー）の初回パスも約0.2秒かかる（フレーム計測で実測）。
    //   playing=true なら、終わった時にプレイ用の見た目を戻す（リリース版は最初から Play で始まるため）
    void UpdateWarmup(CombatSystem& combat, bool playing);

private:
    enum class IrisPhase { None, Closing, Hold, Opening };
    IrisPhase irisPhase_ = IrisPhase::None;
    float irisTimer_ = 0.0f;

    int warmupFrame_ = 0;
};
