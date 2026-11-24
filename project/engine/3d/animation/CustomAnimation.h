#pragma once
#include <vector>
#include <string>
#include "engine/math/struct.h"

namespace KazumimiEngine {

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
    // --- 読む ---
    const std::string& ObjectName() const { return objectName_; }
    const std::vector<TransformKeyframe>& Keyframes() const { return keyframes_; }
    float Duration() const { return duration_; }

    // --- キーフレームの操作（エディタ用）---
    // 1 つを書き換える（並べ替えはしない。時間を変えた後は SortKeyframes を呼ぶ）
    void SetKeyframe(size_t index, const TransformKeyframe& key) { if ( index < keyframes_.size() ) { keyframes_[index] = key; } }
    // 時間順に並べ直して、総時間を最後のキーフレームに合わせる
    void SortKeyframes();
    // 1 つ消して、総時間を最後のキーフレームに合わせる（無くなったら 0）
    void RemoveKeyframe(size_t index);
    // 全部消す（総時間も 0）
    void Clear() { keyframes_.clear(); duration_ = 0.0f; }
    // 全体の速さを変える（すべての時間と総時間に factor を掛ける）
    void ScaleTime(float factor);
    // 総時間を time まで延ばす（短くはしない）
    void ExtendDuration(float time) { if ( duration_ < time ) { duration_ = time; } }

    /// <summary>
    /// 現在の状態をキーフレームとして記録する
    /// </summary>
    void AddKeyframe(float time, const Vector3& pos, const Vector3& rot, const Vector3& scale);

    /// <summary>
    ///　
    /// </summary>
    void UpdateTransformAtTime(float currentTime, Vector3& outPos, Vector3& outRot, Vector3& outScale) const;

    // アニメーションをテキストファイルに書き出す
    void SaveToFile(const std::string& filepath) const;
    // テキストファイルからアニメーションを読み込む
    void LoadFromFile(const std::string& filepath);

    // JSON ファイルに書き出し
    void SaveToJson(const std::string& filepath) const;
    // JSON ファイルから読み込み
    void LoadFromJson(const std::string& filepath);

private:
    std::string objectName_;                     // 動かす対象の名前（識別用）
    std::vector<TransformKeyframe> keyframes_;   // 記録したキーフレームの配列（時間順）
    float duration_ = 0.0f;                      // アニメーションの総時間（最後のキーフレームの時間）
};

} // namespace KazumimiEngine
