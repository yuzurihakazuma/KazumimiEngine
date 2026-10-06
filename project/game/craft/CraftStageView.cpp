#include "game/craft/CraftStageView.h"
#include "game/craft/CraftAssetCatalog.h"

#include "engine/3d/model/ModelManager.h"
#include "engine/3d/obj/Obj3d.h"

#include <unordered_set>

namespace {
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
}

CraftStageView::CraftStageView() = default;
CraftStageView::~CraftStageView() = default;

void CraftStageView::Initialize(Camera* camera){
    camera_ = camera;
    visuals_.clear();
    builtVersion_ = -1;
}

void CraftStageView::Finalize(){
    visuals_.clear();
    builtVersion_ = -1;
}

// id で突き合わせて、増えた物を作り・消えた物を捨てる
void CraftStageView::Rebuild(const CraftStage& stage){
    ModelManager* modelManager = ModelManager::GetInstance();
    std::unordered_set<uint64_t> alive;
    for ( const CraftObject& object : stage.GetObjects() ) {
        alive.insert(object.id);
        Visual& visual = visuals_[object.id];
        if ( !visual.object || visual.asset != object.asset ) {
            Model* model = modelManager->FindModel(object.asset);
            if ( !model ) { model = modelManager->FindModel(CraftAssetCatalog::kMissingModel); } // 一覧に無い物は赤い箱
            visual.object = std::make_unique<Obj3d>();
            visual.object->Initialize(model);
            visual.object->SetCamera(camera_);
            visual.asset = object.asset;
        }
        visual.layer = object.layer;
    }
    for ( auto it = visuals_.begin(); it != visuals_.end(); ) {
        it = alive.count(it->first) ? std::next(it) : visuals_.erase(it);
    }
    builtVersion_ = stage.GetVersion();
}

void CraftStageView::Update(const CraftStage& stage){
    if ( stage.GetVersion() != builtVersion_ ) { Rebuild(stage); }
    // 背景の追従：基準の位置（無ければ追従しない）からカメラが動いた分
    Vector3 cameraMove {};
    const nlohmann::json& environment = stage.GetEnvironment();
    if ( environment.contains("backdropAnchor") && environment["backdropAnchor"].is_array() && environment["backdropAnchor"].size() >= 3 ) {
        const nlohmann::json& anchor = environment["backdropAnchor"];
        cameraMove = { cameraPosition_.x - anchor[0].get<float>(), cameraPosition_.y - anchor[1].get<float>(),
                       cameraPosition_.z - anchor[2].get<float>() };
    }
    for ( const CraftObject& object : stage.GetObjects() ) {
        auto it = visuals_.find(object.id);
        if ( it == visuals_.end() || !it->second.object ) { continue; }
        Obj3d* visual = it->second.object.get();
        if ( object.id != heldId_ ) {
            Vector3 position = object.position;
            Vector3 rotation { object.rotationDeg.x * kDegToRad, object.rotationDeg.y * kDegToRad, object.rotationDeg.z * kDegToRad };
            if ( object.params.contains("parallax") && object.params["parallax"].is_number() ) {
                const float follow = object.params["parallax"].get<float>();
                position = { position.x + cameraMove.x * follow, position.y + cameraMove.y * follow, position.z + cameraMove.z * follow };
            }
            if ( modifier_ ) { modifier_(object, position, rotation); }
            visual->SetTranslation(position);
            visual->SetRotation(rotation);
            visual->SetScale(object.scale);
        }
        visual->Update();
    }
}

void CraftStageView::Draw(){
    for ( auto& [id, visual] : visuals_ ) {
        if ( visual.object && IsLayerVisible(visual.layer) ) { visual.object->Draw(); }
    }
}

Obj3d* CraftStageView::FindObject(uint64_t id) const{
    auto it = visuals_.find(id);
    return ( it != visuals_.end() ) ? it->second.object.get() : nullptr;
}
