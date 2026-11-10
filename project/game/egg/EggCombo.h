#pragma once
// =====================================================================
//  EggCombo：卵の「奥投げ」と、はじけ飛ぶ背景の連鎖コンボ・かくれ花（卵コンボの手ごたえ 企画書）
//   ・構え中のカーソルを背景の木・柵・岩・看板・花に合わせると、その物をロックし、その物だけが光る（代役を光らせる）
//   ・投げた卵は、当たった所で割れる。当たった物は根元から外れて回りながら飛び、近くのまだ当てていない物に当たると
//       それもはじけて飛ぶ（コンボ +1。最大 8）。向かう先が無い物は、奥と上へ飛びながら溶けて消える
//   ・飛んだ物は数秒後に、元の場所にぽんと生え戻る（同じ所で何度でもコンボを試せる。コインは最初の1回だけ）
//   ・初めて当てた物からコイン（コンボが続くほど多い）。木の5本に1本くらいに「かくれ花」が隠れている（収集要素）
//  はじけ方はビリヤード：当たった所から物の真ん中へ向かう向きに飛ぶ。真ん中に当てればまっすぐ奥へ、
//   右のふちに当てれば左へ、左のふちに当てれば右へ。次の物は、その向きに一番合う物（9m 以内・±60°。無ければ ±100°）。
//   狙いは物の真ん中ではなく、カーソルが指している所（物の大きさの 8 割まで）。構え中は、はじける向きを紙の点で見せる。
//  コンボの的（combo_target）：コンボの始まりの印。tools/combo/make_combo_spots.py が、つながるように並べた「コンボの場所」を作る。
//   背景エディタで的を選ぶと、真ん中・左・右に当てた時のはじける順を Game View に線で見せる（DrawPreview）
//  勢い：はじけるたびに 1 割ずつ落ちる（届く距離・飛ぶ速さ・回る速さ）。はじけた向き（±60°）の届く所に次の物が無ければ、そこで終わる。
//   ×8 まで続くのは、物が向きに沿って並んでいる所だけ（tools/combo/make_combo_spots.py も同じ決め方で数える）
//  ×8（いちばん長いコンボ）のごほうび：♡が 1 つ戻る（SetOnMaxCombo）。コンボの的から始めた ×8 なら、その的の「コンボ花」が出る
//   （ステージの集める物。一度見つけたら StageRecords に残り、ワールドマップにも出る）
//  「代役」：背景は同じモデルをまとめて描いている（インスタンシング）ので、1つだけ光らせたり飛ばしたりできない。
//   その物を StageBackdrop::SetComboHidden で隠し、同じモデルの Obj3d（代役）を同じ姿勢で出して動かす。代役は先に作って使い回す
// =====================================================================
#include "engine/math/struct.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class Camera;
class Egg;
class EggSystem;
class CoinSystem;
class StageBackdrop;
class Obj3d;

class EggCombo {
public:
    EggCombo();
    ~EggCombo();

    void SetContext(StageBackdrop* backdrop, EggSystem* eggs, CoinSystem* coins){
        backdrop_ = backdrop; eggs_ = eggs; coins_ = coins;
    }
    void SetStage(const std::string& stage){ stage_ = stage; } // コンボ花の記録（StageRecords）に使う
    // ×8 を出した時（♡を戻すなど）
    void SetOnMaxCombo(std::function<void()> callback){ onMaxCombo_ = std::move(callback); }
    // 代役を先に作っておく（初めてのコンボで止まらないように。シーンの読み込みで呼ぶ）
    void Prewarm(uint32_t noiseSrv);

    // プレイ開始・やりなおし・編集へ戻る：飛んだ物を全部元の場所へ戻し、コンボ・かくれ花も戻す
    void Reset();

    // 構え中：カーソル（画面の画素）の先にある的を探す。見つかれば的の番号と、狙う点・画面の位置・画面での半径
    struct Pick {
        int target = -1; Vector3 center {}; float screenX = 0.0f, screenY = 0.0f, screenRadius = 0.0f;
        Vector3 aimPoint {};                 // 当てる所（カーソルが指す所。物の真ん中から大きさの 8 割まで）
        float   aimX = 0.0f, aimY = 0.0f;     // 当てる所の画面の位置
        Vector3 bounceDir {};                // その所に当てた時に、物がはじける向き（水平）
    };
    bool PickForAim(const Camera& camera, float cursorX, float cursorY, float screenWidth, float screenHeight,
                    const Vector3& playerPos, Pick& out);
    // 狙っている物を光らせる（構え中は毎フレーム。-1 で消す）。ロックした瞬間に一度だけぽんと大きくなる
    void SetAimHighlight(int target);
    // 投げた卵を的の aimPoint へ向かわせる（AimThrowController が投げた直後に呼ぶ）
    void Launch(Egg* egg, int target, const Vector3& aimPoint);
    // 背景エディタ：選んだ物（背景の番号）から、真ん中・左・右のふちに当てた時のはじける順を線で見せる（DebugDraw）
    void DrawPreview(const std::vector<int>& items, const Camera& camera);
    // 狙ってほしいゲームの物（反転スイッチなど）。カーソルがこれに重なっている間は背景をロックしない（毎フレーム渡す）
    struct Blocker { Vector3 center {}; float radius = 1.0f; };
    void SetAimBlockers(const std::vector<Blocker>& blockers){ blockers_ = blockers; }

