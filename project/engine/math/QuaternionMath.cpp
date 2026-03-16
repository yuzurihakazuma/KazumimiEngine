#include "QuaternionMath.h"
#include "engine/math/VectorMath.h" 
#include <cmath>

namespace QuaternionMath{

	float Clamp(float x, float a, float b){
		return ( x < a ) ? a : ( x > b ? b : x );
	}

	Quaternion IdentityQuaternion(){
		return { 0.0f, 0.0f, 0.0f, 1.0f };
	}

	float Dot(const Quaternion& a, const Quaternion& b){
		return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
	}

	float Norm(const Quaternion& q){
		return std::sqrt(Dot(q, q));
	}

	Quaternion Normalize(const Quaternion& q){
		float n = Norm(q);
		if ( n <= 0.0f ) return IdentityQuaternion();
		float inv = 1.0f / n;
		return { q.x * inv, q.y * inv, q.z * inv, q.w * inv };
	}

	Quaternion Conjugate(const Quaternion& q){
		return { -q.x, -q.y, -q.z, q.w };
	}

	Quaternion Inverse(const Quaternion& q){
		float n2 = Dot(q, q);
		if ( n2 <= 0.0f ) return IdentityQuaternion();
		Quaternion c = Conjugate(q);
		float inv = 1.0f / n2;
		return { c.x * inv, c.y * inv, c.z * inv, c.w * inv };
	}

	Quaternion Multiply(const Quaternion& lhs, const Quaternion& rhs){
		float x = lhs.w * rhs.x + rhs.w * lhs.x + ( lhs.y * rhs.z - lhs.z * rhs.y );
		float y = lhs.w * rhs.y + rhs.w * lhs.y + ( lhs.z * rhs.x - lhs.x * rhs.z );
		float z = lhs.w * rhs.z + rhs.w * lhs.z + ( lhs.x * rhs.y - lhs.y * rhs.x );
		float w = lhs.w * rhs.w - ( lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z );
		return { x, y, z, w };
	}

	Quaternion MakeRotateAxisAngleQuaternion(const Vector3& axisRaw, float angle){
		Vector3 axis = VectorMath::Normalize(axisRaw); 
		float half = 0.5f * angle;
		float s = std::sin(half);
		float c = std::cos(half);
		Quaternion q { axis.x * s, axis.y * s, axis.z * s, c };
		return Normalize(q);
	}

	Quaternion Slerp(const Quaternion& q0, const Quaternion& q1, float t){
		float dot = Dot(q0, q1);
		Quaternion q1p = q1;
		if ( dot < 0.0f ) {
			dot = -dot;
			q1p = { -q1.x, -q1.y, -q1.z, -q1.w };
		}

		const float EPS = 1e-6f;
		if ( 1.0f - dot < 1e-5f ) {
			float s0 = 1.0f - t;
			float s1 = t;
			Quaternion q {
				s0 * q0.x + s1 * q1p.x,
				s0 * q0.y + s1 * q1p.y,
				s0 * q0.z + s1 * q1p.z,
				s0 * q0.w + s1 * q1p.w
			};
			return Normalize(q);
		}

		float theta = std::acos(Clamp(dot, -1.0f, 1.0f));
		float sinTheta = std::sin(theta);
		float s0 = std::sin(( 1.0f - t ) * theta) / ( sinTheta + EPS );
		float s1 = std::sin(t * theta) / ( sinTheta + EPS );

		Quaternion q {
			s0 * q0.x + s1 * q1p.x,
			s0 * q0.y + s1 * q1p.y,
			s0 * q0.z + s1 * q1p.z,
			s0 * q0.w + s1 * q1p.w
		};
		return Normalize(q);
	}

	Matrix4x4 MakeRotateMatrix(const Quaternion& quaternion){
		Quaternion q = Normalize(quaternion);
		float x = q.x, y = q.y, z = q.z, w = q.w;

		float xx = x * x, yy = y * y, zz = z * z;
		float xy = x * y, xz = x * z, yz = y * z;
		float wx = w * x, wy = w * y, wz = w * z;

		Matrix4x4 M {}; // 初期化
		M.m[0][0] = 1.0f - 2.0f * ( yy + zz ); M.m[0][1] = 2.0f * ( xy + wz );        M.m[0][2] = 2.0f * ( xz - wy );        M.m[0][3] = 0.0f;
		M.m[1][0] = 2.0f * ( xy - wz );        M.m[1][1] = 1.0f - 2.0f * ( xx + zz ); M.m[1][2] = 2.0f * ( yz + wx );        M.m[1][3] = 0.0f;
		M.m[2][0] = 2.0f * ( xz + wy );        M.m[2][1] = 2.0f * ( yz - wx );        M.m[2][2] = 1.0f - 2.0f * ( xx + yy ); M.m[2][3] = 0.0f;
		M.m[3][0] = 0.0f;                    M.m[3][1] = 0.0f;                    M.m[3][2] = 0.0f;                    M.m[3][3] = 1.0f;
		return M;
	}
}