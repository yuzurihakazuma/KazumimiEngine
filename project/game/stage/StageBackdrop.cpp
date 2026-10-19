#include "game/stage/StageBackdrop.h"

#include "engine/3d/model/Model.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/camera/Camera.h"
#include "engine/graphics/InstancedGroup.h"
#include "engine/math/Matrix4x4.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
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

void StageBackdrop::Build(const json& list, const Vector3& origin, uint32_t noiseSrv){
    Clear();
    if ( !list.is_array() ) { return; }
    // モデルごとに、置く物の行列を集める（World と WorldInverseTranspose は動かないので、ここで計算しておく）
    std::map<std::string, std::vector<Obj3d::TransformationMatrix>> byModel;
    for ( const json& entry : list ) {
        const std::string model = entry.value("model", std::string());
        if ( !ModelManager::GetInstance()->FindModel(model) ) { continue; } // 読んでいないモデルは置かない
        Vector3 position = ReadVector(entry.value("pos", json()), { 0.0f, 0.0f, 0.0f });
        if ( entry.contains("offset") ) {
            const Vector3 offset = ReadVector(entry["offset"], { 0.0f, 0.0f, 0.0f });
            position = { origin.x + offset.x, origin.y + offset.y, origin.z + offset.z };
        }
        const Vector3 scale = ReadVector(entry.value("scale", json(1.0f)), { 1.0f, 1.0f, 1.0f });
        // 向き（度）："rot" [x, y, z]（裏返して置く物など）か、"rotY"（Y 回りだけ）
        Vector3 rotation = { 0.0f, entry.value("rotY", 180.0f), 0.0f };
        if ( entry.contains("rot") ) { rotation = ReadVector(entry["rot"], rotation); }
        rotation = { rotation.x * kPi / 180.0f, rotation.y * kPi / 180.0f, rotation.z * kPi / 180.0f };
        Obj3d::TransformationMatrix matrix {};
        matrix.World = MatrixMath::MakeAffine(scale, rotation, position); // Obj3d::Update と同じ作り方
        matrix.WorldInverseTranspose = MatrixMath::Transpose(MatrixMath::Inverse(matrix.World));
        matrix.WVP = matrix.World;
        byModel[model].push_back(matrix);
    }
    for ( auto& [model, matrices] : byModel ) {
        Group group;
        group.model = model;
        group.matrices = std::move(matrices);
        group.batch = std::make_unique<InstancedGroup>();
        group.batch->Initialize(model, ( uint32_t ) group.matrices.size());
        group.batch->SetNoiseTexture(noiseSrv);
        groups_.push_back(std::move(group));
    }
}

void StageBackdrop::Clear(){ groups_.clear(); }

void StageBackdrop::Update(const Camera* camera){
    if ( !camera ) { return; }
    const Matrix4x4& viewProjection = camera->GetViewProjectionMatrix();
    for ( Group& group : groups_ ) {
        for ( Obj3d::TransformationMatrix& matrix : group.matrices ) {
            matrix.WVP = MatrixMath::Multiply(matrix.World, viewProjection);
        }
        group.batch->Update(group.matrices);
    }
}

void StageBackdrop::Draw(const Camera* camera){
    for ( Group& group : groups_ ) { group.batch->Draw(camera); }
}

size_t StageBackdrop::GetObjectCount() const{
    size_t count = 0;
    for ( const Group& group : groups_ ) { count += group.matrices.size(); }
    return count;
}
