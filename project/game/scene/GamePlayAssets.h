#pragma once
// =====================================================================
//  GamePlayAssets：ゲームプレイシーンが使う音・モデル・テクスチャの一括読み込み。
//   実行中（フレームの途中）の読み込みは D3D12 のデバッグレイヤーが嫌うので、
//   ゲーム中に名前で引かれる物（ブロック・敵・道・エフェクトの粒など）はここで全部先に読む。
//   新しいモデルやテクスチャを使う時はここに足す
// =====================================================================
#include "engine/graphics/TextureManager.h" // TextureData

#include <string>
#include <unordered_map>

namespace GamePlayAssets {
    // bgmFile は BGM のパス。textures には名前 → 読み込んだテクスチャを入れる
    void Load(const std::string& bgmFile, std::unordered_map<std::string, TextureData>& textures);
}
