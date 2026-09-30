#include "game/rail/road/RoadRenderSlots.h"

#include "engine/rail/SplineRail.h"
#include "engine/3d/obj/Obj3d.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/graphics/PipelineManager.h"

#include <cmath>

RoadRenderSlots::RoadRenderSlots() = default;
RoadRenderSlots::~RoadRenderSlots() = default;

void RoadRenderSlots::BeginBuild(Camera* camera, uint32_t atlasSrv){
    camera_ = camera;
    atlasSrv_ = atlasSrv;
    slotsUsed_ = 0;
    jointsUsed_ = 0;
    lastVertexCount_ = 0;
    lastTriangleCount_ = 0;
}

// 生成済みメッシュを空きスロットへ書き込む（スロット不足時のみ新規確保）
void RoadRenderSlots::EmitMesh(const Model::ModelData& data, int followRail, bool isJoint){
    if ( data.vertices.empty() || data.indices.empty() ) return;

    if ( slotsUsed_ == slots_.size() ) {
        // 新しいスロット：固定容量の動的バッファを一度だけ確保する（以後は memcpy のみ）
        auto slot = std::make_unique<MeshSlot>();
        slot->model = std::make_unique<Model>();
        slot->model->InitializeDynamic(ModelManager::GetInstance()->GetModelCommon(),
                                       4096, 24576, "resources/road/road_atlas.png");
        slot->model->SetTexture(atlasSrv_);
        slot->obj = std::make_unique<Obj3d>();
        slot->obj->Initialize(slot->model.get());
        slots_.push_back(std::move(slot));
    }
    MeshSlot& s = *slots_[slotsUsed_++];
    s.model->UpdateMesh(data.vertices, data.indices);
    s.rail = followRail;
    s.isJoint = isJoint;
    lastVertexCount_   += data.vertices.size();
    lastTriangleCount_ += data.indices.size() / 3;
    // カリング無し（既定）：巻き順に依存せず、下から見上げても道が消えない。
    // 背面カリング（オプション）：オーバードロー削減。UIから即時切替できる
    s.obj->SetPipelineType(cullNone_ ? PipelineType::Object3D_CullNone : PipelineType::Object3D);
    s.obj->SetCamera(camera_);
    s.obj->SetTranslation({ 0.0f, 0.0f, 0.0f });
    s.obj->Update();
}

// 動くレールのジョイントを1個置く（基準位置 = 置いた位置 - 現在の animOffset を覚えて毎フレーム追従）
void RoadRenderSlots::PlaceMovingJoint(Model* model, const Vector3& pos, float yaw, int railIdx,
                                       const Vector3& animOffset){
    if ( jointsUsed_ == joints_.size() ) {
        auto slot = std::make_unique<JointSlot>();
        slot->obj = std::make_unique<Obj3d>();
        slot->obj->Initialize(model);
        joints_.push_back(std::move(slot));
    }
    JointSlot& s = *joints_[jointsUsed_++];
    s.obj->SetModel(model);
    s.obj->SetCamera(camera_);
    s.obj->SetScale({ 1.0f, 1.0f, 1.0f });
    s.obj->SetTranslation(pos);
    s.obj->SetRotation({ 0.0f, yaw, 0.0f });
    s.obj->Update();
    s.rail = railIdx;
    s.base = { pos.x - animOffset.x, pos.y - animOffset.y, pos.z - animOffset.z };
}

// 余ったスロットは空メッシュにして描かない（バッファは保持＝次の編集で使い回す）
void RoadRenderSlots::EndBuild(){
    static const std::vector<Model::VertexData> kEmptyV;
    static const std::vector<uint32_t> kEmptyI;
    for ( size_t k = slotsUsed_; k < slots_.size(); ++k ) {
        slots_[k]->model->UpdateMesh(kEmptyV, kEmptyI);
    }
}

void RoadRenderSlots::Clear(){
    slots_.clear();
    slotsUsed_ = 0;
    joints_.clear();
    jointsUsed_ = 0;
    lastVertexCount_ = 0;
    lastTriangleCount_ = 0;
}

// 背面カリングの切替（既存スロットにも即時反映）
void RoadRenderSlots::SetCullNone(bool cullNone){
    if ( cullNone_ == cullNone ) return;
    cullNone_ = cullNone;
    for ( auto& slot : slots_ ) {
        slot->obj->SetPipelineType(cullNone_ ? PipelineType::Object3D_CullNone : PipelineType::Object3D);
    }
}

