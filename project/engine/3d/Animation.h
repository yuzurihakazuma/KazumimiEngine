#pragma once

// --- エンジン側のファイル ---
#include "struct.h"

// --- 標準ライブラリ ---]
#include <vector>
#include <string>
#include <map>



// キーフレームを表す構造体
template<typename tValue>

struct Keyframe{
	float time; // キーフレームの時間
	tValue value; // キーフレームの値
};

// キーフレームの型エイリアス
using KeyFrameVector3 = Keyframe<Vector3>;
using KeyFrameQuaternion = Keyframe<Quaternion>;

// アニメーションカーブを表す構造体
template <typename tValue>
struct AnimationCurve{
	std::vector<Keyframe<tValue>> keyframes;
};

struct NodeAnimation{
	AnimationCurve<Vector3> translate; // 移動（位置）のカーブ
	AnimationCurve<Quaternion> rotate; // 回転のカーブ
	AnimationCurve<Vector3> scale;     // 拡大縮小のカーブ
};

// アニメーション全体を表す構造体
struct Animation{
	float duration; // アニメーション全体の長さ（尺：秒）

	// 「骨の名前（文字列）」をキーにして、その骨の動きのデータを引き出せる辞書
	std::map<std::string, NodeAnimation> nodeAnimations;
};

// アニメーションのキーフレームを表す構造体
class Animation{};

