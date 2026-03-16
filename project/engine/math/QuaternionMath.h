#pragma once
#include "struct.h" // 構造体の定義

namespace QuaternionMath{

	// 便利関数：クランプ
	float Clamp(float x, float a, float b);

	// Identity（初期値）
	Quaternion IdentityQuaternion();

	// 内積
	float Dot(const Quaternion& a, const Quaternion& b);

	// ノルム(長さ)
	float Norm(const Quaternion& q);

	// 正規化
	Quaternion Normalize(const Quaternion& q);

	// 共役
	Quaternion Conjugate(const Quaternion& q);

	// 逆元
	Quaternion Inverse(const Quaternion& q);

	// 掛算 (Hamilton積)
	Quaternion Multiply(const Quaternion& lhs, const Quaternion& rhs);

	// 任意軸回転を表すQuaternionの生成
	Quaternion MakeRotateAxisAngleQuaternion(const Vector3& axisRaw, float angle);

	// 球面線形補間 (Slerp) - アニメーションで超重要！
	Quaternion Slerp(const Quaternion& q0, const Quaternion& q1, float t);

	// Quaternionから回転行列(Matrix4x4)を作る
	Matrix4x4 MakeRotateMatrix(const Quaternion& quaternion);
}