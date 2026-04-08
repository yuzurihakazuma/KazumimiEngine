#include "GamePlayScene.h"
// --- ゲーム固有のファイル ---
#include "TitleScene.h"

// --- エンジン側のファイル ---
#include "Engine/Utils/ImGuiManager.h"
#include "Engine/Audio/AudioManager.h"
#include "Engine/3D/Model/ModelManager.h"
#include "Engine/Particle/ParticleManager.h"
#include "Engine/Graphics/TextureManager.h"
#include "Engine/Graphics/PipelineManager.h"
#include "Engine/Scene/SceneManager.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Camera/DebugCamera.h"
#include "Engine/2D/Sprite.h"
#include "Engine/3D/Obj/Obj3d.h"
#include "Engine/Base/Input.h"
#include "Engine/2D/SpriteCommon.h"
#include "Engine/3D/Obj/Obj3dCommon.h"
#include "Engine/Base/DirectXCommon.h"
#include "Engine/Base/WindowProc.h"
#include "engine/math/VectorMath.h"
#include "engine/collision/Collision.h"
#include "engine/graphics/RenderTexture.h"
#include "engine/graphics/SrvManager.h"
#include "engine/postEffect/PostEffect.h"
#include "engine/utils/TextManager.h"
#include "Bloom.h"
#include "engine/3d/model/Model.h"
#include "engine/utils/EditorManager.h"
#include "engine/3d/obj/SkinnedObj3d.h"

using namespace VectorMath;
using namespace MatrixMath;
// 初期化
void GamePlayScene::Initialize(){
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	WindowProc* windowProc = WindowProc::GetInstance();

	
	
	// コマンドリスト取得
	auto commandList = dxCommon->GetCommandList();

	// BGMロード (シングルトン)
	AudioManager::GetInstance()->LoadWave(bgmFile_);
	// モデル読み込み (シングルトン)
	ModelManager::GetInstance()->LoadModel("fence", "resources", "fence.obj");
	
	ModelManager::GetInstance()->LoadModel("grass", "resources", "terrain.obj");
	ModelManager::GetInstance()->LoadModel("block", "resources/block","block.obj");

	// 球モデル作成 (シングルトン)
	ModelManager::GetInstance()->CreateSphereModel("sphere", 16);
	// パーティクルグループ作成 (シングルトン)
	ParticleManager::GetInstance()->CreateParticleGroup("Circle", "resources/uvChecker.png");

	// テクスチャ読み込み
	textures_["uvChecker"] = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/uvChecker.png", commandList);
	textures_["monsterBall"] = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/monsterBall.png", commandList);
	textures_["fence"] = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/fence.png", commandList);
	textures_["circle"] = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/circle.png", commandList);
	textures_["noise0"] = { TextureManager::GetInstance()->LoadTextureAndCreateSRV("Resources/noise0.png", commandList) };
	textures_["noise1"] = { TextureManager::GetInstance()->LoadTextureAndCreateSRV("Resources/noise1.png", commandList) };
	
	// エディタマネージャーの生成
	EditorManager::GetInstance()->Initialize();



	// モデル読み込み (シングルトン)
	// アニメーション
	ModelManager::GetInstance()->LoadModel("animatedCube", "resources/AnimatedCube", "AnimatedCube.gltf");
	testAnimation_ = LoadAnimationFromFile("resources/AnimatedCube", "AnimatedCube.gltf");
	ModelManager::GetInstance()->LoadModel("human", "resources/human", "walk.gltf");


	// カメラ生成
	camera_ = std::make_unique<Camera>(windowProc->GetClientWidth(), windowProc->GetClientHeight(), dxCommon);
	camera_->SetTranslation({ 0.0f, 2.0f, -15.0f });

	
	// デバッグカメラ生成
	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize();

	// プレイヤーオブジェクト生成
	testObj_ = Obj3d::Create("animatedCube");
	if ( testObj_ ){

		testObj_->SetCamera(camera_.get());
		testObj_->SetTranslation({ 0.0f, 0.0f, 5.0f });

		// ノイズ画像と初期の閾値(0.0)をセット
		testObj_->SetNoiseTexture(textures_["noise0"].srvIndex);
		testObj_->SetDissolveThreshold(0.0f);

		//testObj_->PlayAnimation(&testAnimation_);

		Bloom::GetInstance()->SetTargetEmissivePower(&testObj_->GetModel()->GetMaterial()->emissive);
	}

	

	skinnedObj_ = SkinnedObj3d::Create("human", "resources/human", "walk.gltf");
	skinnedObj_->SetCamera(camera_.get());
	skinnedObj_->SetTranslation({ 0.0f, 0.0f, 5.0f });
	skinnedObj_->SetScale({ 1.0f, 1.0f, 1.0f });
	skinnedObj_->SetRotation({ 0.0f, 3.14159f, 0.0f });


	// デプスステンシル作成 (TextureManagerシングルトン)
	depthStencilResource_ = TextureManager::GetInstance()->CreateDepthStencilTextureResource(
		windowProc->GetClientWidth(), windowProc->GetClientHeight()
	);

	

	EditorManager::GetInstance()->SetCamera(camera_.get());

	sprite_ = Sprite::Create(textures_["uvChecker"].srvIndex, spritePos_);
	
	
	blockGroup_ = std::make_unique<InstancedGroup>();
	blockGroup_->Initialize("block", 10000); // 最大1万個まで対応！
	blockGroup_->SetNoiseTexture(textures_["noise0"].srvIndex);

	// 試しに20x20 = 400個のブロックを床のように敷き詰めてみます
	for (int x = -100; x < 100; ++x) {
		for (int z = 0; z < 20; ++z) {
			auto block = Obj3d::Create("block");
			block->SetTranslation({ x * 2.0f, -2.0f, z * 2.0f });
			block->SetCamera(camera_.get());
			blocks_.push_back(std::move(block));
		}
	}


}

