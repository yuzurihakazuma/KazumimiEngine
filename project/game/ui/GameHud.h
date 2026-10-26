#pragma once
// =====================================================================
//  GameHud：プレイ中の画面表示（左上）。
//   ワールドマップの情報パネル・ゲームオーバーの画面と同じ、段ボールの札とフェルトの部品・ふち付きの太い数字にそろえる：
//   ・上段＝持っている卵（卵の切り絵）6個。空きは点線の卵。右に「×N」（お腹にためた分があれば「+n」）
//   ・卵の下＝お腹にためた数（オレンジのフェルトの丸）
//   ・下段＝コイン（金色のフェルト）と「n/全体」（コインが無いマップでは出さず、札も短くする）
//  全部スプライト（文字も数字の絵を並べる）。1920×1080 を基準に、画面の高さに合わせて縮める。
//  素材は tools/hud/make_hud_assets.py → resources/ui/hud_*.png
//  表示するだけ。数はそれぞれのシステム（EggSystem / CoinSystem）が持つ
// =====================================================================
#include <memory>
#include <string>
#include <vector>

#include <d3d12.h>

class Sprite;
class EggSystem;
class CoinSystem;

class GameHud {
public:
    GameHud();
    ~GameHud();

    void Initialize();

    // aiming=true なら構え中（1個を手に持っている扱いで、列から1個減らして見せる）
    void Update(const EggSystem& eggSystem, bool aiming, const CoinSystem& coinSystem);

    // スプライト描画の準備（SpriteCommon::PreDraw）の後に呼ぶ
    void Draw(ID3D12GraphicsCommandList* commandList);

private:
    // 数字の絵を並べる（x,y＝左端・真ん中の高さ。1080p 基準）。並べ終わりの x を返す
    float DrawNumber(const std::string& text, float x, float y, float height);
    void Place(Sprite* sprite, float x, float y, float width, float height, float alpha = 1.0f);

    std::unique_ptr<Sprite> board_;
    std::unique_ptr<Sprite> coin_;
    std::vector<std::unique_ptr<Sprite>> eggs_, emptyEggs_, belly_;
    std::vector<std::unique_ptr<Sprite>> digits_;
    int nextDigit_ = 0;

    int held_ = 0, belly_count_ = 0, coins_ = 0, coinTotal_ = 0;
    float scale_ = 1.0f; // 1080p に対する画面の大きさ
};
