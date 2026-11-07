#pragma once
#include "engine/rail/SplineRail.h"
#include "engine/math/struct.h"
#include "game/rail/RailMarkers.h"
#include "game/rail/RailMotion.h"
#include <vector>
#include <cstdint>

class Camera;
class EditorManager;

// =====================================================================
//  RailField：実行時のレール管理。
//   ・エディタ(LevelEditor)の最新データから実行用レール rails_ を作り直す
//     （接続・溶接・分岐は RailConnections）
//   ・緑線マーカー（穴区間は赤）で経路を可視化する（RailMarkers）
//   ・動くレールを時間で進める（RailMotion）
//  ゲーム側（プレイヤー・敵・描画・敵エディタ）は GetRails() でこのレールを参照する。
//  ※敵の生成は RailField の責務ではない（シーンが Sync 後に呼ぶ）。
// =====================================================================
class RailField {
public:
    // エディタ保持の最新レールから rails_ を作り直し、マーカーも再構築する。
    //   camera        : マーカーに割り当てるカメラ（描画用）
    //   whiteTexIndex : 単色化用の白テクスチャの SRV インデックス（0=未使用）
    void Sync(Camera* camera, uint32_t whiteTexIndex);

    // 動くレールを進める（プレイヤー更新より先に呼ぶ）。
    //   ridingRail: プレイヤーが今乗っているレール番号（「乗ったら動き出す」レールの発動判定。
    //   -1=誰も乗っていない / kMotionStartAll=全レール強制発動＝エディタのプレビュー用）
    static constexpr int kMotionStartAll = RailMotion::kStartAll;
    void UpdateMotion(float dt, int ridingRail = -1);
    // 編集モードへ戻った時：動くレールを基準位置へ戻す（発動状態・出現状態もリセット）
    void ResetMotion();
    // 動くレールのエディタプレビュー：Playを押さなくても動きを再生して組み方を確認できる。
    //   enabled が false に変わった瞬間に基準位置へ戻す（編集と表示がずれないように）。Edit中に毎フレーム呼ぶ
    void UpdateEditorPreview(bool enabled);
    // 「乗ったら動き出す」で待機中のリフトへ金色の「！」目印を描く（乗れば動くことが一目で分かる）
    void DrawWaitingLiftMarkers(float dt);

    void UpdateMarkers(){ markers_.UpdateMatrices(); } // マーカーの行列更新（毎フレーム。カメラ移動に追従）
    void DrawMarkers() const{ markers_.Draw(); }
    void RebuildMarkers(){ markers_.Build(rails_, camera_, whiteTexIndex_); } // マーカーだけ作り直す（デバッグUI用）

    // ゲーム側が参照する実行時レール
    const std::vector<SplineRail>& GetRails() const { return rails_; }

    int  Version() const { return lastVersion_; }       // 直近に同期したエディタの編集世代
    int  MarkerCount() const { return markers_.Count(); }
    bool ShowMarkers() const { return markers_.IsVisible(); }
    void SetShowMarkers(bool v) { markers_.SetVisible(v); }

    // --- スタート/ゴール地点（マップ設定から Sync 時に距離へ変換済み）---
    int   GetStartRail() const { return startRail_; }
    float GetStartDistance() const { return startDist_; }
    bool  HasGoal() const { return goalRail_ >= 0 && goalRail_ < ( int ) rails_.size(); }
    int   GetGoalRail() const { return goalRail_; }
    float GetGoalDistance() const { return goalDist_; }
    // ゴールの現在ワールド座標（動くレール上でも追従する）
    Vector3 GetGoalPos() const {
        if ( !HasGoal() ) return { 0.0f, 0.0f, 0.0f };
        return rails_[goalRail_].GetPositionByDistance(goalDist_);
    }

private:
    // --- Sync の段階（エディタの並列配列からレールごとの設定を写す）---
    void BuildRailsFromEditor(const EditorManager& editor);
    void ApplyMotionSettings(const EditorManager& editor); // 連結処理より先（HasMotion を判定に使う）
    void ApplyRailTypes(const EditorManager& editor);      // 連結処理より先（分岐キーの割当が type を見る）
    void ApplySurfaceSettings(const EditorManager& editor);
    void ResolveStartGoal(const EditorManager& editor);

    std::vector<SplineRail> rails_; // 実行用レール本体
    RailMarkers markers_;           // 緑線

    int   lastVersion_ = -1;        // 直近に同期したエディタ編集世代
    bool  prevEditorPreview_ = false; // 前フレームにエディタプレビュー中だったか
    float liftMarkerTime_ = 0.0f;     // 「！」目印の上下の揺れ用

    // スタート/ゴール（Sync 時にノード番号から距離へ変換して保持）
    int   startRail_ = 0;   float startDist_ = 0.0f;
    int   goalRail_  = -1;  float goalDist_  = 0.0f;

    Camera*  camera_ = nullptr;    // 直近 Sync のカメラ（マーカー生成・再構築に使う）
    uint32_t whiteTexIndex_ = 0;   // 単色化用テクスチャ
};
