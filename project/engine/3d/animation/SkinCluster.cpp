#include "SkinCluster.h"
#include "engine/base/DirectXCommon.h"
#include "engine/graphics/ResourceFactory.h"
#include "engine/graphics/SrvManager.h"
#include "engine/math/Matrix4x4.h"

using namespace MatrixMath;

SkinCluster CreateSkinCluster(
    Skeleton& skeleton,
    const std::map<std::string, Matrix4x4>& inverseBindPoseMap,
    DirectXCommon* dxCommon
) {
    SkinCluster skinCluster;
    uint32_t jointCount = static_cast<uint32_t>(skeleton.joints.size());

    // 1. inverseBindPoseMatrix を各ジョイントに設定する
    for (Joint& joint : skeleton.joints) {
        auto it = inverseBindPoseMap.find(joint.name);
        if (it != inverseBindPoseMap.end()) {
            joint.inverseBindPoseMatrix = it->second;
        }
        // 見つからないジョイントは CreateJoint で設定した単位行列のまま
    }

    // 2. CPU側のパレット配列をジョイント数分確保
    skinCluster.matrixPalette.resize(jointCount);

    // 3. GPU側のバッファを作成（StructuredBuffer）
    skinCluster.paletteResource = ResourceFactory::GetInstance()->CreateBufferResource(
        sizeof(Matrix4x4) * jointCount
    );

    // 4. Map しっぱなしにして毎フレーム書き込めるようにする
    skinCluster.paletteResource->Map(
        0, nullptr,
        reinterpret_cast<void**>(&skinCluster.mappedPalette)
    );

    // 5. SRV を作成してシェーダーから読めるようにする
    skinCluster.srvIndex = SrvManager::GetInstance()->Allocate();

    SrvManager::GetInstance()->CreateSRVforStructuredBuffer(
        skinCluster.srvIndex,
        skinCluster.paletteResource.Get(),
        jointCount,
        sizeof(Matrix4x4)
    );

    skinCluster.srvHandle = SrvManager::GetInstance()->GetGPUDescriptorHandle(
        skinCluster.srvIndex
    );

    return skinCluster;
}

void UpdateSkinCluster(SkinCluster& skinCluster, const Skeleton& skeleton) {
    for (size_t i = 0; i < skeleton.joints.size(); ++i) {
        const Joint& joint = skeleton.joints[i];

        // 最終行列 = inverseBindPoseMatrix × skeletonSpaceMatrix
        // 「バインドポーズからの差分だけ」をGPUに渡す
        skinCluster.matrixPalette[i] = Multiply(
            joint.inverseBindPoseMatrix,
            joint.skeletonSpaceMatrix
        );
    }

    // CPU側の配列をそのまま GPU バッファに書き込む
    std::copy(
        skinCluster.matrixPalette.begin(),
        skinCluster.matrixPalette.end(),
        skinCluster.mappedPalette
    );
}
