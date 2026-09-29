#pragma once
#include "engine/math/struct.h"
#include "engine/3d/model/Model.h"
#include <cstdint>

// =====================================================================
//  RoadMeshBuilder：道の CPU 側メッシュ（頂点/インデックス）を組み立てる小さな器。
//   ・掃引メッシュ／ジャンクションパッチ／ピースのベイク先が共通で使う
//   ・出来上がった Data() を RoadRenderSlots::EmitMesh に渡すと動的スロットへ書き込まれる
//   ・Clear は容量を保持する（ベイク先を Build 毎に使い回してもヒープ再確保しない）
// =====================================================================
class RoadMeshBuilder {
public:
    // 頂点を1つ追加してインデックスを返す
    uint32_t AddVertex(const Vector3& pos, const Vector3& normal, float u, float v);
    void     AddTriangle(uint32_t a, uint32_t b, uint32_t c);

    // ピースモデルを yaw 回転＋平行移動して焼き込む（静的レール用のDC削減。
    // 全ピースがアトラス共有なので、1つの動的メッシュにまとめて1ドローコールで描ける）
    void AppendModel(const Model::ModelData& src, const Vector3& pos, float yaw);

    void     ReserveVertices(size_t count){ data_.vertices.reserve(count); }
    void     Clear(){ data_.vertices.clear(); data_.indices.clear(); }
    uint32_t VertexCount() const{ return static_cast<uint32_t>( data_.vertices.size() ); }

    const Model::ModelData& Data() const{ return data_; }

private:
    Model::ModelData data_;
};
