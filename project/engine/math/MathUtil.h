#pragma once
// =====================================================================
//  MathUtil：数の定数と、型を選ばない小さな計算（関数テンプレート）。
//   ・円周率・度とラジアンの変換は、ここの 1 か所だけに書く
//     （前は 3.14159265f が 44 個のファイルに 64 回、桁数もまちまちに書かれていた）
//   ・Lerp は関数テンプレート：a + (b - a) * t が書ける型なら、同じ名前で使える
//       float   a = Math::Lerp(0.0f, 10.0f, t);
//       Vector3 p = Math::Lerp(from, to, t);          // Vector2・Vector4 も同じ
//     （前は Easing::Lerp（float 用）と VectorMath::Lerp（Vector3 用）の 2 つに、同じ式を書いていた）
//   ・ToRadians / ToDegrees も関数テンプレート：float でも、度で書いた回転（Vector3）でも
//       float   yaw = Math::ToRadians(90.0f);
//       Vector3 rot = Math::ToRadians(item.rot);      // x・y・z をまとめて
//   ・角度は WrapAngle（-π〜π に収める）・AngleDelta（近い回り方の差）・WrapDegrees（度で -180〜180）
//  ベクトル専用の計算（内積・正規化・距離など）は VectorMath、行列は MatrixMath、動きの緩急は Easing
// =====================================================================
#include <cmath>
#include <type_traits>

namespace KazumimiEngine {

namespace Math {

    // --- 定数 ---
    inline constexpr float kPi = 3.14159265358979323846f;
    inline constexpr float kTwoPi = kPi * 2.0f;
    inline constexpr float kHalfPi = kPi * 0.5f;
    inline constexpr float kDegToRad = kPi / 180.0f; // 度 → ラジアン（掛ける）
    inline constexpr float kRadToDeg = 180.0f / kPi; // ラジアン → 度（掛ける）

    // --- 度とラジアン（関数テンプレート）---
    // 整数を渡すと小数が切れるので、float か Vector で渡す
    template <class T>
    constexpr T ToRadians(const T& degrees){
        static_assert( !std::is_integral_v<T>, "ToRadians: pass float or Vector (an integer would lose the fraction)" );
        return degrees * kPi / 180.0f;
    }
    template <class T>
    constexpr T ToDegrees(const T& radians){
        static_assert( !std::is_integral_v<T>, "ToDegrees: pass float or Vector (an integer would lose the fraction)" );
        return radians * 180.0f / kPi;
    }

    // --- 補間（関数テンプレート）---
    // 線形補間：t=0 で a、t=1 で b
    template <class T>
    constexpr T Lerp(const T& a, const T& b, float t){
        return a + ( b - a ) * t;
    }

    // --- 角度 ---
    // -π〜π に収める（ちょうど ±π はそのまま）
    inline float WrapAngle(float radians){
        while ( radians > kPi ) { radians -= kTwoPi; }
        while ( radians < -kPi ) { radians += kTwoPi; }
        return radians;
    }
    // 度の角度を -180〜180 に収める
    inline float WrapDegrees(float degrees){
        while ( degrees > 180.0f ) { degrees -= 360.0f; }
        while ( degrees < -180.0f ) { degrees += 360.0f; }
        return degrees;
    }
    // from から to への角度の差（近い回り方。-π〜π）
    inline float AngleDelta(float from, float to){
        return WrapAngle(to - from);
    }

} // namespace Math

} // namespace KazumimiEngine
