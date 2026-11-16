#pragma once
// =====================================================================
//  ResultScreen：リザルト画面（クリア演出 企画書）。「帯＋3 行＋花丸」。
//   景色を暗い緑に沈め、緑のマットを重ねて、上から順に 1 つずつ出す：
//     ステージ名 → 帯（集めた花が 1 つずつ咲く）→ 3 行（数字が数え上がり、達成した行に卵スタンプ）→ 全部達成なら花丸
//     → 決定ボタンの案内 → 押すと、今回のスタンプが左上の所持数へ数え込まれる
//   ・行も「順に進む小さな列」（StepSequence）として持つ。行を足す・順番を変えるのは、列の組み立て（BuildRows）だけ
//   ・早送り：出している途中に決定ボタンを押すと、速く進む
//   ・花丸は、描き順のマスク付きの絵を Sprite::SetReveal で一筆ずつ出す
//   全部スプライト。1920×1080 を基準に、画面の高さに合わせて縮める。置き場所と秒数は clear.json（ClearResultLayout）
//   出すだけ。中身（数・達成したか）は Data で受け取る
// =====================================================================
#include "engine/utils/StepSequence.h"
#include "game/clear/ClearSettings.h"
#include "game/ui/DigitStrip.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

class Sprite;

class ResultScreen {
public:
    // 行の番号（帯の下の 3 行）
    enum Row { Row_Combo, Row_Health, Row_Enemy, Row_Count };

    // 出す中身
    struct Data {
        std::string stage;            // ステージの名前（名前の絵 intro_name_<stage>.png）
        std::string bandKind = "comboFlowers"; // 帯の集める物（絵と名前を選ぶ）。"none"＝帯なし
        int bandFound = 0, bandTotal = 0;
        int maxCombo = 0;             // 1 行目
        int health = 0, healthMax = 0; // 2 行目
        int enemiesDefeated = 0, enemyTotal = 0; // 3 行目
        std::array<bool, Row_Count> achieved {}; // 行ごとの、卵スタンプの条件を満たしたか
        bool hanamaru = false;        // 帯と 3 行がすべて達成
        int ownedBefore = 0;          // 今回の分を足す前の、持っている卵スタンプ
        int newStamps = 0;            // 今回、新しく取った卵スタンプ
    };

    ResultScreen();
    ~ResultScreen();

    // stage＝このシーンのステージ（名前の絵を先に読んでおく。プレイの途中で読み込まない）
    void Initialize(const ClearSettings& settings, const std::string& stage);
    void Finalize();

    // 出し始める
    void Open(const Data& data);
    // 片付ける（プレイ開始・エディタへ戻った時）
    void Close();
    bool IsOpen() const{ return phase_ != Phase::Closed; }

    // 毎フレーム。decide＝決定ボタンを押した瞬間（出している途中は早送り・待っている時は加算へ・加算中は数え終える）
    void Update(float deltaTime, bool decide);
    // 所持数への数え込みまで終わった
    bool IsFinished() const{ return phase_ == Phase::Finished; }

    void Draw();

private:
    enum class Phase { Closed, Showing, Waiting, Adding, Finished };
    using RowSequence = StepSequence<ResultScreen>;

    void BuildRows();                 // 出す順番を、列に組み立てる
    void AddRowSteps(Row row);        // 1 行ぶん：行を出す → 数え上げ → スタンプ
    int  RowTarget(Row row) const;    // 行の数え上げる数
    std::string RowText(Row row) const; // 行に出す文字（今の数え上がりの値で）

    // 1080p の座標 → 画面
    void Place(Sprite* sprite, float x, float y, float width, float height, float alpha = 1.0f, float rotation = 0.0f);
    void PlaceNatural(Sprite* sprite, const Vector2& size, float x, float y, float scale, float alpha = 1.0f);
    void DrawNumberRight(const std::string& text, float right, float y, float height);
    // 出てからの秒数 → ぽんと出る大きさ（0 → 行き過ぎて → 1）
    float Pop(float shownTime) const;

    ClearResultLayout layout_;
    ClearSounds sounds_;
    Data data_;
    Phase phase_ = Phase::Closed;
    RowSequence rows_;
    float speed_ = 1.0f;       // 早送りの倍率
    float openTime_ = 0.0f;    // 出し始めてからの秒数（マットのすべり込み・景色の沈み）
    float time_ = 0.0f;        // 画面の時間（案内のゆれ）

    // --- 出し具合（負＝まだ出していない。0 以上＝出してからの秒数）---
    float titleShown_ = -1.0f;
    std::array<float, Row_Count + 1> rowShown_ {};    // [0]＝帯、[1..]＝3 行
    std::vector<float> flowerShown_;                  // 帯の花（咲いてからの秒数）
    std::array<float, Row_Count> rowValue_ {};        // 行の、今出している数
    std::array<float, Row_Count> stampShown_ {};      // スタンプを押してからの秒数
    float hanamaruProgress_ = -1.0f;                  // 花丸の描き具合（0〜1）
    float promptShown_ = -1.0f;
    float ownedValue_ = 0.0f;                         // 左上の所持数（数え込みの途中の値）
    float addTime_ = 0.0f;
    int   lastTick_ = 0;                              // 数え上げの音を、数が変わった時だけ鳴らす

    // --- スプライト ---
    std::unique_ptr<Sprite> dim_, mat_, title_, hanamaru_, prompt_, ownedTag_;
    Vector2 matSize_ {}, titleSize_ {}, promptSize_ {}, ownedSize_ {};
    std::unique_ptr<Sprite> name_;   // ステージの名前の絵（無いステージは空）
    Vector2 nameSize_ {};
    std::array<std::unique_ptr<Sprite>, Row_Count + 1> rowBoards_, labels_;
    std::unique_ptr<Sprite> flowerLabel_;             // 帯が「かくれ花」の時の名前
    std::array<Vector2, Row_Count + 1> labelSizes_ {};
    Vector2 rowSize_ {}, flowerLabelSize_ {};
    std::array<std::unique_ptr<Sprite>, Row_Count> icons_, stampEmpty_, stamps_;
    std::vector<std::unique_ptr<Sprite>> flowerEmpty_, comboFlowers_, hiddenFlowers_, flowerStamps_;
    DigitStrip digits_;

    float scale_ = 1.0f;       // 1080p に対する画面の大きさ
    float offsetX_ = 0.0f;     // 16:9 でない画面で、真ん中へ寄せる量
};
