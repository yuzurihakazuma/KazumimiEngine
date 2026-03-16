#pragma once

// --- エンジン側のファイル ---
#include "struct.h"

// --- 標準ライブラリ ---]
#include <vector>
#include <string>
#include <map>



// キーフレームを表す構造体
template<typename tValue>

struct KeyFrame{
	float time; // キーフレームの時間
	tValue value; // キーフレームの値
};

using KeyFrameVector3 = KeyFrame<Vector3>;


// アニメーションのキーフレームを表す構造体
class Animation{};

