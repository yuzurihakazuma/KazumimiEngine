#include "GameClearScene.h"

#include "Engine/Scene/SceneManager.h"
#include "Engine/Base/Input.h"
#include "Engine/Base/DirectXCommon.h"
#include "Engine/Utils/TextManager.h"

void GameClearScene::Initialize() {
    // クリア画面用のテキストを設定する
    TextManager::GetInstance()->Initialize();
    TextManager::GetInstance()->SetText("SceneMessage", "GAME CLEAR\n\nPRESS SPACE TO TITLE");
}

void GameClearScene::Update() {
    // スペースキーでタイトルへ戻る
    if (Input::GetInstance()->Triggerkey(DIK_SPACE)) {
        SceneManager::GetInstance()->ChangeScene("TITLE");
        return;
    }
}

void GameClearScene::Draw() {
    // テキストだけ描画する
    TextManager::GetInstance()->Draw();
}

void GameClearScene::Finalize() {
    // 表示テキストを消して終了する
    TextManager::GetInstance()->SetText("SceneMessage", "");
    TextManager::GetInstance()->Finalize();
}