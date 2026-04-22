#include "game/player/Player.h"
#include "Engine/Base/Input.h"
#include "engine/math/VectorMath.h"
#include "engine/3d/SplineRail.h"
#include <algorithm>
#include <cmath>

using namespace VectorMath;

void Player::Initialize() {
    position_ = { 0.0f, 0.0f, 0.0f };
	rotation_ = { 0.0f, 0.0f, 0.0f };
    scale_ = { 1.0f, 1.0f, 1.0f };
	currentT_ = 0.0f;
}

void Player::Update(const SplineRail& currentRail){

	UpdateRailMovement(currentRail);



}

// レールに沿って移動する関数
void Player::UpdateRailMovement(const SplineRail& rail){

	if ( rail.nodes.size() < 2 ){ return; }

	Input* input = Input::GetInstance();
	float maxT = static_cast< float >( rail.nodes.size() - 1 );

	// 入力に応じてcurrentT_を増減させる

	if ( input->Pushkey(DIK_D) ){
		currentT_ += moveSpeed_ * ( 0.1f / 60.0f );
	}
	if ( input->Pushkey(DIK_A) ){
		currentT_ -= moveSpeed_ * ( 0.1f / 60.0f );
	}
	// currentT_が0未満にならないように、maxTを超えないようにする
	currentT_ = std::clamp(currentT_, 0.0f, maxT);

	// currentT_に応じた位置を計算してposition_にセットする
	position_ = rail.EvaluatePosition(currentT_);
	
	
	float nextT = std::min(currentT_ + 0.01f, maxT);
	// nextTに応じた位置を計算する
	if ( nextT> currentT_ ){

		// nextTに応じた位置を計算する
		Vector3 nextPos = rail.EvaluatePosition(nextT);

		// position_からnextPosへのベクトルを計算して正規化する
		Vector3 forward = Normalize(Subtract(nextPos, position_));


		rotation_.y = std::atan2(forward.x, forward.z);
		rotation_.x = std::asin(-forward.y);
		rotation_.z = 0.0f;


	}


}
