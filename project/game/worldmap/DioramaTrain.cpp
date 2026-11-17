#include "game/worldmap/DioramaTrain.h"
#include "engine/math/MathUtil.h"
#include "game/rail/ToyTrain.h"

#include "engine/3d/obj/Obj3d.h"
#include "engine/math/Matrix4x4.h"

#include <cmath>

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

void DioramaTrain::AddPart(Car& car, const ToyTrain::Part& def){
    Part part;
    part.obj = Obj3d::Create(ToyTrain::FeltModel(def.shape, def.color));
    if ( !part.obj ) { return; }
    part.obj->SetEnvironmentMap(envSrv_);
    part.offset = def.offset;
    part.size = ToyTrain::ModelScale(def);
    part.pitch = def.pitch;
    car.parts.push_back(std::move(part));
}

void DioramaTrain::Build(const SplineRail& loop, uint32_t envSrv){
    Clear();
    loop_ = loop;
    envSrv_ = envSrv;
    // 機関車＋客車2両（形は ToyTrain。ステージの列車と同じ）
    Car engine;
    engine.behind = 0.0f;
    for ( const ToyTrain::Part& part : ToyTrain::Engine() ) { AddPart(engine, part); }
    cars_.push_back(std::move(engine));
    float behind = 3.5f;
    for ( int i = 0; i < 2; ++i ) {
        Car coach;
        coach.behind = behind;
        for ( const ToyTrain::Part& part : ToyTrain::Coach(i) ) { AddPart(coach, part); }
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
            const Vector3 top = MatrixMath::Transforms(ToyTrain::kChimneyTop, turn);
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
