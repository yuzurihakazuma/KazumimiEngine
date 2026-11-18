#include "game/clear/ClearCamera.h"
#include "engine/math/MathUtil.h"

#include "engine/camera/Camera.h"
#include "engine/math/VectorMath.h"
#include "engine/utils/Easing.h"

#include <cmath>

using namespace KazumimiEngine;

ClearCamera::Orbit ClearCamera::ToOrbit(const Vector3& position, const Vector3& pivot){
    const Vector3 offset = position - pivot;
    Orbit orbit;
    orbit.angle = std::atan2(offset.x, offset.z);
    orbit.radius = std::sqrt(offset.x * offset.x + offset.z * offset.z);
    orbit.height = offset.y;
    return orbit;
}

void ClearCamera::SetEnd(const Vector3& endPosition, const Vector3& endLook, float endFovY){
    end_ = ToOrbit(endPosition, pivot_);
    endPosition_ = endPosition;
    endLook_ = endLook;
    endFovY_ = endFovY;
}

void ClearCamera::Begin(const Camera& camera, const Vector3& pivot){
    pivot_ = pivot;
    const Vector3 startPosition = camera.GetWorldPosition();
    start_ = ToOrbit(startPosition, pivot);
    startFovY_ = camera.GetFovY();
    // 終点は SetEnd が決める。それまでは、動かない（始まりと同じ）
    end_ = start_;
    endPosition_ = startPosition;
    endFovY_ = startFovY_;
    // 始めに見ている点：今のカメラの向きの先で、中心と同じくらいの距離の所
    const Vector3& rotation = camera.GetRotation();
    const Vector3 forward { std::sin(rotation.y) * std::cos(rotation.x), -std::sin(rotation.x), std::cos(rotation.y) * std::cos(rotation.x) };
    const float distance = VectorMath::Length(pivot - startPosition);
    startLook_ = startPosition + forward * distance;
    endLook_ = startLook_;
}

void ClearCamera::Apply(Camera* camera, float progress) const{
    if ( !camera ) { return; }
    const float eased = Easing::EaseInOutCubic(progress);
    Orbit orbit;
    orbit.angle = start_.angle + Math::AngleDelta(start_.angle, end_.angle) * eased; // 近い回り方で
    orbit.radius = Math::Lerp(start_.radius, end_.radius, eased);
    orbit.height = Math::Lerp(start_.height, end_.height, eased);
    const Vector3 position { pivot_.x + std::sin(orbit.angle) * orbit.radius, pivot_.y + orbit.height,
                             pivot_.z + std::cos(orbit.angle) * orbit.radius };
    camera->LookAt(position, Math::Lerp(startLook_, endLook_, eased));
    camera->SetFovY(Math::Lerp(startFovY_, endFovY_, eased));
}
