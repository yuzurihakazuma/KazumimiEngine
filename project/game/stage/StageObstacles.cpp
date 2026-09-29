#include "StageObstacles.h"
#include "game/stage/BlockSystem.h"
#include "engine/rail/SplineRail.h"

#include <algorithm>
#include <cmath>

bool StageObstacles::Sweep(const Vector3& from, const Vector3& to, float radius, Vector3& outHitPos) const{
	// 1) ブロック（見た目と同じ、道に沿って傾いた箱）
	if ( blocks_ && blocks_->SweepSphere(from, to, radius, &outHitPos) ) { return true; }
	// 2) 道の上面
	return HitRoadSurface(to, radius, outHitPos);
}

bool StageObstacles::HitRoadSurface(const Vector3& to, float radius, Vector3& outHitPos) const{
	if ( !rails_ ) return false;
	const float kRoadHalfWidth = 1.0f;  // 道の半幅（RoadMesh と同じ）
	const float kRoadThickness = 0.5f;  // 上面からこの深さまでを道の厚みとして扱う
	const int   kSampleStride  = 2;     // 道の位置テーブル（0.25m刻み）を何個おきに見るか＝0.5m
	// 各サンプルが受け持つ前後の幅。サンプルの間隔は0.5m。きついカーブの外側では隣のサンプルとの間が
	//   広がるので、半分(0.25)より大きめにとって隙間を作らない（レールの端・穴は下の underDist で別に見る）
	const float kNearAlong     = 0.45f;

	for ( const SplineRail& rail : *rails_ ) {
		if ( rail.nodes.size() < 2 || !rail.visible ) continue; // 見えない連結レールに道は無い
		if ( rail.roadMode != 0 ) continue;                      // 道なしのレール
		if ( rail.IsRideBlocked() ) continue;                    // まだ出現していない道
		if ( rail.frameCache_.empty() ) continue;

		// 列車式で回っている最中のレールは、位置テーブル（基準位置）が使えないので都度計算する
		const bool rotating = ( rail.guideAlign != 0 && rail.animYaw != 0.0f );
		const int frameCount = ( int ) rail.frameCache_.size();
		for ( int i = 0; i < frameCount; i += kSampleStride ) {
			float railDist = ( float ) i * SplineRail::kFrameStep;
			SplineRail::RailFrame frame = rail.frameCache_[i];
			Vector3 surface;
			if ( rotating ) {
				surface = rail.GetPositionByDistance(railDist);
				frame   = rail.GetFrameAtDistance(railDist);
				Vector3 tangent = rail.GetTangentByDistance(railDist);
				float horiz = std::sqrt(tangent.x * tangent.x + tangent.z * tangent.z);
				if ( horiz > 1e-4f ) { frame.right = { tangent.z / horiz, 0.0f, -tangent.x / horiz }; }
				frame.tangent = tangent;
			} else {
				surface = { frame.position.x + rail.animOffset.x,
				            frame.position.y + rail.animOffset.y,
				            frame.position.z + rail.animOffset.z };
			}
			Vector3 delta = { to.x - surface.x, to.y - surface.y, to.z - surface.z };
			// 遠いサンプルは先に捨てる
			if ( std::abs(delta.x) > 2.0f || std::abs(delta.y) > 2.0f || std::abs(delta.z) > 2.0f ) continue;
			float along = delta.x * frame.tangent.x + delta.y * frame.tangent.y + delta.z * frame.tangent.z;
			if ( std::abs(along) > kNearAlong ) continue;
			float lateral = delta.x * frame.right.x + delta.y * frame.right.y + delta.z * frame.right.z;
			if ( std::abs(lateral) > kRoadHalfWidth + radius * 0.5f ) continue;
			float height = delta.x * frame.up.x + delta.y * frame.up.y + delta.z * frame.up.z;
			if ( height > radius || height < -kRoadThickness - radius ) continue;
			// 弾の真下にあたるレール上の距離。レールの端より先・穴の区間は道が無いので素通り
			//   （サンプルの距離で見ると、端や穴の手前のサンプルが先まで道を伸ばしてしまう）
			const float underDist = ( std::min )( railDist, rail.GetLength() ) + along;
			if ( underDist < -0.05f || underDist > rail.GetLength() + 0.05f ) continue;
			if ( rail.IsHoleAtDistance(std::clamp(underDist, 0.0f, rail.GetLength())) ) continue;
			// 当たった：道の上面に乗る高さへ戻した位置を返す
			float lift = ( height >= -kRoadThickness * 0.5f ) ? ( radius - height ) : 0.0f;
			outHitPos = { to.x + frame.up.x * lift, to.y + frame.up.y * lift, to.z + frame.up.z * lift };
			return true;
		}
	}
	return false;
}