void GamePlayScene::Update(){
	
	// デバッグカメラ更新
	if (debugCamera_) {
		debugCamera_->Update(camera_.get());
	}
	// カメラ更新
	camera_->Update();

	Input* input = Input::GetInstance();

	// BGM再生 (シングルトン)
	if ( input->Triggerkey(DIK_SPACE) ) {
		AudioManager::GetInstance()->PlayWave(bgmFile_);
	}
	// タイトルシーンへ移動
	if ( input->Triggerkey(DIK_T) ) {
		SceneManager::GetInstance()->ChangeScene(std::make_unique<TitleScene>());
	}
	// パーティクル発生 (シングルトン)
	if ( input->Triggerkey(DIK_P) ) {
		ParticleManager::GetInstance()->Emit("Circle", { 0.0f, 0.0f, 0.0f }, 10);
	}
	// パーティクル更新
	ParticleManager::GetInstance()->Update(camera_.get());


	// 全オブジェクト更新
	for ( auto& obj : object3ds_ ) {
		obj->Update();
		
	}
	if ( testObj_ ){
		testObj_->Update();
	}

	if (skinnedObj_) {
		// 再生中なら時間を進めて、計算したTransformを適用
		if (isAnimPlaying_ && myAnimTrack_.duration > 0.0f) {
			animCurrentTime_ += 1.0f / 60.0f; // 60FPS想定
			if (animCurrentTime_ > myAnimTrack_.duration) {
				animCurrentTime_ = 0.0f; // ループ再生
			}

			Vector3 animPos, animRot, animScale;
			myAnimTrack_.UpdateTransformAtTime(animCurrentTime_, animPos, animRot, animScale);

			skinnedObj_->SetTranslation(animPos);
			skinnedObj_->SetRotation(animRot);
			skinnedObj_->SetScale(animScale);
		}

		skinnedObj_->Update();
	}

	if ( sprite_ ) {
		sprite_->Update();
	}

	PostEffect::GetInstance()->Update();

	
	for (auto& block : blocks_) {
		block->Update();
	}
	// InstancedGroup に「最新のデータをお願い！」と渡すだけ
	if (blockGroup_) {
		blockGroup_->Update(blocks_);
	}

}

