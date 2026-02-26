#pragma once
// --- エンジン側のファイル ---
#include "engine/math/struct.h"

// 前方宣言
class Camera;

class DebugCamera {
public:
    // 初期化
    void Initialize();
    // 更新 
    void Update(Camera* camera);
};