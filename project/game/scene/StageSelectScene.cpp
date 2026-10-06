#include "StageSelectScene.h"
// --- ゲーム固有のファイル ---
#include "GamePlayScene.h"
#include "TitleScene.h"

// --- エンジン側のファイル ---
#include "engine/base/Input.h"
#include "engine/base/WindowProc.h"
#include "engine/scene/SceneManager.h"
#include "engine/sdf/SDFManager.h"
#include "engine/sdf/SDFText.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <string>

namespace {
	const char* kTitleMessage = "ステージせんたく";
	const char* kGuideMessage = "SPACE : ゲームへ   T : タイトルへ";
	const char* kFontAtlas = "jpdot";
	constexpr float   kTitleFontSize = 60.0f;
	constexpr float   kGuideFontSize = 34.0f;
	constexpr float   kTextThickness = 0.1f;
	constexpr Vector4 kTextColor { 1.0f, 0.97f, 0.88f, 1.0f };

	// jpdot は等幅（半角＝文字サイズの半分 / 全角＝文字サイズ）。中央寄せ用に幅を見積もる
	float EstimateTextWidth(const std::string& utf8Text, float fontSize){
		float width = 0.0f;
		for ( unsigned char character : utf8Text ) {
			if ( ( character & 0xC0 ) == 0x80 ) { continue; } // UTF-8 の2バイト目以降
			width += ( character < 0x80 ) ? fontSize * 0.5f : fontSize;
		}
		return width;
	}

	// 文字入力中（ImGui の入力欄）はキー操作を受け付けない
	bool IsTypingInEditor(){
#ifdef USE_IMGUI
		return ImGui::GetIO().WantTextInput;
#else
		return false;
#endif
	}

	std::unique_ptr<SDFText> CreateText(const char* message, float fontSize){
		auto text = std::make_unique<SDFText>();
		text->Initialize();
		text->SetText(message);
		text->SetFontSize(fontSize);
		text->SetOutlineWidth(0.2f);
		text->SetThickness(kTextThickness);
		text->SetColor(kTextColor);
		text->SetOutlineColor(kTextColor);
		return text;
	}
}

StageSelectScene::StageSelectScene(){
	features_.overlay2D = true; // 仮の案内文字を最終画像へ重ねる
}
StageSelectScene::~StageSelectScene(){
	OnFinalize();
}

void StageSelectScene::OnInitialize(){
	// 文字（アトラスは SDFManager が resources/sdf/ から自動ロードするので、ここではアイテムを作るだけ）
	titleText_ = CreateText(kTitleMessage, kTitleFontSize);
	guideText_ = CreateText(kGuideMessage, kGuideFontSize);
	sceneChanging_ = false;
}

void StageSelectScene::OnFinalize(){
	titleText_.reset();
	guideText_.reset();
}

void StageSelectScene::OnUpdate(){
	// 画面サイズが変わっても中央に出るよう、毎フレーム位置を合わせる
	const float width  = ( float ) WindowProc::GetInstance()->GetClientWidth();
	const float height = ( float ) WindowProc::GetInstance()->GetClientHeight();
	if ( titleText_ ) {
		titleText_->SetPosition(( width - EstimateTextWidth(kTitleMessage, kTitleFontSize) ) * 0.5f, height * 0.35f);
	}
	if ( guideText_ ) {
		guideText_->SetPosition(( width - EstimateTextWidth(kGuideMessage, kGuideFontSize) ) * 0.5f, height * 0.6f);
	}

	if ( sceneChanging_ || IsTypingInEditor() ) { return; }
	Input* input = Input::GetInstance();
	if ( input->Triggerkey(DIK_SPACE) ) {
		// ゲームプレイへ
		sceneChanging_ = true;
		SceneManager::GetInstance()->ChangeScene(std::make_unique<GamePlayScene>());
	} else if ( input->Triggerkey(DIK_T) ) {
		// タイトルへ戻る
		sceneChanging_ = true;
		SceneManager::GetInstance()->ChangeScene(std::make_unique<TitleScene>());
	}
}

void StageSelectScene::OnDrawOverlay2D(ID3D12GraphicsCommandList* commandList){
	SDFManager* sdfManager = SDFManager::GetInstance();
	if ( titleText_ ) { sdfManager->DrawTextItem(commandList, *titleText_, kFontAtlas); }
	if ( guideText_ ) { sdfManager->DrawTextItem(commandList, *guideText_, kFontAtlas); }
}
