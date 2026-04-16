#include "GameClearScene.h"

#include "Engine/Scene/SceneManager.h"
#include "Engine/Base/Input.h"
#include "Engine/Base/DirectXCommon.h"
#include "Engine/Base/WindowProc.h"
#include "Engine/Utils/TextManager.h"


#include "Engine/Graphics/PipelineManager.h"
#include "engine/graphics/SrvManager.h"
#include "engine/postEffect/PostEffect.h"
#include "Bloom.h"

void GameClearScene::Initialize() {
    // クリア画面用のテキストを設定する
    TextManager::GetInstance()->Initialize();
    const float screenW = static_cast<float>(WindowProc::GetInstance()->GetClientWidth());
    const float screenH = static_cast<float>(WindowProc::GetInstance()->GetClientHeight());
    TextManager::GetInstance()->SetPosition("SceneMessage", screenW * 0.5f, screenH * 0.5f);
    TextManager::GetInstance()->SetCentered("SceneMessage", true);
    TextManager::GetInstance()->SetText("SceneMessage", "GAME CLEAR\n\nPRESS SPACE TO TITLE");
}

void GameClearScene::Update() {
    // スペースキーでタイトルへ戻る
    if (Input::GetInstance()->Triggerkey(DIK_SPACE)) {
        SceneManager::GetInstance()->ChangeScene("TITLE");
        return;
    }
}

void GameClearScene::Draw() { // ※GameClearSceneの場合は GameClearScene::Draw() にしてください
	auto dxCommon = DirectXCommon::GetInstance();
	auto commandList = dxCommon->GetCommandList();

	// 1. MRT開始 (これで背景がクリアされる)
	PostEffect::GetInstance()->PreDrawSceneMRT(commandList);

	// ※3Dモデルを置きたい場合はここに描画処理を追加

	// 2. MRT終了
	PostEffect::GetInstance()->PostDrawSceneMRT(commandList);

	// 3. ポストエフェクト適用
	PostEffect::GetInstance()->Draw(commandList);

	// 4. Bloomパス
	uint32_t colorSrv = PostEffect::GetInstance()->GetSrvIndex();
	uint32_t maskSrv = PostEffect::GetInstance()->GetMaskSrvIndex();
	Bloom::GetInstance()->Render(commandList, colorSrv, maskSrv);
	uint32_t finalSrv = Bloom::GetInstance()->GetResultSrvIndex();

	// 5. バックバッファへ最終出力
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = dxCommon->GetBackBufferRtvHandle();
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon->GetDsvHandle();
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
	PipelineManager::GetInstance()->SetPostEffectPipeline(commandList, PostEffectType::None);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, finalSrv);
	commandList->DrawInstanced(3, 1, 0, 0);

	// 6. テキスト描画 (バックバッファに直接描かれる)
	TextManager::GetInstance()->Draw();
}

void GameClearScene::Finalize() {
    // 表示テキストを消して終了する
    TextManager::GetInstance()->SetText("SceneMessage", "");
    TextManager::GetInstance()->Finalize();
}