    // 毎フレーム（卵の移動の後）：当たり・はじけ飛ぶ物・生え戻り・光り方
    void Update(const Vector3& playerPos, const Camera* camera, float dt);
    // カメラが決まった後：代役の行列（Obj3d::Update はその時のカメラを焼き込む）
    void UpdateVisuals();
    void Draw();

    // HUD 用
    int   ComboCount() const{ return shownCombo_; }   // 今見せているコンボ数（0＝出さない）
    float ComboAlpha() const;                          // コンボの札の濃さ（最後に当ててから時間で薄くなる）
    float ComboPop() const{ return comboPop_; }        // 当てた瞬間 1 → 0（札をぽんと大きく）
    int   FlowersFound() const{ return flowersFound_; }
    int   FlowersTotal() const{ return flowersTotal_; }
    float FlowerPop() const{ return flowerPop_; }
    int   ComboFlowersFound() const;                       // このステージで見つけたコンボ花（記録）
    int   ComboFlowersTotal() const{ return comboFlowerTotal_; }
    float ComboFlowerPop() const{ return comboFlowerPop_; }

private:
    enum class Kind { Tree, Fence, Rock, Sign, Flower, Bullseye };
    enum class State { Idle, Flying, Gone, Regrowing };
    struct Target {
        int     item = -1;        // StageBackdrop の番号
        Kind    kind = Kind::Tree;
        Vector3 center {};        // 当たりの中心（木は葉の所）
        Vector3 mid {};           // 箱の真ん中（飛ぶ時に回る中心）
        float   halfHeight = 0.5f;
        float   radius = 1.0f;
        bool    flower = false;   // かくれ花が隠れている
        bool    rewarded = false; // このプレイでもう当てた（コインと花は最初の1回だけ）
        State   state = State::Idle;
        float   timer = 0.0f;     // Gone：生え戻るまで / Regrowing：ぽんと出てからの秒
        int     proxy = -1;       // 生え戻りの代役
    };
    struct Chain { int count = 0; std::vector<int> visited; int start = -1; bool finished = false; };
    struct EggShot { Egg* egg = nullptr; int target = -1; int chain = -1; };
    struct Flyer {
        int     proxy = -1;
        int     target = -1;      // 飛んでいる物（targets_ の番号）
        int     aim = -1;         // 向かっている的（無ければ -1：溶けて消える）
        int     chain = -1;
        Vector3 pos {};           // 回る中心
        Vector3 prev {};
        Vector3 vel {};
        Vector3 rot {};           // ラジアン
        Vector3 spin {};
        float   t = 0.0f;
        float   flightTime = 1.0f;
        float   dissolveAt = -1.0f; // 溶け始めた時刻（-1＝まだ）
    };
    struct Proxy { std::unique_ptr<Obj3d> obj; int item = -1; };

    void BuildTargets();
    void Burst(int target, int chain, const Vector3& at, const Vector3& outDir);
    void Finale(const Chain& chain, const Vector3& at); // ×8：♡とコンボ花
    // from の物（targets_ の番号）から outDir の向きに一番合う次の物。live＝遊んでいる時（飛んでいる・隠れている物を除く）
    int  PickNext(int from, const Vector3& outDir, const std::vector<int>& visited, bool live) const;
    // 当たった所 contact から物の真ん中への向き（水平）。はじける向き
    static Vector3 BounceDir(const Vector3& center, const Vector3& contact, const Vector3& fallback);
    // 卵が from から aim へまっすぐ飛んだ時に、的の球に最初に触れる所
    static bool EntryPoint(const Vector3& from, const Vector3& to, const Vector3& center, float radius, Vector3& out);
    int  AcquireProxy(int target);
    void ReleaseProxy(int proxy, bool unhideItem);
    void PlaceProxy(int proxy, const Target& target, const Vector3& center, const Vector3& rot, float scale);
    void SpawnChips(const Target& target, const Vector3& at, int count);
    void ReleaseHighlight();
    float DepthOf(const Vector3& p) const;
    static Kind KindOf(const std::string& model, bool& outTarget);

    StageBackdrop* backdrop_ = nullptr;
    EggSystem*     eggs_ = nullptr;
    CoinSystem*    coins_ = nullptr;

    std::vector<Target>  targets_;
    size_t builtFor_ = 0;          // 的を作った時の背景の数（変わったら作り直す）
    std::vector<Blocker> blockers_;
    std::vector<Chain>   chains_;
    std::vector<EggShot> shots_;
    std::vector<Flyer>   flyers_;
    std::vector<Proxy>   proxies_;

    // 光っている物
    int   highlightTarget_ = -1;
    int   highlightProxy_ = -1;
    float highlightT_ = 0.0f;
    float lockPop_ = 0.0f;

    Vector3 eye_ {};
    Vector3 forward_ { 0.0f, 0.0f, 1.0f }; // カメラの向き（水平）。「奥」の向き

    int   shownCombo_ = 0;
    float comboTimer_ = 0.0f;      // 最後に当ててからの秒
    float comboPop_ = 0.0f;
    int   flowersFound_ = 0;
    int   flowersTotal_ = 0;
    float flowerPop_ = 0.0f;
    std::string stage_ = "stage1";
    std::function<void()> onMaxCombo_;
    int   comboFlowerTotal_ = 0;   // コンボの的の数
    float comboFlowerPop_ = 0.0f;
};
