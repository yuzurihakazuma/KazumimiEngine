#pragma once
#include "struct.h" 
// 色定義
namespace Colors {
    // 赤・緑・青・アルファ(透明度)
    static const Vector4 WHITE = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白
    static const Vector4 BLACK = { 0.0f, 0.0f, 0.0f, 1.0f }; // 黒
    static const Vector4 RED = { 1.0f, 0.0f, 0.0f, 1.0f }; // 赤
    static const Vector4 GREEN = { 0.0f, 1.0f, 0.0f, 1.0f }; // 緑
    static const Vector4 BLUE = { 0.0f, 0.0f, 1.0f, 1.0f }; // 青
    static const Vector4 YELLOW = { 1.0f, 1.0f, 0.0f, 1.0f }; // 黄
    static const Vector4 CYAN = { 0.0f, 1.0f, 1.0f, 1.0f }; // 水色
    static const Vector4 MAGENTA = { 1.0f, 0.0f, 1.0f, 1.0f }; // 紫
    static const Vector4 GRAW = { 0.5f, 0.5f, 0.5f, 1.0f }; // 灰色
}