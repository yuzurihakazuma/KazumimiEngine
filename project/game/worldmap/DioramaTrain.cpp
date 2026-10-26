#include "game/worldmap/DioramaTrain.h"

#include "engine/3d/model/Model.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/3d/obj/Obj3d.h"
#include "engine/math/Matrix4x4.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr const char* kFeltTexture = "resources/road/cloth/cloth_felt_plain.png";
    constexpr float kFeltBrightness = 0.46f; // フェルトの画像の平均の明るさ（linear）

    const Vector3 kDark { 0.22f, 0.2f, 0.22f };
    const Vector3 kGreen { 0.36f, 0.72f, 0.32f };
    const Vector3 kRed { 0.86f, 0.27f, 0.22f };
    const Vector3 kYellow { 0.97f, 0.78f, 0.28f };
    const Vector3 kBlue { 0.45f, 0.7f, 0.92f };
    const Vector3 kCream { 0.98f, 0.95f, 0.86f };

    // 形（box＝1m の箱 / tube＝直径1m・高さ1m の筒 / ball＝半径1m の球）と色ごとに、フェルトの模型を1つ作る
    std::string FeltModel(const char* shape, const Vector3& color){
        char name[96];
        std::snprintf(name, sizeof(name), "wm_%s_%.2f_%.2f_%.2f", shape, color.x, color.y, color.z);
        ModelManager* modelManager = ModelManager::GetInstance();
        if ( !modelManager->FindModel(name) ) {
            const std::string kind = shape;
            if ( kind == "box" )       { modelManager->CreateCubeModel(name, 1.0f); }
            else if ( kind == "tube" ) { modelManager->CreateCylinderModel(name, 20, 0.5f, 1.0f); }
            else                       { modelManager->CreateSphereModel(name, 16); }
            if ( Model* model = modelManager->FindModel(name) ) {
                model->SetTexture(kFeltTexture);
                if ( model->GetMaterial() ) {
                    // フェルトの画像は灰色（明るさ約 0.46）なので、その分を戻す。色は sRGB で書いて、ここで linear へ
                    auto linear = [](float c){ return std::pow(c, 2.2f) / kFeltBrightness; };
                    model->GetMaterial()->color = { linear(color.x), linear(color.y), linear(color.z), 1.0f };
                    model->GetMaterial()->matte = 1.0f; // フェルトなので照り返しなし
                }
            }
        }
        return name;
    }
}

Vector3 DioramaRoot::Apply(const Vector3& local) const{
    const Vector3 scaled { ( local.x - pivot.x ) * scale, ( local.y - pivot.y ) * scale, ( local.z - pivot.z ) * scale };
    const Vector3 turned = MatrixMath::Transforms(scaled, MatrixMath::MakeRotateY(yaw));
    return { top.x + turned.x, top.y + turned.y, top.z + turned.z };
}

Vector3 DioramaRoot::Translation() const{
    const Vector3 turned = MatrixMath::Transforms({ pivot.x * scale, pivot.y * scale, pivot.z * scale }, MatrixMath::MakeRotateY(yaw));
    return { top.x - turned.x, top.y - turned.y, top.z - turned.z };
}

DioramaTrain::DioramaTrain() = default;
DioramaTrain::~DioramaTrain() = default;

void DioramaTrain::AddPart(Car& car, const char* shape, const Vector3& color, const Vector3& offset, const Vector3& size, float pitch){
    Part part;
    part.obj = Obj3d::Create(FeltModel(shape, color));
    if ( !part.obj ) { return; }
    part.obj->SetEnvironmentMap(envSrv_);
    part.offset = offset;
    // 球は半径1m なので半分に
    part.size = std::string(shape) == "ball" ? Vector3 { size.x * 0.5f, size.y * 0.5f, size.z * 0.5f } : size;
    part.pitch = pitch;
    car.parts.push_back(std::move(part));
}

