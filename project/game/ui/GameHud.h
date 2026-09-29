#pragma once
// =====================================================================
//  GameHud：プレイ中の画面表示（左上）。
//   ・上段＝保持卵スロット6個（持っている卵＝緑 / 空き＝薄いグレー）＋「×N（＋お腹）」の数字
//   ・下段＝お腹にためた数（オレンジの小さい丸）
//   ・その下＝コイン取得数「コイン n/全体」（コインが無いマップでは出さない）
//  表示するだけ。数はそれぞれのシステム（EggSystem / CoinSystem）が持つ
// =====================================================================
#include <cstdint>
#include <memory>
#include <vector>

#include <d3d12.h>

class Sprite;
class SDFText;
class EggSystem;
class CoinSystem;

class GameHud {
public:
    GameHud();
    ~GameHud();

    // circleSrv：スロットと小丸に使う円テクスチャ
    void Initialize(uint32_t circleSrv);

    // aiming=true なら構え中（1個を手に持っている扱いで、列から1個減らして見せる）
    void Update(const EggSystem& eggSystem, bool aiming, const CoinSystem& coinSystem);

    // スプライト描画の準備（SpriteCommon::PreDraw）の後に呼ぶ
    void Draw(ID3D12GraphicsCommandList* commandList);

private:
    std::vector<std::unique_ptr<Sprite>> eggSlots_;
    std::vector<std::unique_ptr<Sprite>> bellyIcons_;
    std::unique_ptr<SDFText> eggCountText_;
    std::unique_ptr<SDFText> coinCountText_;
    bool showCoinCount_ = false;
};
