#pragma once
#include <vector>
#include <string>
#include "engine/math/struct.h"

// --------------------------------------------------
// 1つのキーフレーム（ある瞬間の座標・回転・スケール）
// --------------------------------------------------
struct TransformKeyframe {
    float time;       // アニメーションの時間（秒）
    Vector3 position; // その時間での座標
    Vector3 rotation; // その時間での回転
    Vector3 scale;    // その時間でのスケール
};

// --------------------------------------------------
// 1つのオブジェクトのアニメーショントラック（動きの全体）
// --------------------------------------------------
class CustomAnimationTrack {
public:
    std::string objectName;                     // 動かす対象の名前（識別用）
    std::vector<TransformKeyframe> keyframes;   // 記録したキーフレームの配列
    float duration = 0.0f;                      // アニメーションの総時間（最後のキーフレームの時間）

public:
    /// <summary>
    /// 現在の状態をキーフレームとして記録する
    /// </summary>
    void AddKeyframe(float time, const Vector3& pos, const Vector3& rot, const Vector3& scale);

    /// <summary>
    /// 指定された時間から、計算された座標・回転・スケールを返す
    /// </summary>
    void UpdateTransformAtTime(float currentTime, Vector3& outPos, Vector3& outRot, Vector3& outScale) const;

    // アニメーションをテキストファイルに書き出す
    void SaveToFile(const std::string& filepath) const;
    // テキストファイルからアニメーションを読み込む
    void LoadFromFile(const std::string& filepath);

};
