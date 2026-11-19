#pragma once
#include <memory>
#include <string>
#include <vector>

namespace KazumimiEngine { class Sprite; }

// =====================================================================
//  DigitStrip：数字の絵を並べて、数を描く（HUD・ワールドマップのパネル・リザルト画面で共通）。
//   数字は「0123456789/×+」のように 1 枚の絵に横に並んでいて（1 字ぶんの大きさは同じ）、そこから 1 字ずつ切り出して描く。
//   スプライトは 1 枚を 1 フレームに 2 回描くと後の位置で上書きされるので、字ごとに別のスプライトを使い回す。
//   使い方：フレームの描き始めに BeginFrame、あとは Draw を好きなだけ（capacity 字まで）
// =====================================================================
class DigitStrip {
public:
    DigitStrip();
    ~DigitStrip();

    // texturePath＝数字の並びの絵。characters＝絵に並んでいる字（左から。1 字 1 バイト）。
    // cellWidth / cellHeight＝1 字ぶんの大きさ(画素)。capacity＝1 フレームに描ける字の数
    void Initialize(const std::string& texturePath, const std::string& characters, float cellWidth, float cellHeight, int capacity);
    void Finalize();

    // このフレームの描き始め（使ったスプライトの数を戻す）
    void BeginFrame(){ next_ = 0; }
    // text を並べて描く。x＝左はし、y＝字の真ん中の高さ、height＝字の高さ（どれも画面の画素）。並べ終わりの x を返す。
    //   並びに無い字は半字ぶん空ける
    float Draw(const std::string& text, float x, float y, float height, float alpha = 1.0f);
    // text を並べた時の幅（右寄せ・真ん中寄せの計算に使う）
    float Width(const std::string& text, float height) const;

private:
    // 字と字の間（字の幅に対して）。ふちが重なるくらいに詰める
    static constexpr float kAdvance = 0.78f;

    std::vector<std::unique_ptr<KazumimiEngine::Sprite>> sprites_;
    std::string characters_;
    float cellWidth_ = 64.0f;
    float cellHeight_ = 96.0f;
    int   next_ = 0;
};
