#pragma once
// =====================================================================
//  WorldMapPanel：ワールドマップの左の情報パネル（ステージ選択・飛び込み演出 企画書）。
//   マスに立つと左からすべり込み、離れると戻る。段ボールの板（resources/worldmap/panel.png）に
//   ステージの名前（ステージ開始カードと同じ intro_name_<stage>.png）とコインの数・ベストを重ねる。
//   まだ遊べないマスは panel_locked.png（？？？）だけ
//   数字は digits.png（0〜9・/・-）を切り出して並べる（スプライトなので SDF 文字より手前に出せる）
// =====================================================================
#include "engine/math/struct.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

class Sprite;

class WorldMapPanel {
public:
    WorldMapPanel();
    ~WorldMapPanel();

    void Initialize();
    void Finalize();

    // 出すマスの内容。stage が空＝まだ遊べない
    void SetContent(const std::string& stage, bool unlocked, int coinTotal, int coinGot);
    // shown＝出す（左からすべり込む）／隠す
    void Update(float deltaTime, bool shown);
    void Draw();

private:
    void DrawNumber(const std::string& text, float x, float y, float height);

    std::unique_ptr<Sprite> panel_;
    std::unique_ptr<Sprite> panelLocked_;
    std::map<std::string, std::unique_ptr<Sprite>> names_; // ステージごとの名前（必要になった時に読む）
    std::vector<std::unique_ptr<Sprite>> digits_;          // 数字を並べる分
    uint32_t digitsTexture_ = 0;
    int      nextDigit_ = 0; // このフレームで次に使う数字のスプライト

    std::string stage_;
    bool  unlocked_ = false;
    int   coinTotal_ = 0;
    int   coinGot_ = 0;
    float slide_ = 0.0f;     // 0＝隠れている 1＝出ている
    float slideTime_ = 0.0f; // 出始めてからの秒数（行き過ぎて戻る動き）
    bool  shown_ = false;
};
