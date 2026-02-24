#include "VectorMath.h"
#include <cmath>

namespace VectorMath {

	Vector3 Add(const Vector3& v1, const Vector3& v2) {
		return v1 + v2;
	}

	Vector3 Subtract(const Vector3& v1, const Vector3& v2) {
		return v1 - v2;
	}

	Vector3 Multiply(float scalar, const Vector3& v) {
		return v * scalar;
	}

	float Dot(const Vector3& v1, const Vector3& v2) {
		return (v1.x * v2.x + v1.y * v2.y + v1.z * v2.z);
	}

	Vector3 Cross(const Vector3& a, const Vector3& b) {
		return {
			a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x
		};
	}

	float Length(const Vector3& v) {
		return std::sqrt(Dot(v, v));
	}

	Vector3 Normalize(const Vector3& v) {
		float len = Length(v);
		if (len != 0.0f) {
			return v / len;
		}
		return v;
	}

} // namespace VectorMath