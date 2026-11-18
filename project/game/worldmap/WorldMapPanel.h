#pragma once
#include "engine/math/struct.h"
#include "game/ui/DigitStrip.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace KazumimiEngine { class Sprite; }

// =====================================================================
//  WorldMapPanel：ワールドマップの左の情報パネル（ステージ選択・飛び込み演出 企画書）。
//   マスに立つと左からすべり込み、離れると戻る。段ボールの板（resources/worldmap/panel.png）に
//   ステージの名前（ステージ開始カードと同じ intro_name_<stage>.png）とコインの数・コンボ花・卵スタンプを重ねる。
//   まだ遊べないマスは panel_locked.png（？？？）だけ
//   数字は digits.png（0〜9・/・-）を切り出して並べる（DigitStrip。スプライトなので SDF 文字より手前に出せる）
// =====================================================================
class WorldMapPanel {
public:
    WorldMapPanel();
    ~WorldMapPanel();

    void Initialize();
    void Finalize();

    // 出すマスの内容。stage が空＝まだ遊べない
    // 出す数（StageRecords から）
    struct Counts {
        int coins = 0;                       // いちばんためたコイン
        int comboFound = 0, comboTotal = 0;  // 見つけたコンボ花 / 全体
        int stars = 0, starTotal = 0;        // 取った星 / 全体（3。全体が 0＝まだクリアしていない）
    };
    void SetContent(const std::string& stage, bool unlocked, const Counts& counts);
    // shown＝出す（左からすべり込む）／隠す
    void Update(float deltaTime, bool shown);
    void Draw();

private:
    std::unique_ptr<KazumimiEngine::Sprite> panel_;
    std::unique_ptr<KazumimiEngine::Sprite> panelLocked_;
    std::map<std::string, std::unique_ptr<KazumimiEngine::Sprite>> names_; // ステージごとの名前（必要になった時に読む）
    DigitStrip digits_;                                    // 数字を並べる

    std::string stage_;
    bool   unlocked_ = false;
    Counts counts_;
    float slide_ = 0.0f;     // 0＝隠れている 1＝出ている
    float slideTime_ = 0.0f; // 出始めてからの秒数（行き過ぎて戻る動き）
    bool  shown_ = false;
};
