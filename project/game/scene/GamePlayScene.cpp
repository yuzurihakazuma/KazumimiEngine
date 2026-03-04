#include "GamePlayScene.h"
// --- ゲーム固有のファイル ---
#include "TitleScene.h"

// --- エンジン側のファイル ---
#include "Engine/Math/Matrix4x4.h"
#include "Engine/Utils/ImGuiManager.h"
#include "Engine/Utils/Color.h"
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

	// 球モデル作成 (シングルトン)
	ModelManager::GetInstance()->CreateSphereModel("sphere", 16);
	// パーティクルグループ作成 (シングルトン)
	ParticleManager::GetInstance()->CreateParticleGroup("Circle", "resources/uvChecker.png");

	// テクスチャ読み込み
	textures_["uvChecker"] = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/uvChecker.png", commandList);
	textures_["monsterBall"] = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/monsterBall.png", commandList);
	textures_["fence"] = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/fence.png", commandList);
	textures_["circle"] = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/circle.png", commandList);

	
	// カメラ生成
	camera_ = std::make_unique<Camera>(windowProc->GetClientWidth(), windowProc->GetClientHeight(), dxCommon);
	camera_->SetTranslation({ 0.0f, 2.0f, -15.0f });
	
	// デバッグカメラ生成
	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize();


	// オブジェクト生成
	fence_ = std::make_unique<Obj3d>();
	sphere_ = std::make_unique<Obj3d>();
	ground_ = std::make_unique<Obj3d>();

	// モデル取得 (シングルトンから)
	Model* modelFence = ModelManager::GetInstance()->FindModel("fence");
	Model* modelSphere = ModelManager::GetInstance()->FindModel("sphere");
	Model* modelGround = ModelManager::GetInstance()->FindModel("grass");


	// オブジェクト生成＆初期化
	fence_ = Obj3d::Create("fence");
	if (fence_) {
		fence_->SetCamera(camera_.get());
		fence_->SetTranslation(fencePos_);
	}

	sphere_ = Obj3d::Create("sphere");
	if (sphere_) {
		sphere_->SetCamera(camera_.get());
		sphere_->SetTranslation(spherePos_);
		sphere_->SetScale(sphereScale_);
	}

	ground_ = Obj3d::Create("grass"); 
	if (ground_) {
		ground_->SetCamera(camera_.get());
		ground_->SetTranslation(groundPos_);
		ground_->SetScale(groundScale_);
	}


	// ファイル名を指定するだけで、読み込み・生成・配置
	// 引数: (ファイルパス, 座標)
	sprite_ = Sprite::Create(textures_["uvChecker"].srvIndex, spritePos_);

	// スプライトの切り抜き範囲を設定 (テクスチャの左上から128x128ピクセルを使用)
	sprite_->SetTextureRect(0.0f, 0.0f, 128.0f, 128.0f, 256.0f, 256.0f);


	// デプスステンシル作成 (TextureManagerシングルトン)
	depthStencilResource_ = TextureManager::GetInstance()->CreateDepthStencilTextureResource(
		windowProc->GetClientWidth(), windowProc->GetClientHeight()
	);

	postEffect_ = std::make_unique<PostEffect>();
	postEffect_->Initialize(
		dxCommon,
		SrvManager::GetInstance(),
		windowProc->GetClientWidth(),
		windowProc->GetClientHeight()
	);

}

