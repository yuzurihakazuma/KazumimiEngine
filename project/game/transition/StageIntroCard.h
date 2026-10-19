#pragma once
// =====================================================================
//  StageIntroCard：ステージ開始カード（ステージ開始演出 企画書）。
//   段ボールの台紙に、色紙の星とキャラの切り絵が「ポン」と貼られ、巻いた布が転がって広がり、
//   そこにステージ名が出る。待っている間もゆらゆら揺れ、最後は台紙ごと後ろへ倒れる。
//   プレイヤーの演出の間は、画面の上下に帯を出す。
//
//   ・型（どのステージでも同じ）：部品の置き場所と、動きの時刻・長さ
//       → resources/transition/stage_intro.json（"stageIntro" に置き場所、"parts" に動き）
//   ・中身（ステージごと）：ステージ名の画像・帯の色・星の色・出す敵
//       → resources/stage_intro/<ステージ>.json
//   動きは KamishibaiTransition の部品（introElement / introFall / letterbox）から渡される。
//   画像は tools/kamishibai/make_kamishibai_assets.py（キャラは render_cutouts.py の切り絵）で作る
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

    // 画像を先に全部読む（実行中の読み込みはデバッグレイヤーが嫌うので、起動時に）
    void Load();
    void Finalize();

    // 型：部品の置き場所（定義の "stageIntro"）
    void Configure(const nlohmann::json& layout);
    // 中身：ステージごとの名前・色・敵（resources/stage_intro/<stage>.json）
    void SetContent(const std::string& stage);
    // 再生の最初に、全部を隠して元の位置へ
    void Reset();

    // 部品の動き。progress=0〜1、elapsed=始まってからの秒数
    //   motion：slideIn（台紙が右から入る）/ pop（ポンと貼られる）/ unroll（巻いた布が転がって広がる）/
    //           jumpIn（左からジャンプ）/ runIn（右から走り込む）/ rollIn（転がってくる）
    void Animate(const std::string& id, const std::string& motion, float progress, float elapsed);
    void SetFall(float progress);   // 台紙ごと後ろへ倒れる（0〜1）
    void SetLift(float progress);   // 台紙ごと画面の上へ引き抜かれる（0〜1。下の 3D の場面がそのまま見える）
    void SetBars(float amount);     // 画面の上下の帯（0=無い / 1=出ている）
    void Update(float time);        // 待っている間のゆらゆら

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
        // 置き場所（型）
        float baseX = 0.0f, baseY = 0.0f, baseScale = 1.0f, baseAngle = 0.0f; // 角度は rad
        float swayAmount = 0.035f, swaySpeed = 1.6f, swayPhase = 0.0f;        // 待機中のゆれ
        bool  stamp = false;  // 着いたら足踏みする（走ってきた敵）
        // 今の状態
        bool  visible = false;
        float offsetX = 0.0f, offsetY = 0.0f;
        float scaleX = 1.0f, scaleY = 1.0f;
        float angle = 0.0f;
        float reveal = 1.0f;  // 左から見せる割合（名前の帯）
        bool  arrived = false;
    };

    Element* Find(const std::string& id);
    void AddLayer(Element& element, const std::string& file, bool tinted);
    // カードの座標（1920x1080）の点 → 画面の座標（台紙が倒れる奥行きと、画面の大きさ）
    Vector2 Project(float x, float y, float width, float height) const;
    void DrawElement(const Element& element, float width, float height, float time);
    void DrawRoll(float width, float height);

    std::vector<Element> elements_;
    std::unordered_map<std::string, uint32_t> textures_; // 画像のファイル名 → テクスチャ
    std::unordered_map<std::string, std::string> enemyFiles_; // 敵の名前 → 切り絵の画像
    std::string enemy_ = "enemy_ground";
    std::string stage_;
    std::string layoutKey_;     // 今の置き場所の中身（同じなら部品を作り直さない）

    // 巻いた布（名前の帯の右の端で転がる）
    std::unique_ptr<Sprite> roll_;
    std::unique_ptr<Sprite> rollShade_;
    bool  rollVisible_ = false;
    float rollX_ = 0.0f;        // 中心（カードの座標）
    float rollDiameter_ = 0.0f;
    float rollScroll_ = 0.0f;   // 模様のずれ(px)
    float rollBounce_ = 0.0f;   // 端で止まった時の跳ね(px)

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
