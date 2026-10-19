#pragma once
// =====================================================================
//  StageBackdrop：本編のステージに置くクラフトの背景（地面・空・丘・雲・木・柵・看板・草花）。
//   ステージ全体の置き場所は tools/stage_intro/make_stage_backdrop.py がマップ（道）から作る：
//     resources/map/<ステージ>_backdrop.json の "objects"
//       { "model": "ground_block_step", "pos": [x, y, z], "rotY": 180, "scale": [sx, sy, sz] }
//     （"offset" と書いた物は、Build に渡した origin からの相対位置。"rot": [x, y, z]（度）と書けば X・Z 回りも回せる）
//   同じモデルはまとめて1回で描く（InstancedGroup）。Obj3d は作らず、行列を直接計算して渡す
//   （地面のタイルや草花は数百個あるので、1個ずつ描くと重い）。
//   モデルは resources/craft/models（タイトルと同じキット）。LoadKitModels で読む
// =====================================================================
#include "engine/3d/obj/Obj3d.h"
#include "engine/math/struct.h"
#include "externals/nlohmann/json.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Camera;
class InstancedGroup;

class StageBackdrop {
public:
    StageBackdrop();
    ~StageBackdrop();

    // クラフトのキットのモデルを全部読む（底面原点のまま・つやなし）。タイトルと本編で共通
    static void LoadKitModels();

    // 置き場所の一覧から作る。noiseSrv＝ディゾルブのノイズ画像（一括描画が必ず束縛するので、読み込み済みの物を渡す）
    void Build(const nlohmann::json& list, const Vector3& origin, uint32_t noiseSrv);
    void Clear();
    void Update(const Camera* camera); // カメラの行列を反映（毎フレーム）
    void Draw(const Camera* camera);   // 一括描画（BaseScene::OnDrawInstanced から）
    size_t GetObjectCount() const;

private:
    struct Group {
        std::string model;
        std::vector<Obj3d::TransformationMatrix> matrices; // World と WorldInverseTranspose は作る時に計算済み
        std::unique_ptr<InstancedGroup> batch;
    };
    std::vector<Group> groups_;
};
