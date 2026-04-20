#pragma once
#include <string>

class FloorEffectManager {
public:
    // デバフを発動する
    void ActivateDebuff(const std::string &name);

    // デバフを解除する（次のフロアに行った時など）
    void DeactivateDebuff();

    // 毎フレームのUI更新（引数で「攻撃カードを構えているか」を受け取る）
    void UpdateUI(bool isCardReady);

    // デバフ中かどうかを他のクラスに教える
    bool IsDebuffActive() const { return isActive_; }

private:
    bool isActive_ = false;
    std::string debuffName_ = "";
};