#include "game/stage/StageBackdrop.h"

#include "engine/3d/model/Model.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/3d/obj/Obj3d.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

using json = nlohmann::json;

namespace {
    constexpr const char* kKitModelDir = "resources/craft/models";
    constexpr float kPi = 3.14159265358979323846f;

    Vector3 ReadVector(const json& value, const Vector3& fallback){
        if ( value.is_number() ) { const float v = value.get<float>(); return { v, v, v }; }
        if ( !value.is_array() || value.size() < 3 ) { return fallback; }
        return { value[0].get<float>(), value[1].get<float>(), value[2].get<float>() };
    }
}

StageBackdrop::StageBackdrop() = default;
StageBackdrop::~StageBackdrop() = default;

void StageBackdrop::LoadKitModels(){
    namespace fs = std::filesystem;
    ModelManager* modelManager = ModelManager::GetInstance();
    std::error_code errorCode;
    for ( const auto& entry : fs::directory_iterator(kKitModelDir, errorCode) ) {
        if ( !entry.is_regular_file() ) { continue; }
        std::string extension = entry.path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](unsigned char character){ return ( char ) std::tolower(character); });
        if ( extension != ".gltf" ) { continue; }
        const std::string modelName = entry.path().stem().string();
        modelManager->LoadModel(modelName, kKitModelDir, entry.path().filename().string(), true); // 底面原点のまま
        // 紙・フェルト・段ボールなので、白い照り返しを出さない
        if ( Model* model = modelManager->FindModel(modelName) ) {
            if ( model->GetMaterial() ) { model->GetMaterial()->matte = 1.0f; }
        }
    }
}

void StageBackdrop::Add(const std::string& model, const Vector3& position, float rotationY, const Vector3& scale){
    if ( !ModelManager::GetInstance()->FindModel(model) ) { return; } // 読んでいないモデルは置かない
    std::unique_ptr<Obj3d> object = Obj3d::Create(model, position, { 0.0f, rotationY, 0.0f }, scale);
    if ( object ) { objects_.push_back(std::move(object)); }
}

void StageBackdrop::Build(const json& list, const Vector3& origin){
    Clear();
    if ( !list.is_array() ) { return; }
    for ( const json& entry : list ) {
        const std::string model = entry.value("model", std::string());
        const Vector3 offset = ReadVector(entry.value("offset", json()), { 0.0f, 0.0f, 0.0f });
        const Vector3 scale = ReadVector(entry.value("scale", json(1.0f)), { 1.0f, 1.0f, 1.0f });
        const float rotationY = entry.value("rotY", 180.0f) * kPi / 180.0f;
        if ( entry.contains("grid") ) {
            // 並べる（地面のタイルなど）：x と z の範囲を間隔ごとに
            const json& grid = entry["grid"];
            const Vector3 rangeX = ReadVector(grid.value("x", json()), { 0.0f, 0.0f, 1.0f });
            const Vector3 rangeZ = ReadVector(grid.value("z", json()), { 0.0f, 0.0f, 1.0f });
            for ( float x = rangeX.x; x <= rangeX.y + 1e-3f; x += ( std::max )( rangeX.z, 0.1f ) ) {
                for ( float z = rangeZ.x; z <= rangeZ.y + 1e-3f; z += ( std::max )( rangeZ.z, 0.1f ) ) {
                    Add(model, { origin.x + offset.x + x, origin.y + offset.y, origin.z + offset.z + z }, rotationY, scale);
                }
            }
            continue;
        }
        Add(model, { origin.x + offset.x, origin.y + offset.y, origin.z + offset.z }, rotationY, scale);
    }
}

void StageBackdrop::Clear(){ objects_.clear(); }

void StageBackdrop::Update(){
    for ( auto& object : objects_ ) { object->Update(); }
}

void StageBackdrop::Draw(){
    for ( auto& object : objects_ ) { object->Draw(); }
}
