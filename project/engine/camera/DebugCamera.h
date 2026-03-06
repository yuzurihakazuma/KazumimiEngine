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
	// デバッグ用UIの描画
    void DrawDebugUI();
	// デバッグカメラがアクティブかどうか
    bool IsActive() const { return isActive_; }
private:
	// デバッグカメラがアクティブかどうかを管理するフラグ
    bool isActive_ = false;

    // デバッグカメラ切り替え時に元のカメラ位置を保持するための変数
    Vector3 preCameraPos_ = { 0.0f, 0.0f, 0.0f };
    Vector3 preCameraRot_ = { 0.0f, 0.0f, 0.0f };

    // Update内で現在操作中のカメラポインタを保持しておく（UIから操作するため）
    Camera* targetCamera_ = nullptr;
};