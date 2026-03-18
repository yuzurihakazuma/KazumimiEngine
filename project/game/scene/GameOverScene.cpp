#include "GameOverScene.h"

#include "Engine/Scene/SceneManager.h"
#include "Engine/Base/Input.h"
#include "Engine/2D/Sprite.h"
#include "Engine/Graphics/TextureManager.h"
#include "Engine/Base/DirectXCommon.h"
#include "Engine/2D/SpriteCommon.h"

void GameOverScene::Initialize() {

    auto dxCommon = DirectXCommon::GetInstance();
    auto commandList = dxCommon->GetCommandList();

    // 仮のテクスチャ（あとで画像差し替えOK）
    auto tex = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/uvChecker.png", commandList);

    // 画面中央に表示
    sprite_ = std::unique_ptr<Sprite>(Sprite::Create(tex.srvIndex, { 640.0f, 360.0f }));

    // フルスクリーンサイズにする
    sprite_->SetSize({ 1280.0f, 720.0f });
    sprite_->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f }); // 黒背景
}

void GameOverScene::Update() {

    // スペースでタイトルへ戻る
    if (Input::GetInstance()->Triggerkey(DIK_SPACE)) {
        SceneManager::GetInstance()->ChangeScene("TITLE");
    }
}

void GameOverScene::Draw() {

    auto commandList = DirectXCommon::GetInstance()->GetCommandList();

    SpriteCommon::GetInstance()->PreDraw(commandList);

    if (sprite_) {
        sprite_->Draw();
    }
}

void GameOverScene::Finalize() {
    sprite_.reset();
}