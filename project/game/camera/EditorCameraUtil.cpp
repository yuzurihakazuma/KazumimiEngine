#include "EditorCameraUtil.h"
#include "engine/camera/Camera.h"
#include "engine/rail/SplineRail.h"

#include <algorithm>
#include <cmath>

namespace {
	// カメラの回転（pitch, yaw）から前方向を求める
	Vector3 ForwardOf(const Camera& camera){
		Vector3 rotation = camera.GetRotation();
		float cosPitch = std::cos(rotation.x);
		return { cosPitch * std::sin(rotation.y), -std::sin(rotation.x), cosPitch * std::cos(rotation.y) };
	}
}

namespace EditorCameraUtil {

void FocusOnRail(Camera& camera, const std::vector<SplineRail>& rails,
                 int railIndex, float distance, float height){
	if ( railIndex < 0 || railIndex >= ( int ) rails.size() || rails[railIndex].nodes.size() < 2 ) return;
	Vector3 target = rails[railIndex].GetPositionByDistance(distance);
	target.y += height;
	Vector3 forward = ForwardOf(camera);
	const float kFocusDistance = 8.0f;
	camera.SetTranslation({ target.x - forward.x * kFocusDistance,
	                        target.y - forward.y * kFocusDistance,
	                        target.z - forward.z * kFocusDistance });
}

void TopView(Camera& camera, const std::vector<SplineRail>& rails){
	// レール全体のXZ範囲を求めて、真上から全体が収まる高さに置く
	bool hasBounds = false;
	Vector3 minPos {}, maxPos {};
	for ( const auto& rail : rails ) {
		for ( const auto& node : rail.nodes ) {
			if ( !hasBounds ) { minPos = maxPos = node; hasBounds = true; continue; }
			minPos = { ( std::min )( minPos.x, node.x ), ( std::min )( minPos.y, node.y ), ( std::min )( minPos.z, node.z ) };
			maxPos = { ( std::max )( maxPos.x, node.x ), ( std::max )( maxPos.y, node.y ), ( std::max )( maxPos.z, node.z ) };
		}
	}
	Vector3 center { 0.0f, 0.0f, 0.0f };
	float extent = 10.0f;
	if ( hasBounds ) {
		center = { ( minPos.x + maxPos.x ) * 0.5f, ( minPos.y + maxPos.y ) * 0.5f, ( minPos.z + maxPos.z ) * 0.5f };
		extent = ( std::max )( maxPos.x - minPos.x, maxPos.z - minPos.z );
	}
	float cameraHeight = extent * 1.5f + 8.0f; // 全体が画面に収まる高さ（足りなければホイールでズーム）
	camera.SetTranslation({ center.x, center.y + cameraHeight, center.z });
	camera.SetRotation({ 1.5707964f, 0.0f, 0.0f }); // pitch 90°=真下を向く（X=右, Z=上）
}

void DefaultAngle(Camera& camera){
	camera.SetTranslation({ 0.0f, 6.0f, -15.0f });
	camera.SetRotation({ 0.30f, 0.0f, 0.0f });
}

void CenterOnXZ(Camera& camera, float worldX, float worldZ){
	Vector3 camPos = camera.GetWorldPosition();
	Vector3 forward = ForwardOf(camera);
	// 視線がレール高さ(y≈0)へ届く距離ぶん後ろへ引く（真横向きなどの時は20m固定）
	float backDist = ( forward.y < -0.05f ) ? std::clamp(camPos.y / -forward.y, 5.0f, 80.0f) : 20.0f;
	camera.SetTranslation({ worldX - forward.x * backDist, camPos.y, worldZ - forward.z * backDist });
}

} // namespace EditorCameraUtil
