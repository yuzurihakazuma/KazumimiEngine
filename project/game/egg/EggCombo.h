#pragma once
// =====================================================================
//  EggCombo：卵の「奥投げ」と跳ね返りコンボ・かくれ花（収集）
//   ・構え中のカーソルを背景の木・柵・岩・看板・花に合わせると、その物をロックする（AimThrowController が聞く）
//   ・離すと、卵が奥のその物へ飛ぶ。当たると、近くの別の背景へ跳ね返って次々に当たる（コンボ）
//       当たるたびに、その物がゆれて葉っぱ（木くず）が散り、初めて当てた物からはコインが出る（コンボが続くほど多い）
//       跳ね返る先が無くなる（または最大数）と、卵はプレイヤーの所へ戻ってきて割れる
//   ・一部の木には「かくれ花」が隠れている。当てると飛び出して集まる（ステージの収集要素。数は HUD に出る）
//  背景は StageBackdrop の物をそのまま使う（新しく置く物は無い。背景エディタで置いた木もそのまま的になる）
// =====================================================================
#include "engine/math/struct.h"

#include <string>
#include <vector>

class Camera;
class Egg;
class EggSystem;
class CoinSystem;
class StageBackdrop;

class EggCombo {
public:
    void SetContext(StageBackdrop* backdrop, EggSystem* eggs, CoinSystem* coins){
        backdrop_ = backdrop; eggs_ = eggs; coins_ = coins;
    }

    // プレイ開始・やりなおし・編集へ戻る：コンボ・当てた印・かくれ花を戻し、ゆれている物を元の形へ
    void Reset();

    // 構え中：カーソル（画面の画素）の先にある的を探す。見つかれば的の番号と、狙う点・画面の位置・画面での半径
    struct Pick { int target = -1; Vector3 center {}; float screenX = 0.0f, screenY = 0.0f, screenRadius = 0.0f; };
    bool PickForAim(const Camera& camera, float cursorX, float cursorY, float screenWidth, float screenHeight,
                    const Vector3& playerPos, Pick& out);
    // 投げた卵を的へ向かわせる（AimThrowController が投げた直後に呼ぶ）
    void Launch(Egg* egg, int target);
    // 狙ってほしいゲームの物（反転スイッチなど）。カーソルがこれに重なっている間は背景をロックしない（毎フレーム渡す）
    struct Blocker { Vector3 center {}; float radius = 1.0f; };
    void SetAimBlockers(const std::vector<Blocker>& blockers){ blockers_ = blockers; }

    // 毎フレーム（卵の移動の後）：当たり・跳ね返り・ゆれ・表示の残り時間
    void Update(const Vector3& playerPos, float dt);

    // HUD 用
    int   ComboCount() const{ return shownCombo_; }   // 今見せているコンボ数（0＝出さない）
    float ComboAlpha() const;                          // コンボの札の濃さ（最後に当ててから時間で薄くなる）
    float ComboPop() const{ return comboPop_; }        // 当てた瞬間 1 → 0（札をぽんと大きく）
    int   FlowersFound() const{ return flowersFound_; }
    int   FlowersTotal() const{ return flowersTotal_; }
    float FlowerPop() const{ return flowerPop_; }

private:
    struct Target {
        int     item = -1;        // StageBackdrop の番号
        Vector3 center {};        // 当たりの中心（木は葉の所）
        float   radius = 1.0f;
        bool    tree = false;
        bool    flower = false;   // かくれ花が隠れている
        bool    rewarded = false; // このプレイでもう当てた（コインは最初の1回だけ）
    };
    struct Chain {
        Egg*  egg = nullptr;
        int   target = -1;        // 今向かっている的（targets_ の番号）
        int   count = 0;          // このコンボで当てた数
        std::vector<int> visited;
        bool  returning = false;  // 最後：プレイヤーの所へ戻ってくる途中
        float timer = 0.0f;
    };
    struct Wobble { int item = -1; float t = 0.0f; Vector3 baseRot {}; Vector3 baseScale {}; };

    void BuildTargets();
    void Hit(Chain& chain, const Vector3& at, const Vector3& playerPos);
    int  NextTarget(const Chain& chain, const Vector3& from) const;
    void StartWobble(int item);
    void UpdateWobbles(float dt);
    void RestoreWobbles();
    static bool IsTargetModel(const std::string& model, bool& outTree);

    StageBackdrop* backdrop_ = nullptr;
    EggSystem*     eggs_ = nullptr;
    CoinSystem*    coins_ = nullptr;

    std::vector<Target> targets_;
    std::vector<Blocker> blockers_;
    size_t builtFor_ = 0;          // 的を作った時の背景の数（変わったら作り直す）
    std::vector<Chain> chains_;
    std::vector<Wobble> wobbles_;
    int   shownCombo_ = 0;
    float comboTimer_ = 0.0f;      // 最後に当ててからの秒
    float comboPop_ = 0.0f;
    int   flowersFound_ = 0;
    int   flowersTotal_ = 0;
    float flowerPop_ = 0.0f;
};
