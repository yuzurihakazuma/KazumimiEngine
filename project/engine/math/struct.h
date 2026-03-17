#pragma once

// -------------------------------------------------
// 構造体の定義 (データ型のみ)
// -------------------------------------------------

// 2次元ベクトル
struct Vector2 {
	float x, y;
};

// 3次元ベクトル
struct Vector3 {
	float x, y, z;
};

// 4次元ベクトル
struct Vector4 {
	float x, y, z, w;
};
// クォータニオン
struct Quaternion{
	float x, y, z, w;
};


// 3x3行列
struct Matrix3x3 {
	float m[3][3];
};

// 4x4行列
struct Matrix4x4 {
	float m[4][4];
};

// 変換行列用データ
struct Transform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

// 球体
struct Sphere {
	Vector3 center; // 中心点
	float radius;   // 半径
};

// -------------------------------------------------
// 演算子のオーバーロード (便利なようにinlineで定義)
// -------------------------------------------------

// --- 複合代入演算子 (+=, -=, *=, /=) ---
inline constexpr Vector3& operator+=(Vector3& lhs, const Vector3& rhs) noexcept {
	lhs.x += rhs.x; lhs.y += rhs.y; lhs.z += rhs.z; return lhs;
}
inline constexpr Vector3& operator-=(Vector3& lhs, const Vector3& rhs) noexcept {
	lhs.x -= rhs.x; lhs.y -= rhs.y; lhs.z -= rhs.z; return lhs;
}
inline constexpr Vector3& operator*=(Vector3& v, float s) noexcept {
	v.x *= s; v.y *= s; v.z *= s; return v;
}
inline constexpr Vector3& operator/=(Vector3& v, float s) noexcept {
	v.x /= s; v.y /= s; v.z /= s; return v;
}

// --- 単項マイナス (-) ---
inline constexpr Vector3 operator-(const Vector3& v) noexcept {
	return { -v.x, -v.y, -v.z };
}

// --- 二項演算子 (+, -, *, /) ---
inline constexpr Vector3 operator+(const Vector3& v1, const Vector3& v2) noexcept {
	return { v1.x + v2.x, v1.y + v2.y, v1.z + v2.z };
}
inline constexpr Vector3 operator-(const Vector3& v1, const Vector3& v2) noexcept {
	return { v1.x - v2.x, v1.y - v2.y, v1.z - v2.z };
}
inline constexpr Vector3 operator*(const Vector3& v, float s) noexcept {
	return { v.x * s, v.y * s, v.z * s };
}
inline constexpr Vector3 operator*(float s, const Vector3& v) noexcept {
	return { v.x * s, v.y * s, v.z * s };
}
inline constexpr Vector3 operator/(const Vector3& v, float s) noexcept {
	return { v.x / s, v.y / s, v.z / s };
}

// --- Vector2 の演算子オーバーロード ---
inline constexpr Vector2& operator+=(Vector2& lhs, const Vector2& rhs) noexcept {
	lhs.x += rhs.x; lhs.y += rhs.y; return lhs;
}
inline constexpr Vector2 operator+(Vector2 lhs, const Vector2& rhs) noexcept {
	return lhs += rhs;
}