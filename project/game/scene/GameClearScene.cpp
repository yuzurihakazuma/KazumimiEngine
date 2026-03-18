#include "GameClearScene.h"

#include "Engine/Scene/SceneManager.h"
#include "Engine/Base/Input.h"
#include "Engine/2D/Sprite.h"
#include "Engine/Graphics/TextureManager.h"
#include "Engine/Base/DirectXCommon.h"
#include "Engine/2D/SpriteCommon.h"

void GameClearScene::Initialize() {

    auto dxCommon = DirectXCommon::GetInstance();
    auto commandList = dxCommon->GetCommandList();

    auto tex = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/uvChecker.png", commandList);

    sprite_ = std::unique_ptr<Sprite>(Sprite::Create(tex.srvIndex, { 640.0f, 360.0f }));

    sprite_->SetSize({ 1280.0f, 720.0f });
    sprite_->SetColor({ 0.2f, 0.6f, 1.0f, 1.0f }); // 青っぽい背景
}

void GameClearScene::Update() {

    if (Input::GetInstance()->Triggerkey(DIK_SPACE)) {
        SceneManager::GetInstance()->ChangeScene("TITLE");
    }
}

void GameClearScene::Draw() {

    auto commandList = DirectXCommon::GetInstance()->GetCommandList();

    SpriteCommon::GetInstance()->PreDraw(commandList);

    if (sprite_) {
        sprite_->Draw();
    }
}

void GameClearScene::Finalize() {
    sprite_.reset();
}