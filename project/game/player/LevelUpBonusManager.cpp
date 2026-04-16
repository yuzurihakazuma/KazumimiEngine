#include "LevelUpBonusManager.h"
#include "game/player/PlayerManager.h"
#include "game/card/HandManager.h"
#include "engine/base/Input.h"
#include "engine/2d/Sprite.h"
#include "engine/utils/TextManager.h"
#include <stdlib.h>
#include <WindowProc.h>

void LevelUpBonusManager::Initialize() {
    isSelecting_ = false;
	previousPlayerLevel_ = 1;
    currentSelectedChoice_ = Choice::IncreaseMaxHandSize;

    //  暗転用の黒スプライトを作成（背景）
    blackOverlay_ = Sprite::Create("resources/white1x1.png", { 0.0f, 0.0f });
    if (blackOverlay_) {
        // 画面全体を覆うサイズ（適当に大きく設定）
        blackOverlay_->SetSize({ 4000.0f, 4000.0f });
        blackOverlay_->SetColor({ 0.0f, 0.0f, 0.0f, 0.7f }); // 半透明の黒
    }

    // --- 選択肢1: 手札上限アップ (画面左側) ---
    // 画像がある場合は "resources/bonus_limit.png" などに書き換えてください。
    choiceVisuals_[(int)Choice::IncreaseMaxHandSize] = Sprite::Create("resources/UI/Change.png", { 0.0f, 0.0f });
    if (choiceVisuals_[(int)Choice::IncreaseMaxHandSize]) {
        choiceVisuals_[(int)Choice::IncreaseMaxHandSize]->SetAnchorPoint({ 0.5f, 0.5f }); // 中心基準
        choiceVisuals_[(int)Choice::IncreaseMaxHandSize]->SetPosition({ 340.0f, 250.0f }); // 中央から少し左
    }

    // --- 選択肢2: カード獲得 (画面右側) ---
    // 画像がある場合は "resources/bonus_card.png" などに書き換えてください。
    choiceVisuals_[(int)Choice::GetRandomCard] = Sprite::Create("resources/UI/LevelC.png", { 0.0f, 0.0f });
    if (choiceVisuals_[(int)Choice::GetRandomCard]) {
        choiceVisuals_[(int)Choice::GetRandomCard]->SetAnchorPoint({ 0.5f, 0.5f });
        choiceVisuals_[(int)Choice::GetRandomCard]->SetPosition({ 740.0f, 210.0f }); // 中央から少し右
    }

   // テキストUI
    UISprite_ = Sprite::Create("resources/UI/LevelUpUi.png", { 0.0f,0.0f });
    if (UISprite_) {
        UISprite_->SetAnchorPoint({ 0.5f,0.5f });
		UISprite_->SetPosition({ 640.0f, 360.0f }); // 画面中央
    }
   
}

void LevelUpBonusManager::Reset(int currentLevel) {
    isSelecting_ = false;
	previousPlayerLevel_ = currentLevel;
}

LevelUpResult LevelUpBonusManager::Update(PlayerManager *playerManager, HandManager *handManager, Input *input) {
    LevelUpResult finalResult = { false, { -1, "", 0 } };

    if (!playerManager || !handManager) {
        return finalResult;
    }

    int currentLevel = playerManager->GetLevel();

    if (!isSelecting_) {
        // 通常時：レベルアップを検知
        if (currentLevel > previousPlayerLevel_) {
            isSelecting_ = true;
            currentSelectedChoice_ = Choice::IncreaseMaxHandSize; // デフォルトは枠増加

            // 記憶しているレベルを更新（ApplyBonus後に再度Updateが呼ばれた際に、まだレベルが残っていれば再度Selectingに移行する）
            previousPlayerLevel_++;
        }
    } else {
        // 選択画面表示中：入力処理（A/Dキー または 左右矢印キー）
        if (input->Triggerkey(DIK_A) || input->Triggerkey(DIK_LEFT)) {
            currentSelectedChoice_ = Choice::IncreaseMaxHandSize;
            // 必要ならここにカーソル移動SEなどを鳴らす
        }
        if (input->Triggerkey(DIK_D) || input->Triggerkey(DIK_RIGHT)) {
            currentSelectedChoice_ = Choice::GetRandomCard;
        }

        // 決定（スペースキー）
        if (input->Triggerkey(DIK_SPACE)) {
            // 選択されたボーナスを適用
            finalResult = ApplyBonus(handManager, currentSelectedChoice_);

            // 選択画面を閉じる
            isSelecting_ = false;
        }
    }

    return finalResult;
}

void LevelUpBonusManager::Draw() {
    if (!isSelecting_) {
        return;
    }

    // 今の画面サイズを取得する
    float screenW = static_cast<float>(WindowProc::GetInstance()->GetClientWidth());
    float screenH = static_cast<float>(WindowProc::GetInstance()->GetClientHeight());
    float centerX = screenW * 0.5f;
    float centerY = screenH * 0.5f;

    // 1. 暗転幕（背景）を描画
    if (blackOverlay_) {
        blackOverlay_->SetPosition({ centerX, centerY });
        blackOverlay_->SetSize({ screenW, screenH });
        blackOverlay_->Update();
        blackOverlay_->Draw();
    }

    // 選択肢の描画
    for (int i = 0; i < (int)Choice::Count; ++i) {
        if (choiceVisuals_[i]) {
            // 画面横幅の 20% 程度を中央からオフセットさせる
            float offsetX = screenW * 0.15f;
            Vector2 basePos;

            if (i == (int)Choice::IncreaseMaxHandSize) {
                basePos = { centerX - offsetX, centerY }; // 左側
            } else {
                basePos = { centerX + offsetX, centerY }; // 右側
            }

            // 選択中の演出（大きさや位置の微調整）
            if (i == (int)currentSelectedChoice_) {
                choiceVisuals_[i]->SetSize({ 264.0f, 396.0f });
                choiceVisuals_[i]->SetPosition({ basePos.x, basePos.y - 10.0f });
            } else {
                choiceVisuals_[i]->SetSize({ 240.0f, 360.0f });
                choiceVisuals_[i]->SetPosition(basePos);
            }

            choiceVisuals_[i]->Update();
            choiceVisuals_[i]->Draw();
        }
    }

    // UIスプライトの描画
    if (UISprite_) {
        UISprite_->SetPosition({ centerX, screenH * 0.5f });
        UISprite_->Update();
        UISprite_->Draw();
    }
    
}

LevelUpResult LevelUpBonusManager::ApplyBonus(HandManager *handManager, Choice choice) {
    LevelUpResult result = { false, { -1, "", 0 } };

    if (choice == Choice::IncreaseMaxHandSize) {
        // ① 手札の最大枚数を1増やす (安全のために上限10までにする)
        if (handManager->GetMaxHandSize() < 10) {
            handManager->IncreaseMaxHandSize(1);
        } else {
            // 上限に達していたら、代わりにカード獲得にする、などの処理。
            // 選択肢に出ている以上、無効にするのは不親切なので、強制的にカード獲得にする例
            choice = Choice::GetRandomCard;
        }
    }

    // ①の処理でchoiceがGetRandomCardに変わった場合も含めて処理する
    if (choice == Choice::GetRandomCard) {
        // ② ランダムなカードを1枚獲得する
        int randomId = 2 + (rand() % 11);
        Card randomCard = CardDatabase::GetCardData(randomId);

        // 手札に追加してみて、失敗したら交換モード用のフラグを立てる
        if (!handManager->AddCard(randomCard)) {
            result.needCardSwap = true;
            result.droppedCard = randomCard;
        }
    }

    return result;
}
