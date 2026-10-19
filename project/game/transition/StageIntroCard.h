#pragma once
// =====================================================================
//  StageIntroCard：段ボールのカード（読み込み中のカードとステージ開始カードで共通）。
//   段ボールの台紙に、色紙の星とキャラの切り絵が「ポン」と貼られ、巻いた布が転がって広がり、
//   そこに名前（行き先・ステージ名）が出る。待っている間もゆらゆら揺れ、「ロードちゅう…」の点が増える。
//   最後は、はさみが下の折り目の線を切って、上の部分は上へ・下の帯は下へ分かれ、その間から次のシーンが出る
//   （後ろへ倒れる SetFall・上へ抜ける SetLift も残してある）。演技の間は、画面の上下に帯を出す。
//
//   ・置き場所：resources/kamishibai/intro_layout.json（tools/kamishibai/make_kamishibai_assets.py が書き出す。
//       切れ目のまわりに部品を置かないことも、そこで確かめる）
//   ・時刻：遷移の定義（resources/transition/card.json / title_open.json / stage_intro.json）の部品
//       introElement / introCut / introFall / introLift / letterbox
//   ・中身：名前の画像・帯と星の色・出す敵 → resources/stage_intro/<中身>.json
//       （ステージ開始＝ステージ名、読み込み中＝ card_<行き先のシーン>）。書いていない色は置き場所の既定の色
// =====================================================================
#include "engine/math/struct.h"
#include "externals/nlohmann/json.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Sprite;

class StageIntroCard {
public:
    StageIntroCard();
    ~StageIntroCard();

    // 画像と置き場所を先に全部読む（実行中の読み込みはデバッグレイヤーが嫌うので、起動時に）
    void Load();
    void Finalize();

    // 中身：名前・色・敵（resources/stage_intro/<content>.json）
    void SetContent(const std::string& content);
    // 再生の最初に、全部を隠して元の位置へ
    void Reset();

    // 部品の動き（同じ id の部品は全部）。progress=0〜1、elapsed=始まってからの秒数
    //   motion：slideIn（台紙が右から入る）/ pop（ポンと貼られる）/ unroll（巻いた布が転がって広がる）/
    //           jumpIn（左からジャンプ）/ runIn（右から走り込む）/ rollIn（転がってくる）/ show（ただ出す）
    void Animate(const std::string& id, const std::string& motion, float progress, float elapsed);
    // はさみで切って分かれる。elapsed=始まってからの秒数、cutTime=切り終えるまで、flyTime=分かれて飛ぶ時間
    //   lift / drop＝上の部分が上へ・下の帯が下へ動く量（画面の高さ=1）
    void SetCut(float elapsed, float cutTime, float flyTime, float lift, float drop);
    void SetFall(float progress);   // 台紙ごと後ろへ倒れる（0〜1）
    void SetLift(float progress);   // 台紙ごと画面の上へ引き抜かれる（0〜1）
    void SetBars(float amount);     // 画面の上下の帯（0=無い / 1=出ている）
    void Update(float time);        // 待っている間のゆらゆら・点の数

    // 画面の大きさ（px）で描く。カードの座標は 1920x1080 を基準に引き伸ばす
    void Draw(float width, float height);
    bool IsCardVisible() const{ return cardVisible_; }

private:
    struct Layer {
        std::unique_ptr<Sprite> sprite;
        float width = 0.0f, height = 0.0f; // 画像の大きさ(px)
        bool  tinted = false;              // 部品の色を掛ける層か（白いふち・影・縫い目には掛けない）
    };
    struct Element {
        std::string id;
        std::vector<Layer> layers;
        Vector3 color { 1.0f, 1.0f, 1.0f };
        Vector3 defaultColor { 1.0f, 1.0f, 1.0f }; // 置き場所に書いた色（中身に色が無い時）
        // 置き場所
        float baseX = 0.0f, baseY = 0.0f, baseScale = 1.0f, baseAngle = 0.0f; // 角度は rad
        float swayAmount = 0.035f, swaySpeed = 1.6f, swayPhase = 0.0f;        // 待機中のゆれ
        float bob = 0.0f;     // 待機中にぴょこぴょこ跳ねる高さ(px)
        int   blink = -1;     // 「…」の何個目か（時間で1つずつ増える。負＝いつも出す）
        bool  stamp = false;  // 着いたら足踏みする（走ってきた敵）
        bool  bottom = false; // 切れ目より下の帯の部品（切ると下へ落ちる）
        // 今の状態
        bool  visible = false;
        bool  disabled = false; // 中身に絵が無い（名前の画像が無い時など）
        float offsetX = 0.0f, offsetY = 0.0f;
        float scaleX = 1.0f, scaleY = 1.0f;
        float angle = 0.0f;
        float reveal = 1.0f;  // 左から見せる割合（名前の帯）
        bool  arrived = false;
    };

