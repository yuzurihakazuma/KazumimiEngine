#include "game/rail/RailMarkers.h"
#include "engine/3d/obj/Obj3d.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/graphics/PipelineManager.h"
#include "engine/rail/SplineRail.h"

#include <cmath>

namespace {
    // 1セグメント（十字リボン2枚=4三角形）を out へ追加する。
    //   断面は「横リボン＋縦リボン」の十字型で、どの方向から見ても線に見える
    void AppendCrossRibbon(Model::ModelData& out, const SplineRail::RailFrame& f0,
                           const SplineRail::RailFrame& f1, float half){
        auto quad = [&](const Vector3& a0, const Vector3& a1, const Vector3& b1, const Vector3& b0, const Vector3& normal){
            uint32_t base = static_cast< uint32_t >( out.vertices.size() );
            const Vector3 ps[4] = { a0, a1, b1, b0 };
            const Vector2 uv[4] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
            for ( int k = 0; k < 4; ++k ) {
                Model::VertexData v {};
                v.position = { ps[k].x, ps[k].y, ps[k].z, 1.0f };
                v.normal = normal;
                v.texcoord = uv[k];
                out.vertices.push_back(v);
            }
            out.indices.push_back(base); out.indices.push_back(base + 1); out.indices.push_back(base + 2);
            out.indices.push_back(base); out.indices.push_back(base + 2); out.indices.push_back(base + 3);
        };
        auto offset = [&](const SplineRail::RailFrame& f, const Vector3& axis, float sign) -> Vector3{
            return { f.position.x + axis.x * half * sign, f.position.y + axis.y * half * sign, f.position.z + axis.z * half * sign };
        };
        // 横リボン（法線=up）と縦リボン（法線=right）の十字
        quad(offset(f0, f0.right, -1.0f), offset(f0, f0.right, +1.0f), offset(f1, f1.right, +1.0f), offset(f1, f1.right, -1.0f), f0.up);
        quad(offset(f0, f0.up, -1.0f), offset(f0, f0.up, +1.0f), offset(f1, f1.up, +1.0f), offset(f1, f1.up, -1.0f), f0.right);
    }
}

RailMarkers::RailMarkers() = default;
RailMarkers::~RailMarkers() = default;

void RailMarkers::Build(const std::vector<SplineRail>& rails, Camera* camera, uint32_t whiteTexIndex){
    camera_ = camera;
    whiteTexIndex_ = whiteTexIndex;
    slotsUsed_ = 0;

    const float spacing   = 0.5f;
    const float thickness = 0.08f;
    const Vector4 lineColor = { 0.2f, 1.0f, 0.35f, 1.0f }; // 通常区間：明るい緑
    const Vector4 holeColor = { 1.0f, 0.2f, 0.15f, 1.0f }; // 穴区間：赤

    for ( int railIdx = 0; railIdx < ( int ) rails.size(); ++railIdx ) {
        const SplineRail& rail = rails[railIdx];
        float len = rail.GetLength();
        if ( len <= 0.0f || rail.nodes.size() < 2 ) continue;
        if ( !rail.visible ) continue; // 非表示レール（連結用）は線を描かない

        const auto holes = rail.GetHoleIntervals();
        auto isHole = [&](float s) -> bool{
            for ( const auto& h : holes ) { if ( s >= h.d0 && s <= h.d1 ) return true; }
            return false;
        };

        Model::ModelData lineData, holeData;
        SplineRail::RailFrame prev = rail.GetFrameAtDistance(0.0f);
        float prevS = 0.0f;
        for ( float s = spacing; s <= len + 0.001f; s += spacing ) {
            float clamped = ( s > len ) ? len : s;
            SplineRail::RailFrame cur = rail.GetFrameAtDistance(clamped);
            if ( clamped - prevS > 1e-4f ) {
                AppendCrossRibbon(isHole(( prevS + clamped ) * 0.5f) ? holeData : lineData, prev, cur, thickness * 0.5f);
            }
            prev = cur;
            prevS = clamped;
        }
        Emit(lineData, railIdx, lineColor);
        Emit(holeData, railIdx, holeColor);
    }

    // 余ったスロットは空にして描かない（バッファは使い回す）
    static const std::vector<Model::VertexData> kEmptyVertices;
    static const std::vector<uint32_t> kEmptyIndices;
    for ( size_t k = slotsUsed_; k < slots_.size(); ++k ) {
        slots_[k]->model->UpdateMesh(kEmptyVertices, kEmptyIndices);
    }
}

void RailMarkers::Emit(const Model::ModelData& data, int railIdx, const Vector4& color){
    if ( data.vertices.empty() || data.indices.empty() ) return;
    if ( slotsUsed_ == slots_.size() ) {
        auto slot = std::make_unique<Slot>();
        slot->model = std::make_unique<Model>();
        // 白テクスチャ（GamePlayScene が先読み済み）＋マテリアル色で単色の線にする
        slot->model->InitializeDynamic(ModelManager::GetInstance()->GetModelCommon(),
                                       4096, 12288, "resources/block/white1x1.png");
        slot->obj = std::make_unique<Obj3d>();
        slot->obj->Initialize(slot->model.get());
        slot->obj->SetPipelineType(PipelineType::Object3D_CullNone);
        slots_.push_back(std::move(slot));
    }
    Slot& slot = *slots_[slotsUsed_++];
    slot.model->UpdateMesh(data.vertices, data.indices);
    if ( whiteTexIndex_ != 0 ) { slot.model->SetTexture(whiteTexIndex_); }
    if ( slot.model->GetMaterial() ) {
        slot.model->GetMaterial()->color = color;
        slot.model->GetMaterial()->enableLighting = 0;
    }
    slot.rail = railIdx;
    slot.obj->SetCamera(camera_);
    slot.obj->SetTranslation({ 0.0f, 0.0f, 0.0f });
    slot.obj->Update();
}

// リボンはワールド座標で焼いてあるので、レールの animOffset ぶんだけ平行移動。
//   列車式（animYaw）はpivot中心回転＝回転＋(pivot - pivot*Ry)の平行移動（道メッシュと同じ式）
void RailMarkers::UpdatePositions(const std::vector<SplineRail>& rails){
    for ( size_t k = 0; k < slotsUsed_; ++k ) {
        Slot& slot = *slots_[k];
        if ( slot.rail < 0 || slot.rail >= ( int ) rails.size() ) continue;
        const SplineRail& rail = rails[slot.rail];
        if ( rail.animYaw != 0.0f ) {
            float sinYaw = std::sin(rail.animYaw), cosYaw = std::cos(rail.animYaw);
            const Vector3& pivot = rail.animPivot;
            Vector3 rotatedPivot { pivot.x * cosYaw + pivot.z * sinYaw, pivot.y, -pivot.x * sinYaw + pivot.z * cosYaw };
            slot.obj->SetRotation({ 0.0f, rail.animYaw, 0.0f });
            slot.obj->SetTranslation({ pivot.x - rotatedPivot.x + rail.animOffset.x, rail.animOffset.y,
                                       pivot.z - rotatedPivot.z + rail.animOffset.z });
        } else {
            slot.obj->SetRotation({ 0.0f, 0.0f, 0.0f });
            slot.obj->SetTranslation(rail.animOffset);
        }
    }
}

void RailMarkers::UpdateMatrices(){
    for ( size_t k = 0; k < slotsUsed_; ++k ) { slots_[k]->obj->Update(); }
}

void RailMarkers::Draw() const{
    if ( !visible_ ) return;
    for ( size_t k = 0; k < slotsUsed_; ++k ) { slots_[k]->obj->Draw(); }
}
