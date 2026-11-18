#include "game/rail/ToyTrain.h"
#include "engine/math/MathUtil.h"

#include "engine/3d/model/Model.h"
#include "engine/3d/model/ModelManager.h"

#include <cmath>
#include <cstdio>
#include <cstring>

using namespace KazumimiEngine;

namespace {
    constexpr const char* kFeltTexture = "resources/road/cloth/cloth_felt_plain.png";
    constexpr float kFeltBrightness = 0.46f; // フェルトの画像の平均の明るさ（linear）

    const Vector3 kDark { 0.22f, 0.2f, 0.22f };
    const Vector3 kGreen { 0.36f, 0.72f, 0.32f };
    const Vector3 kRed { 0.86f, 0.27f, 0.22f };
    const Vector3 kYellow { 0.97f, 0.78f, 0.28f };
    const Vector3 kBlue { 0.45f, 0.7f, 0.92f };
    const Vector3 kCream { 0.98f, 0.95f, 0.86f };
}

std::vector<ToyTrain::Part> ToyTrain::Engine(){
    const float halfPi = Math::kPi * 0.5f;
    std::vector<Part> parts = {
        { "box",  kDark,   { 0.0f, 0.45f, 0.0f },   { 1.5f, 0.35f, 3.4f },   0.0f },
        { "tube", kGreen,  { 0.0f, 1.15f, 0.45f },  { 1.15f, 2.0f, 1.15f },  halfPi },
        { "ball", kGreen,  { 0.0f, 1.15f, 1.45f },  { 1.15f, 1.15f, 0.35f }, 0.0f },
        { "box",  kRed,    { 0.0f, 1.35f, -0.95f }, { 1.6f, 1.5f, 1.2f },    0.0f },
        { "box",  kDark,   { 0.0f, 2.15f, -0.95f }, { 1.85f, 0.15f, 1.45f }, 0.0f },
        { "box",  kCream,  { 0.81f, 1.55f, -0.95f },  { 0.04f, 0.55f, 0.6f }, 0.0f },
        { "box",  kCream,  { -0.81f, 1.55f, -0.95f }, { 0.04f, 0.55f, 0.6f }, 0.0f },
        { "tube", kDark,   { 0.0f, 2.0f, 1.0f },    { 0.42f, 0.8f, 0.42f },  0.0f },
        { "ball", kYellow, { 0.0f, 1.3f, 1.66f },   { 0.36f, 0.36f, 0.16f }, 0.0f },
    };
    for ( float z : { 1.05f, 0.0f, -1.05f } ) {
        for ( float x : { kWheelX, -kWheelX } ) { parts.push_back({ "ball", kDark, { x, 0.45f, z }, { 0.18f, 0.82f, 0.82f }, 0.0f }); }
    }
    return parts;
}

std::vector<ToyTrain::Part> ToyTrain::Coach(int index){
    const bool first = ( index % 2 ) == 0;
    const Vector3 body = first ? kYellow : kBlue;
    const Vector3 roof = first ? kRed : kYellow;
    std::vector<Part> parts = {
        { "box", kDark, { 0.0f, 0.45f, 0.0f }, { 1.5f, 0.3f, 3.0f },   0.0f },
        { "box", body,  { 0.0f, 1.25f, 0.0f }, { 1.6f, 1.3f, 2.8f },   0.0f },
        { "box", roof,  { 0.0f, 1.98f, 0.0f }, { 1.85f, 0.18f, 3.1f }, 0.0f },
    };
    for ( float z : { 0.75f, -0.75f } ) {
        for ( float x : { 0.81f, -0.81f } ) { parts.push_back({ "box", kCream, { x, 1.4f, z }, { 0.04f, 0.5f, 0.75f }, 0.0f }); }
    }
    for ( float z : { 0.95f, -0.95f } ) {
        for ( float x : { kWheelX, -kWheelX } ) { parts.push_back({ "ball", kDark, { x, 0.45f, z }, { 0.18f, 0.78f, 0.78f }, 0.0f }); }
    }
    return parts;
}

// 形（box＝1m の箱 / tube＝直径1m・高さ1m の筒 / ball＝半径1m の球）と色ごとに、フェルトの模型を1つ作る
std::string ToyTrain::FeltModel(const char* shape, const Vector3& color){
    char name[96];
    std::snprintf(name, sizeof(name), "wm_%s_%.2f_%.2f_%.2f", shape, color.x, color.y, color.z);
    ModelManager* modelManager = ModelManager::GetInstance();
    if ( !modelManager->FindModel(name) ) {
        if ( std::strcmp(shape, "box") == 0 )       { modelManager->CreateCubeModel(name, 1.0f); }
        else if ( std::strcmp(shape, "tube") == 0 ) { modelManager->CreateCylinderModel(name, 20, 0.5f, 1.0f); }
        else                                        { modelManager->CreateSphereModel(name, 16); }
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

Vector3 ToyTrain::ModelScale(const Part& part){
    if ( std::strcmp(part.shape, "ball") == 0 ) { return { part.size.x * 0.5f, part.size.y * 0.5f, part.size.z * 0.5f }; }
    return part.size;
}
