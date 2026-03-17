#include <Windows.h>

#include "Game.h"
#include "CrashDumper.h"
#include <engine/scene/SceneManager.h>
#include "ResourceLeakChecker.h"
#include "scene/TitleScene.h"

// Windowsアプリでのエントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int){

	ResourceLeakChecker leakCheck;

	// COM初期化
	CoInitializeEx(0, COINIT_MULTITHREADED);

	// クラッシュダンパー登録
	CrashDumper::Install();

	// ゲームクラスの生成
	std::unique_ptr<Game> game= std::make_unique<Game>();

	// 実行フロー
	game->Initialize(); // 初期化
	game->Run();        // メインループ
	game->Finalize();   // 終了処理（

	// COM終了
	CoUninitialize();

	return 0;
}