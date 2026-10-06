#include "game/craft/CraftLighting.h"

#include "engine/math/VectorMath.h"

namespace {
    struct Profile {
        const char* name;
        Vector3 direction;   // 光が進む向き
        Vector4 color;
        float   intensity;
    };
    const Profile kProfileTable[] = {
        { "day",     { 0.35f, -0.7f, 0.6f }, { 1.0f, 0.97f, 0.9f, 1.0f },  1.25f }, // 昼：手前上から白っぽく（タイトルと同じ）
        { "evening", { 0.8f, -0.35f, 0.3f }, { 1.0f, 0.72f, 0.48f, 1.0f }, 1.15f }, // 夕方：低い横からオレンジ
    };
}

bool CraftLighting::Apply(const std::string& profileName, Saved& saved){
    const Profile* profile = nullptr;
    for ( const Profile& entry : kProfileTable ) {
        if ( profileName == entry.name ) { profile = &entry; }
    }
    Obj3dCommon* obj3dCommon = Obj3dCommon::GetInstance();
    Obj3dCommon::DirectionalLightData* light = obj3dCommon->GetLightData();
    if ( !profile || !light ) { return false; }
    if ( !saved.valid ) {
        saved.directional = *light;
        saved.pointIntensity = obj3dCommon->GetPointLightData()->intensity;
        saved.spotIntensity = obj3dCommon->GetSpotLightData()->intensity;
        saved.valid = true;
    }
    light->activeCount = 1;
    light->lights[0].color = profile->color;
    light->lights[0].direction = VectorMath::Normalize(profile->direction);
    light->lights[0].intensity = profile->intensity;
    obj3dCommon->GetPointLightData()->intensity = 0.0f;
    obj3dCommon->GetSpotLightData()->intensity = 0.0f;
    return true;
}

void CraftLighting::Restore(Saved& saved){
    if ( !saved.valid ) { return; }
    Obj3dCommon* obj3dCommon = Obj3dCommon::GetInstance();
    *obj3dCommon->GetLightData() = saved.directional;
    obj3dCommon->GetPointLightData()->intensity = saved.pointIntensity;
    obj3dCommon->GetSpotLightData()->intensity = saved.spotIntensity;
    saved.valid = false;
}