void GamePlayScene::Update(){
	
	// デバッグカメラ更新
	if (isDebugCameraActive_ && debugCamera_) {
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

	// オブジェクト更新
	if (sphere_) {
		sphere_->SetTranslation(spherePos_);
		sphere_->SetScale(sphereScale_);
		sphere_->Update();
	}
	if (fence_) {
		fence_->SetTranslation(fencePos_); 
		fence_->SetScale(fenceScale_);
		fence_->Update();
	}
	// 地面の更新
	if (ground_) {
		ground_->SetTranslation(groundPos_);
		ground_->SetScale(groundScale_);
		ground_->Update();
	}

	// 全オブジェクト更新
	for ( auto& obj : object3ds_ ) {
		obj->Update();
	}
	// スプライト更新
	sprite_->Update();

#ifdef USE_IMGUI

	
	ImGui::Text("Current Scene: GamePlay");
	ImGui::Separator(); // 区切り線


	ImGui::Begin("Debug");
	// デバッグカメラの切り替え前の座標・回転を保存
	bool isPreDebugCameraActive = isDebugCameraActive_;

	ImGui::DragFloat3("Pos", &groundPos_.x, 0.1f);
	ImGui::DragFloat3("Scale", &groundScale_.x, 0.1f);
	ImGui::DragFloat3("FencePos", &fencePos_.x, 0.1f);
	ImGui::DragFloat3("FenceScale", &fenceScale_.x, 0.1f);

	ImGui::DragFloat3("SpherePos", &spherePos_.x, 0.1f);
	ImGui::DragFloat3("SphereScale", &sphereScale_.x, 0.1f);
	ImGui::Checkbox("Debug Camera Active", &isDebugCameraActive_);

	ImGui::Text("=== Collision Test ===");

	// 1. スフィアの当たり判定データを作成
	Sphere sphereCollider;
	sphereCollider.center = spherePos_;
	sphereCollider.radius = sphereScale_.x; // スケールのXを半径とする

	// 2. フェンスの当たり判定データを作成 (AABB)
	AABB fenceCollider;
	// 中心座標からサイズの半分を引いたものがMin(左下奥)、足したものがMax(右上手前)
	fenceCollider.min = {
		fencePos_.x - (fenceScale_.x / 2.0f),
		fencePos_.y - (fenceScale_.y / 2.0f),
		fencePos_.z - (fenceScale_.z / 2.0f)
	};
	fenceCollider.max = {
		fencePos_.x + (fenceScale_.x / 2.0f),
		fencePos_.y + (fenceScale_.y / 2.0f),
		fencePos_.z + (fenceScale_.z / 2.0f)
	};

	// 3. 判定チェック！
	bool isHit = Collision::IsCollision(sphereCollider, fenceCollider);

	// 4. 結果を色付きで表示
	if (isHit) {
		// 当たっていたら赤色で Hit!! と表示
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "HIT!!");
	}
	else {
		// 当たっていなければ緑色で Safe と表示
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Safe");
	}


	if (isDebugCameraActive_ && !isPreDebugCameraActive) {
		preDebugCameraPos_ = camera_->GetTransform().translate;
		preDebugCameraRot_ = camera_->GetTransform().rotate;
	}
	// デバッグカメラが非アクティブになったとき、切り替え前の座標・回転を復元
	if (!isDebugCameraActive_ && isPreDebugCameraActive) {
		camera_->SetTranslation(preDebugCameraPos_);
		camera_->SetRotation(preDebugCameraRot_);
	}

	if ( ImGui::TreeNode("Directional Light") ) {
		auto light = Obj3dCommon::GetInstance()->GetLightData();
		ImGui::DragFloat3("Direction", &light->direction.x, 0.01f);
		ImGui::ColorEdit3("Color", &light->color.x);
		ImGui::DragFloat("Intensity", &light->intensity, 0.01f, 0.0f, 10.0f);
		light->direction = Normalize(light->direction);
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Point Light")) {
		auto pLight = Obj3dCommon::GetInstance()->GetPointLightData();
		ImGui::DragFloat3("Position", &pLight->position.x, 0.01f);
		ImGui::ColorEdit3("Color", &pLight->color.x);
		ImGui::DragFloat("Intensity", &pLight->intensity, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat("Radius", &pLight->radius, 0.1f, 0.1f, 100.0f); 
		ImGui::DragFloat("Decay", &pLight->decay, 0.1f, 0.1f, 10.0f); 
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Spot Light")) {
		auto sLight = Obj3dCommon::GetInstance()->GetSpotLightData();
		ImGui::DragFloat3("Position", &sLight->position.x, 0.01f);
		ImGui::DragFloat3("Direction", &sLight->direction.x, 0.01f);
		sLight->direction = Normalize(sLight->direction); // 常に正規化
		ImGui::ColorEdit3("Color", &sLight->color.x);
		ImGui::DragFloat("Intensity", &sLight->intensity, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat("Distance", &sLight->distance, 0.1f, 0.1f, 100.0f);
		ImGui::DragFloat("Decay", &sLight->decay, 0.1f, 0.1f, 10.0f);
		ImGui::DragFloat("cosAngle", &sLight->cosAngle, 0.01f, -1.0f, 1.0f);
		ImGui::DragFloat("FalloffStart", &sLight->cosFalloffStart, 0.01f, -1.0f, 1.0f);
		ImGui::TreePop();
	}
	if ( ImGui::TreeNode("Camera") ) {
		Vector3 camPos = camera_->GetTransform().translate;
		Vector3 camRot = camera_->GetTransform().rotate;
		ImGui::DragFloat3("Position", &camPos.x, 0.1f);
		ImGui::DragFloat3("Rotation", &camRot.x, 0.01f);
		camera_->SetTranslation(camPos);
		camera_->SetRotation(camRot);
		ImGui::TreePop();
	}

	ImGui::Text("=== Particle Debug ===");
	size_t particleCount = ParticleManager::GetInstance()->GetParticleCount("Circle");
	uint32_t instanceCount = ParticleManager::GetInstance()->GetInstanceCount("Circle");
	ImGui::Text("Particles (CPU) : %zu", particleCount);
	ImGui::Text("Instances (GPU) : %u", instanceCount);

	if ( ImGui::Button("Emit 10") ) {
		ParticleManager::GetInstance()->Emit("Circle", { 0.0f, 5.0f, 0.0f }, 10);
	}
	ImGui::End();


	// 要件2: ウィンドウサイズを(500, 100)に固定
	ImGui::SetNextWindowSize(ImVec2(500, 100));

	// 要件1: 専用のImGuiウィンドウを作成
	ImGui::Begin("Sprite Setup");

	// 要件4: 小数第1位まで表示するため、最後の引数に "%.1f" を指定する！
	ImGui::DragFloat2("Position", &spritePos_.x, 0.1f, -2000.0f, 2000.0f,"% 06.1f");
	ImGui::End();


	if (sprite_) {
		sprite_->SetPosition(spritePos_);
		sprite_->Update();
	}

	// デバッグカメラ更新 (デバッグカメラがアクティブなときのみ、カメラの座標・回転を更新)
	if (isDebugCameraActive_ && debugCamera_) {
		debugCamera_->Update(camera_.get());
	}
	// カメラ更新
	camera_->Update();


	if ( !object3ds_.empty() ) {
		object3ds_[0]->SetTranslation(fencePos_);
	}
#endif


}

void GamePlayScene::Draw(){


	auto dxCommon = DirectXCommon::GetInstance();
	auto commandList = DirectXCommon::GetInstance()->GetCommandList();
	
	
	postEffect_->PreDrawScene(commandList, dxCommon);

	// 3D描画の前準備
	Obj3dCommon::GetInstance()->PreDraw(commandList);

	
	if (sphere_) {
		sphere_->Draw();
	}
	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Object3D_CullNone);
	
	if (fence_) {
		fence_->Draw();
	}

	if (ground_) {
		ground_->Draw();
	}
	// 3Dオブジェクト描画
	for ( auto& obj : object3ds_ ) {
		obj->Draw();
	}

	// パーティクル描画 (パイプライン切り替え)
	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Particle);
	ParticleManager::GetInstance()->Draw(commandList);
	
	postEffect_->PostDrawScene(commandList, dxCommon);


	// ポストエフェクトの描画
	postEffect_->Draw(commandList);
	
	// スプライト描画の前準備
	SpriteCommon::GetInstance()->PreDraw(commandList);
	
	if (sprite_) {
		sprite_->Draw();
	}
}

GamePlayScene::GamePlayScene(){}

GamePlayScene::~GamePlayScene(){}
// 終了
void GamePlayScene::Finalize(){
	

	object3ds_.clear();

	textures_.clear();
	depthStencilResource_.Reset();
}