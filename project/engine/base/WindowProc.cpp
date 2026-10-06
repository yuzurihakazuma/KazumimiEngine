#include "WindowProc.h"
// --- 標準ライブラリ・外部ライブラリ ---
#include <Windows.h>
#include <shellapi.h> // D&D受付 (DragAcceptFiles / DragQueryFile)

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "shell32.lib")


WindowProc* WindowProc::GetInstance(){
	static WindowProc instance;
	return &instance;
}

// WindowProcの初期化
void WindowProc::Initialize(){

	// ウィンドウクラスの設定
	SetupWindowClass(wc_);
	// ウィンドウクラスの登録
	RegisterWindowClass();
	// クライアント領域の調整
	AdjustClientRect();
	// メインウィンドウの作成
	CreateMainWindow();
	// メインウィンドウの表示
	ShowMainWindow();

	// エクスプローラーからのファイルD&Dを受け付ける
	DragAcceptFiles(hwnd_, TRUE);

	timeBeginPeriod(1); // タイマーの分解能を1msに設定

}
// ウィンドウクラスの設定
void WindowProc::SetupWindowClass(WNDCLASS& wc){

	// ウィンドウプロシージャを設定
	wc.lpfnWndProc = WndProc;

	// ウィンドウクラス名
	wc.lpszClassName = L"CG2WindowClass";

	// インスタンスハンドル
	wc.hInstance = GetModuleHandle(nullptr);
	// カーソル
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

}

// ウィンドウクラスの登録
void WindowProc::RegisterWindowClass(){

	// ウィンドウクラスを登録
	RegisterClass(&wc_);

}
// クライアント領域の調整
void WindowProc::AdjustClientRect(){

	// クライアント領域サイズ
	wrc_ = { 0, 0, kClientWidth_, kClientHeight_ };

	// クライアント領域を元に実際のサイズにwrc_を変更してもらう
	AdjustWindowRect(&wrc_, WS_OVERLAPPEDWINDOW, false);

}
// メインウィンドウの作成
void WindowProc::CreateMainWindow(){

	// ウィンドウの生成
	hwnd_ = CreateWindow(
		wc_.lpszClassName,      // 利用するクラス名
		L"CG2",                // タイトルバーの文字(なんでもいい)
		WS_OVERLAPPEDWINDOW,   // よく見るウィンドウスタイル
		CW_USEDEFAULT,		   // 表示X座標(Windowsに任せる)
		CW_USEDEFAULT,		   // 表示Y座標(WindowsOSに任せる)
		wrc_.right - wrc_.left,  // ウィンドウ横幅
		wrc_.bottom - wrc_.top,  // ウィンドウ立幅
		nullptr,			   // 親ウィンドウハンドル
		nullptr,			   // メニューハンドル
		wc_.hInstance,		   // インスタンスハンドル
		nullptr				   // オプション
	);



}
// メインウィンドウの表示
void WindowProc::ShowMainWindow(){

	// ウィンドウの表示
	ShowWindow(hwnd_, SW_SHOW);

}

// フルスクリーン ⇔ ウィンドウ の切り替え
void WindowProc::ToggleFullscreen(){
	if ( !hwnd_ ) { return; }
	if ( !isFullscreen_ ) {
		// 今のウィンドウの位置・大きさ・枠を覚えてから、枠を外してモニター全体へ広げる
		GetWindowRect(hwnd_, &windowedRect_);
		windowedStyle_ = GetWindowLong(hwnd_, GWL_STYLE);
		MONITORINFO monitorInfo = {};
		monitorInfo.cbSize = sizeof(monitorInfo);
		GetMonitorInfo(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &monitorInfo);
		const RECT& screen = monitorInfo.rcMonitor;
		SetWindowLong(hwnd_, GWL_STYLE, windowedStyle_ & ~WS_OVERLAPPEDWINDOW);
		SetWindowPos(hwnd_, HWND_TOP, screen.left, screen.top,
			screen.right - screen.left, screen.bottom - screen.top,
			SWP_FRAMECHANGED | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
		isFullscreen_ = true;
	} else {
		// 覚えておいた枠・位置・大きさへ戻す
		SetWindowLong(hwnd_, GWL_STYLE, windowedStyle_);
		SetWindowPos(hwnd_, nullptr, windowedRect_.left, windowedRect_.top,
			windowedRect_.right - windowedRect_.left, windowedRect_.bottom - windowedRect_.top,
			SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
		isFullscreen_ = false;
	}
	// 大きさが変わるので、描画先（スワップチェーン・深度・ポストエフェクト）を作り直してもらう
	isResized_ = true;
}

// ウィンドウの更新
void WindowProc::Update(){

	MSG msg = {};
	// メッセージキューからメッセージを取得
	while ( PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) ) {
		// メッセージがWM_QUITならばアプリケーションを終了
		if ( msg.message == WM_QUIT ) {
			isClosed_ = true; // ウィンドウが閉じられたことを記録
			return;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

}


// ウィンドウプロシージャ
LRESULT WindowProc::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam){
#ifdef USE_IMGUI
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}
#endif

	switch ( msg ) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_SIZE:
		// ウィンドウのサイズが変更されたときの処理
		if ( wparam!=SIZE_MINIMIZED ){
			kClientWidth_ = LOWORD(lparam);  // クライアント領域の横幅を更新
			kClientHeight_ = HIWORD(lparam); // クライアント領域の縦幅を更新
			isResized_ = true; // ウィンドウサイズ変更フラグを立てる
		}
		break;
	case WM_KEYDOWN:
		// F11：フルスクリーン ⇔ ウィンドウ（押しっぱなしの連続入力では切り替えない）
		if ( wparam == VK_F11 && ( lparam & ( 1 << 30 ) ) == 0 ) {
			GetInstance()->ToggleFullscreen();
			return 0;
		}
		break;

	case WM_ENTERSIZEMOVE:

		isResized_ = true; // ウィンドウサイズ変更フラグを立てる
		break;

	case WM_DROPFILES: {
		// エクスプローラーからファイルがドロップされた
		HDROP hDrop = ( HDROP ) wparam;
		UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
		for ( UINT i = 0; i < count; ++i ) {
			wchar_t widePath[MAX_PATH] = {};
			if ( DragQueryFileW(hDrop, i, widePath, MAX_PATH) ) {
				// ワイド文字 → マルチバイト（std::ifstream等に渡せる形式）に変換
				char path[MAX_PATH * 2] = {};
				WideCharToMultiByte(CP_ACP, 0, widePath, -1, path, sizeof(path), nullptr, nullptr);
				droppedFiles_.push_back(path);
			}
		}
		DragFinish(hDrop);
		return 0;
	}

	}
	// デフォルトのウィンドウプロシージャを呼び出す
	return DefWindowProc(hwnd, msg, wparam, lparam);
}


