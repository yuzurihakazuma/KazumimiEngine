#pragma once
// =====================================================================
//  StageBackdrop：本編のスタートのまわりに置くクラフトの背景（空・丘・雲・柵・看板・草花・地面）。
//   ステージ開始の演技を撮るイントロカメラと、最初の通常カメラに映る範囲だけを埋める
//   （カードが抜けた先が水色のクリアカラーにならないように）。
//   置き場所はステージごと（resources/stage_intro/<stage>.json の "backdrop"）に、スタート位置からの相対位置で書く：
//     { "model": "hill_backdrop_00", "offset": [x, y, z], "rotY": 180, "scale": 2.6 }
//     "grid": { "x": [から, まで, 間隔], "z": [から, まで, 間隔] } を付けると、その範囲に並べる（地面のタイルなど）
//   モデルは resources/craft/models（タイトルと同じキット）。LoadKitModels で読む
// =====================================================================
#include "engine/math/struct.h"
#include "externals/nlohmann/json.hpp"

#include <memory>
#include <vector>

class Obj3d;

class StageBackdrop {
public:
    StageBackdrop();
    ~StageBackdrop();

    // クラフトのキットのモデルを全部読む（底面原点のまま・つやなし）。タイトルと本編で共通
    static void LoadKitModels();

    // 置き場所の一覧から物を作る（origin＝スタート位置）
    void Build(const nlohmann::json& list, const Vector3& origin);
    void Clear();
    void Update();  // カメラの行列を反映（毎フレーム）
    void Draw();

private:
    void Add(const std::string& model, const Vector3& position, float rotationY, const Vector3& scale);

    std::vector<std::unique_ptr<Obj3d>> objects_;
};