void DioramaTrain::Build(const SplineRail& loop, uint32_t envSrv){
    Clear();
    loop_ = loop;
    envSrv_ = envSrv;
    const float halfPi = kPi * 0.5f;

    // 機関車（緑の釜・赤い運転室・黒い煙突）
    Car engine;
    engine.behind = 0.0f;
    AddPart(engine, "box", kDark, { 0.0f, 0.45f, 0.0f }, { 1.5f, 0.35f, 3.4f });
    AddPart(engine, "tube", kGreen, { 0.0f, 1.15f, 0.45f }, { 1.15f, 2.0f, 1.15f }, halfPi);
    AddPart(engine, "ball", kGreen, { 0.0f, 1.15f, 1.45f }, { 1.15f, 1.15f, 0.35f });
    AddPart(engine, "box", kRed, { 0.0f, 1.35f, -0.95f }, { 1.6f, 1.5f, 1.2f });
    AddPart(engine, "box", kDark, { 0.0f, 2.15f, -0.95f }, { 1.85f, 0.15f, 1.45f });
    AddPart(engine, "box", kCream, { 0.81f, 1.55f, -0.95f }, { 0.04f, 0.55f, 0.6f });
    AddPart(engine, "box", kCream, { -0.81f, 1.55f, -0.95f }, { 0.04f, 0.55f, 0.6f });
    AddPart(engine, "tube", kDark, { 0.0f, 2.0f, 1.0f }, { 0.42f, 0.8f, 0.42f });
    AddPart(engine, "ball", kYellow, { 0.0f, 1.3f, 1.66f }, { 0.36f, 0.36f, 0.16f });
    for ( float z : { 1.05f, 0.0f, -1.05f } ) {
        for ( float x : { 0.8f, -0.8f } ) { AddPart(engine, "ball", kDark, { x, 0.45f, z }, { 0.18f, 0.82f, 0.82f }); }
    }
    cars_.push_back(std::move(engine));

    // 客車2両（黄色に赤い屋根 / 水色に黄色い屋根）
    const std::pair<Vector3, Vector3> coaches[] = { { kYellow, kRed }, { kBlue, kYellow } };
    float behind = 3.5f;
    for ( const auto& [body, roof] : coaches ) {
        Car coach;
        coach.behind = behind;
        AddPart(coach, "box", kDark, { 0.0f, 0.45f, 0.0f }, { 1.5f, 0.3f, 3.0f });
        AddPart(coach, "box", body, { 0.0f, 1.25f, 0.0f }, { 1.6f, 1.3f, 2.8f });
        AddPart(coach, "box", roof, { 0.0f, 1.98f, 0.0f }, { 1.85f, 0.18f, 3.1f });
        for ( float z : { 0.75f, -0.75f } ) {
            for ( float x : { 0.81f, -0.81f } ) { AddPart(coach, "box", kCream, { x, 1.4f, z }, { 0.04f, 0.5f, 0.75f }); }
        }
        for ( float z : { 0.95f, -0.95f } ) {
            for ( float x : { 0.8f, -0.8f } ) { AddPart(coach, "ball", kDark, { x, 0.45f, z }, { 0.18f, 0.78f, 0.78f }); }
        }
        cars_.push_back(std::move(coach));
        behind += 3.3f;
    }
    distance_ = 0.0f;
}

void DioramaTrain::Clear(){
    cars_.clear();
    hasChimney_ = false;
    loop_ = SplineRail {};
}

void DioramaTrain::Update(float deltaTime, float speed, const DioramaRoot& root){
    const float length = loop_.GetLength();
    if ( cars_.empty() || length <= 0.0f ) { return; }
    distance_ = std::fmod(distance_ + speed * deltaTime, length);
    for ( Car& car : cars_ ) {
        float d = std::fmod(distance_ - car.behind, length);
        if ( d < 0.0f ) { d += length; }
        const Vector3 position = loop_.GetPositionByDistance(d);
        const Vector3 tangent = loop_.GetTangentByDistance(d);
        const float carYaw = std::atan2(tangent.x, tangent.z);
        const Matrix4x4 turn = MatrixMath::MakeRotateY(carYaw);
        if ( &car == &cars_.front() ) {
            // 機関車の煙突の口（けむりを出す所）
            const Vector3 top = MatrixMath::Transforms({ 0.0f, 2.45f, 1.0f }, turn);
            chimney_ = root.Apply({ position.x + top.x, position.y + top.y, position.z + top.z });
            hasChimney_ = true;
        }
        for ( Part& part : car.parts ) {
            const Vector3 offset = MatrixMath::Transforms(part.offset, turn);
            part.obj->SetTranslation(root.Apply({ position.x + offset.x, position.y + offset.y, position.z + offset.z }));
            part.obj->SetRotation({ part.pitch, carYaw + root.yaw, 0.0f });
            part.obj->SetScale({ part.size.x * root.scale, part.size.y * root.scale, part.size.z * root.scale });
            part.obj->Update();
        }
    }
}

void DioramaTrain::Draw(){
    for ( Car& car : cars_ ) {
        for ( Part& part : car.parts ) { part.obj->Draw(); }
    }
}
