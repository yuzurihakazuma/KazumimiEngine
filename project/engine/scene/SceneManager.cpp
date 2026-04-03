#include "SceneManager.h"
// --- 標準ライブラリ ---
#include <cassert>
// --- エンジン側のファイル ---
#include "engine/scene/IScene.h"
#include "engine/scene/AbstractSceneFactory.h"
#include "engine/base/DirectXCommon.h"
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

		auto updateStartTime = std::chrono::steady_clock::now();
		currentScene_->Update();

		auto updateEndTime = std::chrono::steady_clock::now();
		cpuUpdateTimeMs_ = std::chrono::duration<float, std::milli>(updateEndTime - updateStartTime).count();
		

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

void SceneManager::SetFirstScene(std::unique_ptr<IScene> scene) {
	// 既にシーンがある場合は何もしない
	if (currentScene_) {
		return;
	}

	currentScene_ = std::move(scene);

	if (currentScene_) {
		DirectXCommon* dxCommon = DirectXCommon::GetInstance();
		dxCommon->BeginCommandRecording();
		currentScene_->Initialize();
		dxCommon->EndCommandRecording();
	}

	// 最初のシーンなのでフェードは使わない
	nextScene_.reset();
	fadeAlpha_ = 0.0f;
	fadeWaitTimer_ = 0;
	fadeState_ = FadeState::None;
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
}

void SceneManager::DrawCurrentSceneDebugUI(){
	if ( currentScene_ ) {
		currentScene_->DrawDebugUI();
	}
}