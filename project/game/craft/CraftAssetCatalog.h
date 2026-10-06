#pragma once
// =====================================================================
//  CraftAssetCatalog：配置できる物の一覧（CraftKit の manifest.json から作る）。
//   ステージのファイルには物の名前（asset）だけを書き、大きさ・当たり判定・分類はここから引く。
//   キットのモデルはここで全部読む（実行中の読み込みはデバッグレイヤーが嫌うので、シーンの読み込み時に）。
//   キットに無いがエンジンで作る物（机の天板など）は AddBuiltin で足す
// =====================================================================
#include "engine/math/struct.h"

#include <string>
#include <vector>

class CraftAssetCatalog {
public:
    struct Asset {
        std::string name;      // モデル名（ステージの asset と同じ）
        std::string category;  // ground / tree / decor / prop / backdrop / title / builtin
        Vector3 boundsMin {};  // 置いた時の大きさ（エンジン座標・拡縮1倍）。選択の当たり判定にも使う
        Vector3 boundsMax {};
        bool    hasCollider = false;
    };

    static CraftAssetCatalog* GetInstance();

    // manifest.json を読み、キットのモデルを全部読み込む（2回目以降は何もしない）
    void Load();
    // エンジンで作るモデルを一覧に足す（モデルは呼び出し側が作っておく）
    void AddBuiltin(const std::string& name, const std::string& category, const Vector3& boundsMin, const Vector3& boundsMax);

    const Asset* Find(const std::string& name) const;
    const std::vector<Asset>& GetAssets() const{ return assets_; }
    // 分類の一覧（アセットブラウザのタブ用。manifest に出てくる順）
    const std::vector<std::string>& GetCategories() const{ return categories_; }
    // 分類から既定の層を決める（地面→ground、背景→backdrop、それ以外→decor）
    static int DefaultLayerFor(const std::string& category);

    static constexpr const char* kMissingModel = "craftMissing"; // 一覧に無い物の代わりに出す赤い箱
    static constexpr const char* kDeskModel = "craftDesk";       // 机の天板（ステージの下に敷くクラフト紙の板）

private:
    CraftAssetCatalog() = default;
    std::vector<Asset> assets_;
    std::vector<std::string> categories_;
    bool loaded_ = false;
};
