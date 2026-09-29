#include "TitleScene.h"
// --- ゲーム固有のファイル ---
#include "GamePlayScene.h"

// --- エンジン側のファイル ---
#include "Engine/Audio/AudioManager.h"
#include "Engine/3D/Model/ModelManager.h"
#include "Engine/Particle/ParticleManager.h"
#include "Engine/Graphics/TextureManager.h"
#include "Engine/Scene/SceneManager.h"
#include "Engine/2D/Sprite.h"
#include "Engine/Base/Input.h"
#include "Engine/Base/WindowProc.h"
#include "engine/sdf/SDFManager.h"
#include "engine/sdf/SDFText.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <cmath>

TitleScene::TitleScene(){
	features_.overlay2D    = true; // スタート案内を最終画像へ重ねる
}
TitleScene::~TitleScene() = default;

// 音・モデル・テクスチャ（実行中の読み込みはデバッグレイヤーが嫌うので、使う可能性がある物は先に読む）
void TitleScene::OnLoadResources(){
	AudioManager::GetInstance()->LoadWave(bgmFile_);
	// マップエディタが置く物のモデル（タイトルでもマップの配置物を表示・編集できるので先に読んでおく）
	ModelManager* modelManager = ModelManager::GetInstance();
	modelManager->LoadModel("fence", "resources", "fence.obj");
	modelManager->LoadModel("grass", "resources", "terrain.obj");
	modelManager->LoadModel("block", "resources/block", "block.obj");
	modelManager->CreateSphereModel("sphere", 16);

	TextureManager* textureManager = TextureManager::GetInstance();
	textureManager->Load("resources/monsterBall.png");
	textureManager->Load("resources/fence.png");
	textureManager->Load("resources/circle.png");
}

void TitleScene::OnInitialize(){
	
	// スタート案内（アトラスは SDFManager が resources/sdf/ から自動ロードするので、ここではアイテムを作るだけ）
	startText_ = std::make_unique<SDFText>();
	startText_->Initialize();
	startText_->SetText("T : ゲームスタート");
	startText_->SetFontSize(40.0f);
	startText_->SetOutlineWidth(0.2f);
	startText_->SetOutlineColor({ 0.05f, 0.1f, 0.05f, 1.0f });
}

void TitleScene::OnUpdate(){
	Input* input = Input::GetInstance();
	// Space=BGM再生 / T=ゲームプレイへ / P=パーティクル
	if ( input->Triggerkey(DIK_SPACE) ) {
		AudioManager::GetInstance()->PlayWave(bgmFile_);
	}
	if ( input->Triggerkey(DIK_T) ) {
		SceneManager::GetInstance()->ChangeScene(std::make_unique<GamePlayScene>());
	}
	if ( input->Triggerkey(DIK_P) ) {
		ParticleManager::GetInstance()->Emit("Circle", { 0.0f, 0.0f, 0.0f }, 10);
	}

	
	UpdateTitleAnimation();
}

// スタート案内をゆっくり点滅（フェード）させてスタート待ちを示す
void TitleScene::UpdateTitleAnimation(){
	titleAnimTime_ += 1.0f / 60.0f;
	float W = ( float ) WindowProc::GetInstance()->GetClientWidth();
	float H = ( float ) WindowProc::GetInstance()->GetClientHeight();
	if ( startText_ ) {
		float a = 0.55f + 0.45f * std::sin(titleAnimTime_ * 2.5f);
		startText_->SetColor({ 1.0f, 1.0f, 1.0f, a });
		startText_->SetPosition(W * 0.5f - 170.0f, H * 0.82f);
	}
}

// スタート案内（最終画像へ重ねる＝Game View にもフルスクリーンにも映る）
void TitleScene::OnDrawOverlay2D(ID3D12GraphicsCommandList* commandList){
	if ( startText_ )  { SDFManager::GetInstance()->DrawTextItem(commandList, *startText_, "jpdot"); }
}

void TitleScene::OnDrawUI(ID3D12GraphicsCommandList* /*commandList*/){

}

void TitleScene::OnDebugUI(){
#ifdef USE_IMGUI

#endif
}
