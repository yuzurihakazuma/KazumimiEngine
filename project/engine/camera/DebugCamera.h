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
	// 外部（メニュー等）から ON/OFF する。切替時に元カメラ姿勢を保存/復元する
    void SetActive(bool active);

	// Game View（ゲーム画面）にマウスが乗っているか。EditorManager が毎フレーム渡す。
	//   false の間はホイールズームを無効化する（ImGuiパネル上のスクロールでズームしない）。
	//   エディタ非アクティブ（フルスクリーン）では常に true 扱い。
	void SetGameViewHovered(bool hovered){ gameViewHovered_ = hovered; }
	// 右/中ドラッグを始めてよいか（Game View の上か）。入力欄が有効な間でも true になる、ゆるい判定
	void SetGameViewDragHovered(bool hovered){ gameViewDragHovered_ = hovered; }
private:
	// デバッグカメラがアクティブかどうかを管理するフラグ
    bool isActive_ = false;

	// Game View にマウスが乗っているか（ホイールズームの可否に使う）。既定は許可。
	bool gameViewHovered_ = true;
	bool gameViewDragHovered_ = true; // 右/中ドラッグの始まりに使う（入力欄が有効な間も Game View の上なら true）

	// 右ドラッグ（回転）・中ドラッグ（平行移動）は「Game View の上で押し始めた時」だけ効かせる。
	//   ImGuiパネルの中で右ドラッグ/中ドラッグしても（配置ビューで消す・表示を動かす等）
	//   カメラが一緒に動かないようにするため、押し始めた場所を覚えておく
	bool rightWasDown_  = false;
	bool middleWasDown_ = false;
	bool rotateDragValid_ = false; // 今の右ドラッグは Game View の上で始まったか
	bool panDragValid_    = false; // 今の中ドラッグは Game View の上で始まったか

    // デバッグカメラ切り替え時に元のカメラ位置を保持するための変数
    Vector3 preCameraPos_ = { 0.0f, 0.0f, 0.0f };
    Vector3 preCameraRot_ = { 0.0f, 0.0f, 0.0f };

    // Update内で現在操作中のカメラポインタを保持しておく（UIから操作するため）
    Camera* targetCamera_ = nullptr;
};