#include "Framework.h"
// --- エンジン側のファイル ---
#include "engine/base/WindowProc.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/Input.h"
#include "engine/audio/AudioManager.h" 
#include "engine/graphics/SrvManager.h"
#include "engine/graphics/ResourceFactory.h"
#include "engine/utils/ImGuiManager.h"
#include "engine/2d/SpriteCommon.h"
#include "engine/3d/obj/Obj3dCommon.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/particle/ParticleManager.h"
#include "engine/graphics/TextureManager.h"
#include "engine/graphics/PipelineManager.h"
#include "engine/scene/AbstractSceneFactory.h"
#include "engine/scene/SceneManager.h"

void Framework::Initialize(){
	// ---------------------------------------------
	// 基盤システムの初期化
	// ---------------------------------------------
	
	// WindowProc
	WindowProc* windowProc = WindowProc::GetInstance();
	windowProc->Initialize();

	
	// DirectXCommon
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	DirectXCommon::GetInstance()->Initialize(windowProc);

	// Input
	Input::GetInstance()->Initialize(windowProc->GetHwnd());

	// SrvManager
	SrvManager* srvManager = SrvManager::GetInstance();
	srvManager->Initialize(dxCommon);
	dxCommon->SetSrvManager(srvManager);

	// ResourceFactory
	ResourceFactory::GetInstance()->SetDevice(dxCommon->GetDevice());
	dxCommon->SetResourceFactory(ResourceFactory::GetInstance());

	// TextureManager
	TextureManager::GetInstance()->Initialize(dxCommon->GetDevice(), dxCommon, srvManager);
	TextureManager::GetInstance()->SetResourceFactory(ResourceFactory::GetInstance());
	// PipelineManager
	PipelineManager::GetInstance()->Initialize(dxCommon);

	// ImGuiManager
	ImGuiManager* imguiManager= ImGuiManager::GetInstance();
	imguiManager->Initialize(windowProc, dxCommon);

	// Audio
	AudioManager::GetInstance()->Initialize();

	// スプライト共通初期化
	SpriteCommon::GetInstance()->Initialize(dxCommon);

	// 3Dオブジェクト共通初期化
	Obj3dCommon::GetInstance()->Initialize(dxCommon);

	// モデルマネージャー初期化
	ModelManager::GetInstance()->Initialize(dxCommon);

	// パーティクルマネージャー初期化
	ParticleManager::GetInstance()->Initialize(dxCommon, srvManager);

}

void Framework::Finalize(){

	// ---------------------------------------------
	// 終了処理は初期化の逆順で行う
	// ---------------------------------------------
	SceneManager::GetInstance()->Finalize();

	// 2. ゲーム固有のマネージャー類を終了
	ParticleManager::GetInstance()->Finalize();
	ModelManager::GetInstance()->Finalize();
	Obj3dCommon::GetInstance()->Finalize();
	AudioManager::GetInstance()->Finalize();

	// 3. ImGuiは描画系のマネージャーより先に終了させるのが安全
	ImGuiManager::GetInstance()->Shutdown();

	// 4. 描画・基盤系のマネージャーを終了
	PipelineManager::GetInstance()->Finalize();
	TextureManager::GetInstance()->Finalize();
	SrvManager::GetInstance()->Finalize();
	Input::GetInstance()->Finalize();
	ResourceFactory::GetInstance()->Finalize();


	// 5. 最後に DirectXCommon と WindowProc を終了
	DirectXCommon::GetInstance()->Finalize();
}

void Framework::Update(){
	// 基盤更新
	WindowProc::GetInstance()->Update();
	// 入力更新
	Input::GetInstance()->Update();

	// ウィンドウが閉じられたら終了フラグを立てる
	if ( WindowProc::GetInstance()->GetIsClosed() ) {
		endRequest_ = true;
	}
}

void Framework::Run(){
	
	// メインループ
	while ( true ) {
		// 毎フレーム更新
		Update();

		// 終了リクエストがあったらループを抜ける
		if ( IsEndRequest() ) {
			break;
		}

		// 描画
		Draw();
	}

}

bool Framework::IsEndRequest(){
	return endRequest_;
}