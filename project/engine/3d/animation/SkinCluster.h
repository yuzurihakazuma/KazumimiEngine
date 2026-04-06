#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <map>
#include <string>
#include "engine/math/Matrix4x4.h"
#include "engine/3d/animation/Skeleton.h"

// 前方宣言
class DirectXCommon;

struct SkinCluster {
    // CPU側：ジョイントごとの最終行列（毎フレーム計算する）
    std::vector<Matrix4x4> matrixPalette;

    // GPU側：StructuredBuffer
    Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource;
    Matrix4x4* mappedPalette = nullptr; // Map しっぱなしで書き込むためのポインタ

    // シェーダーに渡す SRV 情報
    uint32_t                    srvIndex = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = {};


};

// 関数宣言
SkinCluster CreateSkinCluster(
    Skeleton& skeleton,
    const std::map<std::string, Matrix4x4>& inverseBindPoseMap,
    const std::vector<std::string>& boneOrder,
    DirectXCommon* dxCommon
);


void UpdateSkinCluster(
    SkinCluster& cluster,
    const Skeleton& skeleton,
    const std::vector<std::string>& boneOrder   // 追加
);