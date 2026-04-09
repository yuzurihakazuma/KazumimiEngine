#pragma once
#include "engine/3d/obj/IAnimatable.h"
#include "engine/3d/animation/CustomAnimation.h"
#include "engine/camera/Camera.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>

/// <summary>
/// アニメーションエディター
/// キーフレームの記録・編集・再生UIを一括管理する
/// IAnimatableを実装したオブジェクトであれば何でも編集対象にできる
/// </summary>
class AnimationEditor {
public:
    /// <summary>初期化</summary>
    void Initialize();

    /// <summary>アニメーション再生・補間の更新（毎フレーム呼ぶ）</summary>
    void Update();

    /// <summary>ImGui UIの描画（DrawDebugUI内で呼ぶ）</summary>
    void DrawDebugUI();

    /// <summary>編集対象オブジェクトを登録する</summary>
    void AddTarget(IAnimatable* target);

    /// <summary>カメラを設定する（将来のGizmo計算用）</summary>
    void SetCamera(const Camera* camera) { camera_ = camera; }

private:
    const Camera* camera_ = nullptr; // カメラ（所有しない）

    // 編集対象オブジェクト一覧（所有しない）
    std::vector<IAnimatable*> targets_;

    // オブジェクトごとのアニメーショントラック
    std::unordered_map<std::string, CustomAnimationTrack> animTracks_;

    int   selectedTargetIndex_ = 0;     // 現在選択中のオブジェクト番号
    float animCurrentTime_ = 0.0f;  // 現在の再生時間（秒）
    bool  isAnimPlaying_ = false; // 再生中フラグ
    int   selectedKeyframeIndex_ = -1;    // 選択中のキーフレーム番号（-1=未選択）
    int   fps_ = 60;    // フレームレート設定
    int   currentFrame_ = 0;     // 現在のフレーム番号
    float timelineZoom_ = 1.0f;  // タイムライン拡大率（将来用）
    float timelineDuration_ = 5.0f;  // タイムラインの表示秒数上限
    float playbackSpeed_ = 1.0f;  // 再生速度倍率
    bool  isLoop_ = true;  // ループ再生フラグ
    int   draggingKeyframe_ = -1;    // タイムライン上でドラッグ中のキーフレーム番号

    std::string saveFilePath_;

    int copiedKeyframeIndex_ = -1; // コピー元のキーフレーム番号
    bool snapToFrame_ = true;
};
