#pragma once
#include "engine/math/struct.h"
#include "engine/3d/model/Model.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace KazumimiEngine {
class Obj3d;
class Camera;
class SplineRail;
}

// =====================================================================
//  RailMarkers：レールの経路を見せる細い線（エディット中の緑線。穴の区間は赤）。
//   レール1本 = リボンメッシュ1個（穴があるレールは赤リボンをもう1個）。
//   0.5m毎の Obj3d 群をやめ、固定容量の動的バッファを使い回す
//   （ドローコールが約220→レール本数になり、編集中の Obj3d 生成もゼロ）。
//   リボンはワールド座標で焼いておき、動くレールは animOffset / animYaw ぶん動かすだけ
// =====================================================================
class RailMarkers {
public:
    RailMarkers();
    ~RailMarkers(); // unique_ptr<Obj3d> のため cpp 側で定義

    // rails をサンプルしてリボンを作り直す。whiteTexIndex=単色化用の白テクスチャ（0=未使用）
    void Build(const std::vector<KazumimiEngine::SplineRail>& rails, KazumimiEngine::Camera* camera, uint32_t whiteTexIndex);
    // 動くレールの今の位置へリボンを動かす
    void UpdatePositions(const std::vector<KazumimiEngine::SplineRail>& rails);
    void UpdateMatrices();   // 行列更新（毎フレーム。カメラ移動に追従）
    void Draw() const;

    int  Count() const { return ( int ) slotsUsed_; }
    bool IsVisible() const { return visible_; }
    void SetVisible(bool visible){ visible_ = visible; }

private:
    // レール 1 本ぶんの線（リボンのモデルと見た目・レールの番号）
    struct Slot {
        std::unique_ptr<KazumimiEngine::Model> model;
        std::unique_ptr<KazumimiEngine::Obj3d> obj;
        int rail = -1;
    };
    // 生成したリボンを空きスロットへ書き込む（不足時のみ新規確保）
    void Emit(const KazumimiEngine::Model::ModelData& data, int railIdx, const KazumimiEngine::Vector4& color);

    std::vector<std::unique_ptr<Slot>> slots_;
    size_t   slotsUsed_ = 0;
    bool     visible_ = true;
    KazumimiEngine::Camera*  camera_ = nullptr;   // 直近 Build のカメラ
    uint32_t whiteTexIndex_ = 0;
};
