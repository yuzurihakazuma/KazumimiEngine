#pragma once
#define NOMINMAX
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <Windows.h>
#include <wrl.h>
#include <cstdint>
#include"LogManager.h"

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")



class Input{
public:
	// namespace省略
	template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;
	// シングルトンインスタンス取得
	static Input* GetInstance();

public:
	// 初期化
	void Initialize(HWND hwnd);
	// 更新
	void Update();
	/// <summary>
	/// キーの押下をチェック
	/// </summary>
	/// <param name="keyNumber">キー番号(DIK_0等)</param>
	/// <returns>押されているか</returns>
	bool Pushkey(BYTE keyNumber);
	/// <summary>
	/// キーのトリガーチェック
	/// </summary>
	/// <param name="keyNumber">キー番号(DIK_0等)</param>
	/// <returns>トリガーか</returns>
	bool Triggerkey(BYTE keyNumber);

private:
	// コンストラクタを private にして、外部からの生成を禁止する
	Input() = default;
	~Input() = default;
	// コピー禁止
	Input(const Input&) = delete;
	Input& operator=(const Input&) = delete;

private:
	// キーボードのデバイス
	ComPtr<IDirectInputDevice8> keyboard;
	// 全キーの状態
	BYTE keys[256] = {};
	// 前回フレームの全キーの状態
	BYTE preKeys[256] = {};
};

