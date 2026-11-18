#pragma once
#define NOMINMAX
#define DIRECTINPUT_VERSION 0x0800

// --- 標準ライブラリ・外部ライブラリ ---
#include <dinput.h>
#include "engine/base/Singleton.h"
#include <Xinput.h>
#include <Windows.h>
#include <wrl.h>
#include <cstdint>
#include <string>
#include <map>
#include <vector>
#include "engine/math/struct.h" // Vector2

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "xinput.lib") 

namespace KazumimiEngine {

// =====================================================================
//  Input：キーボード・マウス（DirectInput）と、ゲームパッド（XInput）の入力。
//   毎フレーム Update で「今」と「1 つ前」の状態を取り、押している（PushKey）・押した瞬間（TriggerKey）を答える。
//   BindAction で、キーとパッドのボタンを 1 つの名前（例：Jump）に束ねられる
// =====================================================================
class Input : public Singleton<Input> {
	friend class Singleton<Input>; // 土台（Singleton）だけが作れる
public:
	template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
	// 初期化と更新
	void Initialize(HWND hwnd);
	void Update();

	void Finalize();

	// ==========================================
	//  キーボード
	// ==========================================
	bool PushKey(BYTE keyNumber);
	bool TriggerKey(BYTE keyNumber);

	// 文字入力中（エディタの入力欄に文字を打っている間）はキーボードを「押されていない」扱いにする。
	//   これが無いと、名前を入力中に T を打っただけでタイトルへ戻る・R でシーンが読み直される等、
	//   ゲーム側のホットキーが誤って発動する。EditorManager が毎フレーム渡す
	// 遷移の演出中など、ゲーム側のキー操作を止める（true の間 PushKey / TriggerKey は常に false）
	void SetLocked(bool locked){ locked_ = locked; }
	bool IsLocked() const{ return locked_; }
	void SetTextInputActive(bool active){ textInputActive_ = active; }
	bool IsTextInputActive() const{ return textInputActive_; }

	// 撮影（ScreenCapture）用：遷移の演出中・文字入力中でも読むキー（F9/F12 などの撮影のキー）
	bool TriggerKeyRaw(BYTE keyNumber) const{ return !( preKeys_[keyNumber] & 0x80 ) && ( keys_[keyNumber] & 0x80 ); }
	// 撮影の「テイク」：今押しているキーの一覧（F1〜F12・Esc は除く。文字入力中は空）
	std::vector<uint8_t> GetPressedKeysForTake() const;
	// 撮影の「テイク」の再生：このフレームのキーを pressed に差し替える（Update の後に呼ぶ）。
	//   F1〜F12・Esc だけは本物のキーのまま（再生中も撮影やエディタの切り替えができるように）。
	//   コントローラーは「つながっていない」扱いにする
	void OverrideKeysForTake(const std::vector<uint8_t>& pressed);
	static bool IsTakeLiveKey(BYTE keyNumber);

	// ==========================================
	// 🖱 マウス
	// ==========================================
	// ボタン押下 (0:左, 1:右, 2:ホイール(中)ボタン)
	bool PushMouseButton(int buttonNumber);
	bool TriggerMouseButton(int buttonNumber);

	// マウスの移動量を取得（デバッグカメラで視点を回すのに使います！）
	float GetMouseMoveX();
	float GetMouseMoveY();
	float GetMouseWheel(); // ホイールの回転量

	// ==========================================
	// コントローラー
	// ==========================================
	// コントローラーが繋がっているか
	bool GetJoystickState();
	// ボタン押下 (XINPUT_GAMEPAD_A などを指定)
	bool PushJoystickButton(WORD button);
	bool TriggerJoystickButton(WORD button);
	// スティックの入力
	float GetLeftStickX();
	float GetLeftStickY();
	float GetRightStickX();
	float GetRightStickY();

	// スティックを Vector2 で取得（ラジアル・デッドゾーン処理済み、長さ0〜1）
	Vector2 GetLeftStick(float deadzone = 0.2f);
	Vector2 GetRightStick(float deadzone = 0.2f);

	// ==========================================
	// アクションマッピング（「ジャンプ」等にキー/パッドボタンを束ねる）
	//   BindAction("Jump", DIK_SPACE, XINPUT_GAMEPAD_A); のように複数回束ねられる
	//   padButton に 0 を渡すとキーボードのみ。
	// ==========================================
	void BindAction(const std::string& action, BYTE keyboardKey, WORD padButton = 0);
	void ClearAction(const std::string& action);     // そのアクションの束ねを全消去
	bool IsAction(const std::string& action);        // 押されている間 true
	bool IsActionTriggered(const std::string& action); // 押した瞬間だけ true

private:
	Input() = default;
	~Input() = default;

private:
	// キーボード用
	ComPtr<IDirectInputDevice8> keyboard_;
	BYTE keys_[256] = {};
	BYTE preKeys_[256] = {};
	bool locked_ = false;          // 遷移の演出中（true の間 PushKey / TriggerKey は常に false）
	bool textInputActive_ = false; // 文字入力中（true の間 PushKey / TriggerKey は常に false）

	// マウス用
	ComPtr<IDirectInputDevice8> mouse_;
	DIMOUSESTATE2 mouseState_ = {};
	DIMOUSESTATE2 preMouseState_ = {};

	// コントローラー用
	XINPUT_STATE joyState_ = {};
	XINPUT_STATE preJoyState_ = {};
	bool isJoyConnected_ = false;

	// アクション → (キーボードDIK, パッドボタン) の束ね。padButton=0 はキーボードのみ
	struct ActionBind{ BYTE key; WORD pad; };
	std::map<std::string, std::vector<ActionBind>> actions_;
};

} // namespace KazumimiEngine
