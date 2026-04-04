#include "LevelUpBonusManager.h"
#include "game/player/PlayerManager.h"
#include "game/card/HandManager.h"
#include "engine/base/Input.h"
#include "engine/2d/Sprite.h"
#include "engine/utils/TextManager.h"
#include <stdlib.h>

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
    choiceVisuals_[(int)Choice::IncreaseMaxHandSize] = Sprite::Create("resources/white1x1.png", { 0.0f, 0.0f });
    if (choiceVisuals_[(int)Choice::IncreaseMaxHandSize]) {
        // カードっぽいサイズ感にする
        choiceVisuals_[(int)Choice::IncreaseMaxHandSize]->SetSize({ 200.0f, 300.0f });
        choiceVisuals_[(int)Choice::IncreaseMaxHandSize]->SetAnchorPoint({ 0.5f, 0.5f }); // 中心基準
        choiceVisuals_[(int)Choice::IncreaseMaxHandSize]->SetPosition({ 340.0f, 250.0f }); // 中央から少し左
        choiceVisuals_[(int)Choice::IncreaseMaxHandSize]->SetColor({ 0.3f, 0.3f, 1.0f, 1.0f }); // とりあえず青色
    }

    // --- 選択肢2: カード獲得 (画面右側) ---
    // 画像がある場合は "resources/bonus_card.png" などに書き換えてください。
    choiceVisuals_[(int)Choice::GetRandomCard] = Sprite::Create("resources/white1x1.png", { 0.0f, 0.0f });
    if (choiceVisuals_[(int)Choice::GetRandomCard]) {
        choiceVisuals_[(int)Choice::GetRandomCard]->SetSize({ 200.0f, 300.0f });
        choiceVisuals_[(int)Choice::GetRandomCard]->SetAnchorPoint({ 0.5f, 0.5f });
        choiceVisuals_[(int)Choice::GetRandomCard]->SetPosition({ 740.0f, 210.0f }); // 中央から少し右
        choiceVisuals_[(int)Choice::GetRandomCard]->SetColor({ 1.0f, 0.3f, 0.3f, 1.0f }); // とりあえず赤色
    }

    // テキストの位置（タイトルのみ上部に表示）
    TextManager::GetInstance()->SetPosition("LevelUpTitle", 640.0f, 100.0f); // 中央上部

    // 選択肢の中身を説明する文字の位置（各スプライトの少し下）
    TextManager::GetInstance()->SetPosition("ChoiceLimitDesc", 1280.0f / 2.0f - 200.0f, 720.0f / 2.0f + 180.0f);
    TextManager::GetInstance()->SetPosition("ChoiceCardDesc", 1280.0f / 2.0f + 200.0f, 720.0f / 2.0f + 180.0f);

    // ゲーム開始時はテキストを空( "") にして非表示にしておく！
    TextManager::GetInstance()->SetText("LevelUpTitle", "");
    TextManager::GetInstance()->SetText("ChoiceLimitDesc", "");
    TextManager::GetInstance()->SetText("ChoiceCardDesc", "");
}

void LevelUpBonusManager::Reset() {
    isSelecting_ = false;
	previousPlayerLevel_ = 1;
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
        if (input->Triggerkey(DIK_RETURN)) {
            // 選択されたボーナスを適用
            finalResult = ApplyBonus(handManager, currentSelectedChoice_);

            // 選択画面を閉じて、文字を非表示にする
            isSelecting_ = false;
            TextManager::GetInstance()->SetText("LevelUpTitle", "");
            TextManager::GetInstance()->SetText("ChoiceLimitDesc", "");
            TextManager::GetInstance()->SetText("ChoiceCardDesc", "");
        }
    }

    return finalResult;
}

void LevelUpBonusManager::Draw() {
    if (!isSelecting_) {
        return;
    }

    // 1. 暗転幕（背景）を描画
    if (blackOverlay_) {
        blackOverlay_->Update();
        blackOverlay_->Draw();
    }

    // ★2. 選択肢のスプライトを描画
    for (int i = 0; i < (int)Choice::Count; ++i) {
        if (choiceVisuals_[i]) {
            // 現在カーソルが合っている選択肢を強調する
            if (i == (int)currentSelectedChoice_) {
                choiceVisuals_[i]->SetSize({ 240.0f, 360.0f }); // 少し大きく
                // 色を明るく（本来の色）
                if (i == (int)Choice::IncreaseMaxHandSize) {
                    // 大きくなった分、左上に座標をズラして中心を合わせる
                    choiceVisuals_[i]->SetPosition({ 490.0f, 360.0f });
                    choiceVisuals_[i]->SetColor({ 0.6f, 0.6f, 1.0f, 1.0f }); // 明るい青
                } else {
                    choiceVisuals_[i]->SetPosition({ 890.0f, 360.0f });
                    choiceVisuals_[i]->SetColor({ 1.0f, 0.6f, 0.6f, 1.0f }); // 選択中は明るい赤
                }
            } else {
                choiceVisuals_[i]->SetSize({ 200.0f, 300.0f }); // 通常サイズ
                // 選択されていない方は暗くする（グレーがかった色に）
                if (i == (int)Choice::IncreaseMaxHandSize) {
                    choiceVisuals_[i]->SetPosition({ 550.0f, 390.0f }); // 元の座標
                    choiceVisuals_[i]->SetColor({ 0.2f, 0.2f, 0.5f, 1.0f }); // 非選択時は暗い青
                } else {
                    choiceVisuals_[i]->SetPosition({ 950.0f, 390.0f }); // 元の座標
                    choiceVisuals_[i]->SetColor({ 0.5f, 0.2f, 0.2f, 1.0f }); // 非選択時は暗い赤
                }
            }

            choiceVisuals_[i]->Update();
            choiceVisuals_[i]->Draw();
        }
    }

    // 3. テキストを設定（DrawはGamePlaySceneで行われる）
    TextManager::GetInstance()->SetText("LevelUpTitle", "LEVEL UP! ボーナスを選択");

    // スプライトだけだと何かわからないので、説明文を足しておく
    TextManager::GetInstance()->SetText("ChoiceLimitDesc", "手札の上限+1");
    TextManager::GetInstance()->SetText("ChoiceCardDesc", "カード獲得");
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
