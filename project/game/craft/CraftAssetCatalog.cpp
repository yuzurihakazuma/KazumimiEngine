#include "game/craft/CraftAssetCatalog.h"
#include "game/craft/CraftStage.h"

#include "engine/3d/model/Model.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/math/Matrix4x4.h"
#include "externals/nlohmann/json.hpp"

#include <algorithm>
#include <fstream>

using json = nlohmann::json;

namespace {
    constexpr const char* kManifestPath = "resources/craft/manifest.json";
    constexpr const char* kModelDir = "resources/craft";

    Vector3 ReadVector(const json& value){
        if ( !value.is_array() || value.size() < 3 ) { return {}; }
        return { value[0].get<float>(), value[1].get<float>(), value[2].get<float>() };
    }
}

CraftAssetCatalog* CraftAssetCatalog::GetInstance(){
    static CraftAssetCatalog instance;
    return &instance;
}

void CraftAssetCatalog::Load(){
    ModelManager* modelManager = ModelManager::GetInstance();
    // 一覧に無い物の代わりの赤い箱（データは消さずに残し、見た目だけで知らせる）
    modelManager->CreateCubeModel(kMissingModel, 1.0f);
    if ( Model* missing = modelManager->FindModel(kMissingModel) ) {
        missing->SetTexture("resources/block/white1x1.png");
        if ( missing->GetMaterial() ) { missing->GetMaterial()->color = { 1.0f, 0.15f, 0.15f, 1.0f }; }
    }
    // 机の天板（キットに無いので、1m角の箱にクラフト紙を繰り返し貼って作る。大きく引き伸ばして敷く）
    modelManager->CreateCubeModel(kDeskModel, 1.0f);
    if ( Model* desk = modelManager->FindModel(kDeskModel) ) {
        desk->SetTexture("resources/craft/textures/kraft.png");
        desk->SetTiledUV(true);
        if ( Model::Material* material = desk->GetMaterial() ) {
            material->matte = 1.0f;
            material->uvTransform = MatrixMath::MakeScale({ 240.0f, 110.0f, 1.0f });
            material->color = { 0.93f, 0.84f, 0.70f, 1.0f };
        }
    }
    if ( loaded_ ) { return; }

    std::ifstream file(kManifestPath);
    if ( !file.is_open() ) { return; }
    json manifest;
    try { file >> manifest; } catch ( ... ) { return; }

    for ( const json& entry : manifest.value("assets", json::array()) ) {
        Asset asset;
        asset.name = entry.value("name", "");
        asset.category = entry.value("category", "decor");
        // manifest は glTF の座標（右手系）。エンジンは読み込み時に X を反転するので、X の範囲も反転させる
        const Vector3 gltfMin = ReadVector(entry["bounds_min"]);
        const Vector3 gltfMax = ReadVector(entry["bounds_max"]);
        asset.boundsMin = { -gltfMax.x, gltfMin.y, gltfMin.z };
        asset.boundsMax = { -gltfMin.x, gltfMax.y, gltfMax.z };
        asset.hasCollider = entry.contains("collider") && !entry["collider"].is_null();
        if ( asset.name.empty() ) { continue; }

        const std::string file = entry.value("file", "");
        const size_t slash = file.find_last_of('/');
        const std::string directory = std::string(kModelDir) + "/" + ( slash == std::string::npos ? "" : file.substr(0, slash) );
        modelManager->LoadModel(asset.name, directory, file.substr(slash == std::string::npos ? 0 : slash + 1), true);
        // 紙・フェルト・段ボールはつやが無いので、光の照り返しを出さない
        if ( Model* model = modelManager->FindModel(asset.name) ) {
            if ( model->GetMaterial() ) { model->GetMaterial()->matte = 1.0f; }
        }

        if ( std::find(categories_.begin(), categories_.end(), asset.category) == categories_.end() ) {
            categories_.push_back(asset.category);
        }
        assets_.push_back(asset);
    }
    AddBuiltin(kDeskModel, "builtin", { -0.5f, -0.5f, -0.5f }, { 0.5f, 0.5f, 0.5f });
    loaded_ = true;
}

void CraftAssetCatalog::AddBuiltin(const std::string& name, const std::string& category,
                                   const Vector3& boundsMin, const Vector3& boundsMax){
    if ( Find(name) ) { return; }
    Asset asset;
    asset.name = name;
    asset.category = category;
    asset.boundsMin = boundsMin;
    asset.boundsMax = boundsMax;
    assets_.push_back(asset);
    if ( std::find(categories_.begin(), categories_.end(), category) == categories_.end() ) {
        categories_.push_back(category);
    }
}

const CraftAssetCatalog::Asset* CraftAssetCatalog::Find(const std::string& name) const{
    for ( const Asset& asset : assets_ ) {
        if ( asset.name == name ) { return &asset; }
    }
    return nullptr;
}

int CraftAssetCatalog::DefaultLayerFor(const std::string& category){
    if ( category == "ground" ) { return CraftLayer_Ground; }
    if ( category == "backdrop" || category == "builtin" ) { return CraftLayer_Backdrop; }
    return CraftLayer_Decor;
}
