#include "FloorEffectManager.h"
#include "engine/base/WindowProc.h"
#include "engine/utils/TextManager.h"

void FloorEffectManager::ActivateDebuff(const std::string &name) {
    isActive_ = true;
    debuffName_ = name;
}

void FloorEffectManager::DeactivateDebuff() {
    isActive_ = false;
    debuffName_ = "";
    TextManager::GetInstance()->SetText("FloorDebuffUI", ""); // 文字も消す
}

void FloorEffectManager::UpdateUI(bool isCardReady) {
    // アクティブじゃない時は何もしない
    if (!isActive_) {
        return;
    }

    // 表示するテキスト
    std::string displayText = "【" + debuffName_ + "】弱体化中";
    TextManager::GetInstance()->SetText("FloorDebuffUI", displayText);

    // 座標の計算
    float screenW = static_cast<float>(WindowProc::GetInstance()->GetClientWidth());
    float bgWidth = 390.0f;
    float bgHeight = 200.0f;
    float marginRight = 20.0f;
    float marginTop = 10.0f;

    float textPosX = screenW - bgWidth - marginRight;
    float textPosY = marginTop + bgHeight + 20.0f;

    // 攻撃カードの「Eキーで発動」が出ている場合はさらに下にずらす
    if (isCardReady) { textPosY += 30.0f; }

    TextManager::GetInstance()->SetPosition("FloorDebuffUI", textPosX, textPosY);
}