void GamePlayScene::Draw(){
	auto dxCommon = DirectXCommon::GetInstance();
	auto commandList = dxCommon->GetCommandList();

	// 1. 【MRT開始】キャンバスを2枚(色用とマスク用)セットする！
	PostEffect::GetInstance()->PreDrawSceneMRT(commandList);

	// --- 3D描画の前準備 ---
	Obj3dCommon::GetInstance()->PreDraw(commandList);

	// --- カリングありの3D描画 ---
	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Object3D);
	for ( auto& obj : object3ds_ ) { obj->Draw(); }
	
	
	EditorManager::GetInstance()->Draw();

	// --- カリングなしの3D描画 ---
	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Object3D_CullNone);
	if ( testObj_ ){ testObj_->Draw(); }

	// --- インスタンシングの3D描画 ---
	if ( blockGroup_ ) { blockGroup_->Draw(camera_.get()); }

	// 
	if (skinnedObj_) { skinnedObj_->Draw(); }


	// --- パーティクル描画 ---
	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Particle);
	ParticleManager::GetInstance()->Draw(commandList);


	// 2. 【MRT終了】3Dの描画が終わったので、2枚のキャンバスを読み込みモードに戻す
	PostEffect::GetInstance()->PostDrawSceneMRT(commandList);


	// 3. いつものPostEffect（色用のキャンバスだけに処理がかかります）
	 PostEffect::GetInstance()->Draw(commandList); // ※もしこの関数を作っていれば

	// 4. エフェクト後の「色画像」と「マスク画像」の番号(SRV)をもらう
	uint32_t colorSrv = PostEffect::GetInstance()->GetSrvIndex();
	uint32_t maskSrv = PostEffect::GetInstance()->GetMaskSrvIndex();

	// 5. Bloomに「色」と「マスク」を両方渡す！
	Bloom::GetInstance()->Render(commandList, colorSrv, maskSrv);
	
	// 5-2. Bloomの最終結果のSRV番号をもらう
	uint32_t finalSrv = Bloom::GetInstance()->GetResultSrvIndex();
	// エディタマネージャーに「これが最終的なゲーム画面のSRVだよ！」と教えてあげる
	EditorManager::GetInstance()->SetGameViewSrvIndex(finalSrv);


	// 6. メイン画面（バックバッファ）への直接描画！
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = dxCommon->GetBackBufferRtvHandle();
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon->GetDsvHandle();
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	// 7. 最終的な結果を画面にドーン！と描く
	PipelineManager::GetInstance()->SetPostEffectPipeline(commandList, PostEffectType::None); // シェーダーはエフェクトなしのやつを使う
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, finalSrv); // Bloomの結果をSRVとしてセット
	commandList->DrawInstanced(3, 1, 0, 0); // 巨大な三角形を描いて全画面にテクスチャを貼る方式なので、頂点数は3でOK！



	// --- スプライト・UI描画 ---
	SpriteCommon::GetInstance()->PreDraw(commandList);
	if (sprite_) { sprite_->Draw(); }
	TextManager::GetInstance()->Draw();
	

}

void GamePlayScene::DrawDebugUI(){

#ifdef USE_IMGUI
	// 3Dオブジェクト、カメラ、パーティクルのUI
	Obj3dCommon::GetInstance()->DrawDebugUI();
	if ( camera_ ) { camera_->DrawDebugUI(); }
	if ( debugCamera_ ) { debugCamera_->DrawDebugUI(); }
	ParticleManager::GetInstance()->DrawDebugUI();


	TextManager::GetInstance()->DrawDebugUI();

	ImGui::Begin("Block Dissolve Test");

	// スライダーで 0.0(通常) 〜 1.0(消滅) を操作
	if ( ImGui::SliderFloat("ブロックの消滅度", &dissolveThreshold_, 0.0f, 1.0f) ) {
		if ( testObj_ ) {
			// スライダーを動かすと、このブロックの閾値だけが書き換わる
			testObj_->SetDissolveThreshold(dissolveThreshold_);
		}
	}

	// 便利なリセットボタン
	if ( ImGui::Button("元に戻す") ) {
		dissolveThreshold_ = 0.0f;
		if ( testObj_ ){

			testObj_->SetDissolveThreshold(0.0f);
		}
			
	}
	ImGui::SameLine();
	if ( ImGui::Button("完全に消す") ) {
		dissolveThreshold_ = 1.0f;
		if ( testObj_ ){
			testObj_->SetDissolveThreshold(1.0f);
		}
	}

	ImGui::End();

	if (skinnedObj_) {
		ImGui::Begin("Skinned Model Control");

		Vector3 pos = skinnedObj_->GetTranslation();
		if (ImGui::DragFloat3("Position", &pos.x, 0.1f)) { 
			skinnedObj_->SetTranslation(pos); 
		}

		Vector3 rot = skinnedObj_->GetRotation();
		if (ImGui::DragFloat3("Rotation", &rot.x, 0.05f)) { 
			skinnedObj_->SetRotation(rot); 
		}

		Vector3 scale = skinnedObj_->GetScale();
		if (ImGui::DragFloat3("Scale", &scale.x, 0.05f)) { 
			skinnedObj_->SetScale(scale); 
		}

		ImGui::Separator();
		ImGui::Text("--- Custom Animation ---");

		// タイムライン
		ImGui::SliderFloat("Time(sec)", &animCurrentTime_, 0.0f, 10.0f); // とりあえず最大10秒

		// 再生/停止ボタン
		if (ImGui::Button(isAnimPlaying_ ? "Stop" : "Play")) {
			isAnimPlaying_ = !isAnimPlaying_;
		}
		ImGui::SameLine();

		// キーフレーム追加ボタン
		if (ImGui::Button("Add Keyframe")) {
			myAnimTrack_.AddKeyframe(animCurrentTime_, pos, rot, scale);
		}

		// 記録されているキーフレーム数の表示
		ImGui::Text("Keyframes: %zu", myAnimTrack_.keyframes.size());

		ImGui::End();
	}

#endif

}

GamePlayScene::GamePlayScene(){}

GamePlayScene::~GamePlayScene(){}
// 終了
void GamePlayScene::Finalize(){
	

	object3ds_.clear();
	
	textures_.clear();
	depthStencilResource_.Reset();
}