    void Configure(const nlohmann::json& layout);
    void AddLayer(Element& element, const std::string& file, bool tinted);
    // カードの中の点 → 切れ目で分かれた後の位置（上の部分・下の帯それぞれの中心のまわりに回って、上・下へずれる）
    Vector2 Split(float x, float y, bool bottom) const;
    // カードの座標（1920x1080）の点 → 画面の座標（倒れる・抜ける・分かれる、と画面の大きさ）
    Vector2 Project(float x, float y, bool bottom, float width, float height) const;
    void DrawElement(const Element& element, float width, float height, float time);
    void DrawRoll(float width, float height);
    void DrawSlit(float width, float height);
    void DrawScissors(float width, float height);

    std::vector<Element> elements_;
    std::unordered_map<std::string, uint32_t> textures_; // 画像のファイル名 → テクスチャ
    std::string enemy_ = "enemy_ground";

    // 巻いた布（名前の帯の右の端で転がる）
    std::unique_ptr<Sprite> roll_;
    std::unique_ptr<Sprite> rollShade_;
    bool  rollVisible_ = false;
    float rollX_ = 0.0f;        // 中心（カードの座標）
    float rollDiameter_ = 0.0f;
    float rollScroll_ = 0.0f;   // 模様のずれ(px)
    float rollBounce_ = 0.0f;   // 端で止まった時の跳ね(px)

    // 切れ目とはさみ（寸法は intro_layout.json の cut / scissors。カードの px）
    std::unique_ptr<Sprite> slit_;
    std::unique_ptr<Sprite> scissors_;
    float margin_ = 0.0f;       // カードの左右の、段ボールの切れ目（ギザギザ）の幅
    float cutY_ = 0.0f, topHeight_ = 0.0f, bottomTop_ = 0.0f, bottomHeight_ = 0.0f;
    float slitTop_ = 0.0f, slitHeight_ = 0.0f, slitImageWidth_ = 0.0f;
    float scissorsTipX_ = 0.0f, scissorsTipY_ = 0.0f, scissorsWidth_ = 0.0f, scissorsHeight_ = 0.0f;
    float cutProgress_ = 0.0f;  // はさみが切り進んだ割合（0〜1）
    float scissorsX_ = -1.0f;   // はさみの刃先（画面幅=1。負なら出さない）
    float snipTime_ = 0.0f;
    float topLift_ = 0.0f, topTurn_ = 0.0f;       // 上の部分：上へのずれ（画面の高さ=1）と回り(rad)
    float bottomDrop_ = 0.0f, bottomTurn_ = 0.0f; // 下の帯：下へのずれと回り

    // 上下の帯
    std::unique_ptr<Sprite> barTop_;
    std::unique_ptr<Sprite> barBottom_;
    float bars_ = 0.0f;

    float fall_ = 0.0f;         // 倒れる角度(rad)
    float lift_ = 0.0f;         // 上へ抜けた量（カードの px）
    float cardAlpha_ = 1.0f;
    bool  cardVisible_ = false;
    float time_ = 0.0f;
    bool  loaded_ = false;
};
