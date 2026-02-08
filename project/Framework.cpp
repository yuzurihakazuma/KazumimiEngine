#include "Framework.h"
#include "WindowProc.h"
#include "DirectXCommon.h"
#include "Input.h"
#include "AudioManager.h" 
#include "SrvManager.h"
#include "ResourceFactory.h"
#include "ImGuiManager.h"
#include "SpriteCommon.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "ParticleManager.h"
#include "TextureManager.h"
#include "PipelineManager.h"
#include <engine/scene/AbstractSceneFactory.h>

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
	// 各マネージャー終了
	AudioManager::GetInstance()->Finalize();
	ModelManager::GetInstance()->Finalize();
	ParticleManager::GetInstance()->Finalize();
	TextureManager::GetInstance()->Finalize();
	PipelineManager::GetInstance()->Finalize();

	// ImGui終了
	ImGuiManager::GetInstance()->Shutdown();
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