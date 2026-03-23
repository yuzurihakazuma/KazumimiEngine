#include "SceneManager.h"
// --- 標準ライブラリ ---
#include <cassert>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif
// --- エンジン側のファイル ---
#include "engine/scene/IScene.h"
#include "engine/scene/AbstractSceneFactory.h"
#include "engine/base/DirectXCommon.h"
#include "engine/utils/ImGuiManager.h"
#include "engine/utils/Level/LevelEditor.h"
#include "engine/postEffect/PostEffect.h"
#include "engine/graphics/SrvManager.h"
#include "engine/base/Input.h"
#include "Bloom.h"


// シングルトンクラスの実装
SceneManager* SceneManager::GetInstance(){
	static SceneManager instance;
	return &instance;
}
// デストラクタ
SceneManager::~SceneManager(){
	// 現在のシーンを終了
	if ( currentScene_ ) {
		// 現在のシーンを終了
		currentScene_->Finalize();
		// シーン情報をクリア
		currentScene_ = nullptr;
	}
}
// シーンマネージャーの更新
void SceneManager::Update(){
	// 次のシーンへの切り替え処理
	if ( nextScene_ ) {
		// 現在のシーンを終了
		if ( currentScene_ ) {
			currentScene_->Finalize();
		}
		// シーンを切り替え
		currentScene_ = std::move(nextScene_);
		// 次のシーン情報をクリア
		nextScene_ = nullptr;
		DirectXCommon* dxCommon = DirectXCommon::GetInstance();

		dxCommon->BeginCommandRecording();

		currentScene_->Initialize();
		dxCommon->EndCommandRecording();
	}
	if (currentScene_) {
		auto updateStartTime = std::chrono::steady_clock::now();
		
		currentScene_->Update();

		auto updateEndTime = std::chrono::steady_clock::now();
		cpuUpdateTimeMs_ = std::chrono::duration<float, std::milli>(updateEndTime - updateStartTime).count();
		

	}
#ifdef USE_IMGUI
	Input* input = Input::GetInstance();
	if ( input->Triggerkey(DIK_F1) ) {
		isEditorActive_ = !isEditorActive_;
	}

	if ( isEditorActive_ ) {
		// 1. 全画面の透明なドッキング土台（変更なし）
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

		ImGui::Begin("MasterDockSpace", nullptr, window_flags);
		ImGui::PopStyleColor();
		ImGui::PopStyleVar(3);
		ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
		ImGui::End();

		// 2. 🎮 ゲーム画面（Game View）ウィンドウ
		ImGui::Begin("Game View");
		ImVec2 sceneSize = ImGui::GetContentRegionAvail();

		// 🚨 安全装置：サイズが潰れていたら最低サイズを保証する！
		if ( sceneSize.x < 10.0f ) sceneSize.x = 640.0f;
		if ( sceneSize.y < 10.0f ) sceneSize.y = 360.0f;

		uint32_t srvIndex = PostEffect::GetInstance()->GetSrvIndex();
		D3D12_GPU_DESCRIPTOR_HANDLE textureHandle = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex);

		// 🚨 D3D12の安全なキャスト方法（uintptr_t を経由する）
		ImGui::Image(( ImTextureID ) ( uintptr_t ) textureHandle.ptr, sceneSize);
		ImGui::End();

		// 3. 全シーン共通のUI
		PostEffect::GetInstance()->DrawDebugUI();

		// 4. 現在のシーン固有のUI
		if ( currentScene_ ) {
			currentScene_->DrawDebugUI();
		}
		ImGui::Begin("パフォーマンスモニター");

		float fps = ImGui::GetIO().Framerate;
		ImGui::Text("FPS: %.1f", fps);
		ImGui::Text("フレーム時間: %.3f ms", 1000.0f / fps);

		ImGui::Separator();

		ImGui::Text("[CPU] 更新処理(Update) : %.3f ms", cpuUpdateTimeMs_);
		ImGui::Text("[CPU] 描画準備(Draw)   : %.3f ms", cpuDrawTimeMs_);

		ImGui::Separator();

		// ボトルネック（重い原因）の自動診断
		float totalCpuTime = cpuUpdateTimeMs_ + cpuDrawTimeMs_;
		if (fps < 55.0f) {
			ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), " 警告: 処理落ちが発生しています！");
			if (totalCpuTime > 16.0f) {
				ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), " 原因: CPUの処理が重いです\n（計算やループ処理が多すぎます）");
			}
			else {
				ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), " 原因: GPUの処理が重いです\n（描画する量が多すぎるか、シェーダーが重いです）");
			}
		}
		else {
			ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), " 快適に動作しています！ (60 FPS維持)");
		}

		ImGui::End();

	}
#endif


}
// シーンマネージャーの描画
void SceneManager::Draw(){
	if ( currentScene_ ) {

		auto drawStartTime = std::chrono::steady_clock::now();
		// 現在のシーンを描画
		currentScene_->Draw();

		auto drawEndTime = std::chrono::steady_clock::now();
		cpuDrawTimeMs_ = std::chrono::duration<float, std::milli>(drawEndTime - drawStartTime).count();
	}
}

// 文字列を使ってファクトリーにシーンを作ってもらう
void SceneManager::ChangeScene(const std::string& sceneName) {
	assert(sceneFactory_ && "SceneManager: SceneFactory is not set!");

	// ファクトリーが unique_ptr を作ってくれるので、そのまま受け取る
	nextScene_ = sceneFactory_->CreateScene(sceneName);
}

// シーン変更（シーン名で指定）
void SceneManager::ChangeScene(std::unique_ptr<IScene> nextScene){
	nextScene_ = std::move(nextScene);
}


void SceneManager::Finalize() {
	// 現在のシーンを破棄する
	if (currentScene_) {	
		currentScene_->Finalize();
		currentScene_.reset();    
	}
}