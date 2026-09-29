#include "game/rail/road/RoadMeshBuilder.h"

#include <cmath>

uint32_t RoadMeshBuilder::AddVertex(const Vector3& pos, const Vector3& normal, float u, float v){
    Model::VertexData vert {};
    vert.position = { pos.x, pos.y, pos.z, 1.0f };
    vert.normal = normal;
    vert.texcoord = { u, v };
    data_.vertices.push_back(vert);
    return static_cast<uint32_t>( data_.vertices.size() - 1 );
}

void RoadMeshBuilder::AddTriangle(uint32_t a, uint32_t b, uint32_t c){
    data_.indices.push_back(a); data_.indices.push_back(b); data_.indices.push_back(c);
}

// ピースモデルの頂点を 回転(yaw)＋平行移動 してベイク先へ焼き込む。
//   回転は Obj3d の MakeAffine（行ベクトル・Ry）と同じ結果になるよう展開している。
//   ※以前は pitch も受けていたが、呼び出し元（ジョイント配置）が常に 0 だったので廃止
void RoadMeshBuilder::AppendModel(const Model::ModelData& src, const Vector3& pos, float yaw){
    const float cy = std::cos(yaw), sy = std::sin(yaw);
    auto rotate = [&](const Vector3& v) -> Vector3{
        // Ry(yaw): (x·c + z·s, y, -x·s + z·c)
        return { v.x * cy + v.z * sy, v.y, -v.x * sy + v.z * cy };
    };
    uint32_t base = VertexCount();
    for ( const auto& sv : src.vertices ) {
        Model::VertexData v = sv;
        Vector3 p = rotate({ sv.position.x, sv.position.y, sv.position.z });
        v.position = { p.x + pos.x, p.y + pos.y, p.z + pos.z, 1.0f };
        v.normal = rotate(sv.normal);
        data_.vertices.push_back(v);
    }
    for ( uint32_t idx : src.indices ) { data_.indices.push_back(base + idx); }
}