// 毎フレーム：動くレールの animOffset に追従し、カメラ行列を焼き直す
void RoadRenderSlots::Update(const std::vector<SplineRail>& rails, bool playMode){
    // 「後から出現する道」はプレイ中だけ隠す/せり上げる（エディタでは普通に見せて編集できるように）
    // レール r の出現状態 → 隠すか(true/false) と 追加の沈み込みY を返す
    auto appearOffset = [&](int r, bool& outHidden) -> float {
        outHidden = false;
        if ( !playMode || r < 0 || r >= ( int ) rails.size() ) return 0.0f;
        const SplineRail& rail = rails[r];
        if ( rail.appearTrigger < 0 ) return 0.0f;
        if ( rail.appearAnim <= 0.001f ) { outHidden = true; return 0.0f; }
        return -( 1.0f - rail.appearAnim ) * 2.5f; // 出現中：下からせり上がる
    };
    for ( size_t k = 0; k < slotsUsed_; ++k ) {
        MeshSlot& s = *slots_[k];
        if ( s.rail >= 0 && s.rail < ( int ) rails.size() ) {
            // 掃引メッシュはワールド座標で焼いてあるので、動くレールぶんだけ平行移動。
            // 列車式（animYaw）は「pivot中心の回転」＝回転＋(pivot - pivot*Ry) の平行移動で表す
            const SplineRail& followRail = rails[s.rail];
            Vector3 off = followRail.animOffset;
            off.y += appearOffset(s.rail, s.hiddenNow);
            if ( followRail.animYaw != 0.0f ) {
                float sinYaw = std::sin(followRail.animYaw), cosYaw = std::cos(followRail.animYaw);
                const Vector3& pivot = followRail.animPivot;
                Vector3 rotatedPivot { pivot.x * cosYaw + pivot.z * sinYaw, pivot.y,
                                       -pivot.x * sinYaw + pivot.z * cosYaw };
                s.obj->SetRotation({ 0.0f, followRail.animYaw, 0.0f });
                s.obj->SetTranslation({ pivot.x - rotatedPivot.x + off.x, off.y,
                                        pivot.z - rotatedPivot.z + off.z });
            } else {
                s.obj->SetRotation({ 0.0f, 0.0f, 0.0f });
                s.obj->SetTranslation(off);
            }
        } else {
            s.hiddenNow = false;
        }
        s.obj->Update();
    }
    for ( size_t k = 0; k < jointsUsed_; ++k ) {
        JointSlot& s = *joints_[k];
        if ( s.rail >= 0 && s.rail < ( int ) rails.size() ) {
            const SplineRail& followRail = rails[s.rail];
            Vector3 off = followRail.animOffset;
            off.y += appearOffset(s.rail, s.hiddenNow);
            Vector3 basePos = s.base;
            if ( followRail.animYaw != 0.0f ) {
                // ピース（ジョイント）は位置だけpivot中心で公転させる（自身の回転は据え置き）
                float sinYaw = std::sin(followRail.animYaw), cosYaw = std::cos(followRail.animYaw);
                const Vector3& pivot = followRail.animPivot;
                float dx = basePos.x - pivot.x, dz = basePos.z - pivot.z;
                basePos.x = pivot.x + dx * cosYaw + dz * sinYaw;
                basePos.z = pivot.z - dx * sinYaw + dz * cosYaw;
            }
            s.obj->SetTranslation({ basePos.x + off.x, basePos.y + off.y, basePos.z + off.z });
        } else {
            s.hiddenNow = false;
        }
        s.obj->Update();
    }
}

void RoadRenderSlots::Draw(bool showJoints) const{
    for ( size_t k = 0; k < slotsUsed_; ++k ) {
        if ( slots_[k]->isJoint && !showJoints ) continue; // ジョイントのまとめメッシュ
        if ( slots_[k]->hiddenNow ) continue;              // 出現前の道（プレイ中のみ）
        slots_[k]->obj->Draw();
    }
    if ( showJoints ) {
        for ( size_t k = 0; k < jointsUsed_; ++k ) {
            if ( joints_[k]->hiddenNow ) continue;
            joints_[k]->obj->Draw();
        }
    }
}
