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
#include "engine/2d/Sprite.h"
#include "engine/2d/SpriteCommon.h"
#include "engine/graphics/TextureManager.h"

// シングルトンクラスの実装
SceneManager* SceneManager::GetInstance() {
	static SceneManager instance;
	return &instance;
}

// デストラクタ
SceneManager::~SceneManager() {
	// 現在のシーンを終了
	if (currentScene_) {
		currentScene_->Finalize();
		currentScene_ = nullptr;
	}

	// フェード用スプライトを破棄
	fadeSprite_.reset();
}

// シーンマネージャーの更新
void SceneManager::Update() {
	// -----------------------------
	// フェードアウト処理
	// -----------------------------
	if (fadeState_ == FadeState::Out) {
		fadeAlpha_ += fadeSpeed_;

		if (fadeAlpha_ >= 1.0f) {
			fadeAlpha_ = 1.0f;

			// 真っ黒になったら待機状態へ移る
			fadeState_ = FadeState::Wait;
			fadeWaitTimer_ = fadeWaitTimeMax_;
		}
	}
	// -----------------------------
	// 真っ黒で停止する処理
	// -----------------------------
	else if (fadeState_ == FadeState::Wait) {
		fadeWaitTimer_--;

		if (fadeWaitTimer_ <= 0) {
			// 真っ黒時間が終わったらシーンを切り替える
			if (nextScene_) {
				// 現在のシーンを終了
				if (currentScene_) {
					currentScene_->Finalize();
				}

				// シーンを切り替え
				currentScene_ = std::move(nextScene_);
				nextScene_ = nullptr;

				DirectXCommon* dxCommon = DirectXCommon::GetInstance();
				dxCommon->BeginCommandRecording();

				// フェード用スプライトがまだ無ければここで作成する
				if (!fadeSprite_) {
					auto commandList = dxCommon->GetCommandList();

					TextureData tex = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/white1x1.png", commandList);

					fadeSprite_ = std::unique_ptr<Sprite>(Sprite::Create(tex.srvIndex, { 640.0f, 360.0f }));
					if (fadeSprite_) {
						fadeSprite_->SetSize({ 1280.0f, 720.0f });
						fadeSprite_->SetColor({ 0.0f, 0.0f, 0.0f, fadeAlpha_ });
						fadeSprite_->Update();
					}
				}

				currentScene_->Initialize();
				dxCommon->EndCommandRecording();
			}

			// 切り替え後はフェードインへ移る
			fadeState_ = FadeState::In;
		}
	}
	// -----------------------------
	// フェードイン処理
	// -----------------------------
	else if (fadeState_ == FadeState::In) {
		fadeAlpha_ -= fadeSpeed_;

		if (fadeAlpha_ <= 0.0f) {
			fadeAlpha_ = 0.0f;
			fadeState_ = FadeState::None;
		}
	}

	// フェードスプライトの色を更新
	if (fadeSprite_) {
		fadeSprite_->SetColor({ 0.0f, 0.0f, 0.0f, fadeAlpha_ });
		fadeSprite_->Update();
	}

	// 現在のシーンを更新
	if (currentScene_) {
		currentScene_->Update();
	}

#ifdef USE_IMGUI
	Input* input = Input::GetInstance();
	if (input->Triggerkey(DIK_F1)) {
		isEditorActive_ = !isEditorActive_;
	}

	if (isEditorActive_) {
		// 1. 全画面の透明なドッキング土台
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);

		ImGuiWindowFlags window_flags =
			ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
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

		// 2. ゲーム画面
		ImGui::Begin("Game View");
		ImVec2 sceneSize = ImGui::GetContentRegionAvail();

		// サイズが潰れていたら最低サイズを保証
		if (sceneSize.x < 10.0f) sceneSize.x = 640.0f;
		if (sceneSize.y < 10.0f) sceneSize.y = 360.0f;

		uint32_t srvIndex = PostEffect::GetInstance()->GetSrvIndex();
		D3D12_GPU_DESCRIPTOR_HANDLE textureHandle = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex);

		ImGui::Image((ImTextureID)(uintptr_t)textureHandle.ptr, sceneSize);
		ImGui::End();

		// 3. 全シーン共通のUI
		PostEffect::GetInstance()->DrawDebugUI();

		// 4. 現在のシーン固有のUI
		if (currentScene_) {
			currentScene_->DrawDebugUI();
		}
	}
#endif
}

// シーンマネージャーの描画
void SceneManager::Draw() {
	if (currentScene_) {
		// 現在のシーンを描画
		currentScene_->Draw();
	}

	// 最後にフェードを上から重ねる
	if (fadeSprite_) {
		auto commandList = DirectXCommon::GetInstance()->GetCommandList();
		SpriteCommon::GetInstance()->PreDraw(commandList);
		fadeSprite_->Draw();
	}
}

// 文字列を使ってファクトリーにシーンを作ってもらう
void SceneManager::ChangeScene(const std::string& sceneName) {
	assert(sceneFactory_ && "SceneManager: SceneFactory is not set!");

	// すでにフェード中なら新しいシーン変更を受け付けない
	if (fadeState_ != FadeState::None) {
		return;
	}

	// 次のシーンを予約してフェードアウト開始
	nextScene_ = sceneFactory_->CreateScene(sceneName);
	fadeState_ = FadeState::Out;
	fadeAlpha_ = 0.0f;
	fadeWaitTimer_ = 0;
}

// シーン変更（直接シーンを渡す）
void SceneManager::ChangeScene(std::unique_ptr<IScene> nextScene) {
	// すでにフェード中なら新しいシーン変更を受け付けない
	if (fadeState_ != FadeState::None) {
		return;
	}

	nextScene_ = std::move(nextScene);
	fadeState_ = FadeState::Out;
	fadeAlpha_ = 0.0f;
	fadeWaitTimer_ = 0;
}

void SceneManager::Finalize() {
	// 現在のシーンを破棄する
	if (currentScene_) {
		currentScene_->Finalize();
		currentScene_.reset();
	}

	// 次のシーンも破棄する
	nextScene_.reset();

	// フェード用スプライトを破棄する
	fadeSprite_.reset();

	// フェード状態を初期化する
	fadeAlpha_ = 0.0f;
	fadeWaitTimer_ = 0;
	fadeState_ = FadeState::None;
}