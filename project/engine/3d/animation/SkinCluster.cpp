#include "SkinCluster.h"
#include "engine/base/DirectXCommon.h"
#include "engine/graphics/ResourceFactory.h"
#include "engine/graphics/SrvManager.h"
#include "engine/math/Matrix4x4.h"

using namespace MatrixMath;

SkinCluster CreateSkinCluster(
    Skeleton& skeleton,
    const std::map<std::string, Matrix4x4>& inverseBindPoseMap,
    const std::vector<std::string>& boneOrder,
    DirectXCommon* dxCommon
) {
    SkinCluster cluster;
    int boneCount = static_cast<int>(boneOrder.size());

    // inverseBindPose を skeleton の各 Joint にセット（既存処理）
    for (auto& joint : skeleton.joints) {
        auto it = inverseBindPoseMap.find(joint.name);
        if (it != inverseBindPoseMap.end()) {
            joint.inverseBindPoseMatrix = it->second;
        }
    }

    // GPU バッファを boneCount のサイズで作成
    cluster.paletteResource = dxCommon->GetResourceFactory()->CreateBufferResource(sizeof(Matrix4x4) * boneCount);
    cluster.paletteResource->Map(0, nullptr, reinterpret_cast<void**>(&cluster.mappedPalette));

    cluster.matrixPalette.resize(boneCount, MakeIdentity4x4());

    // SRV 作成
    cluster.srvIndex = SrvManager::GetInstance()->Allocate();
    SrvManager::GetInstance()->CreateSRVforStructuredBuffer(
        cluster.srvIndex, cluster.paletteResource.Get(), boneCount, sizeof(Matrix4x4)
    );
    cluster.srvHandle = SrvManager::GetInstance()->GetGPUDescriptorHandle(cluster.srvIndex);

    return cluster;
}
void UpdateSkinCluster(SkinCluster& cluster, const Skeleton& skeleton, const std::vector<std::string>& boneOrder) {
    for (int i = 0; i < static_cast<int>(boneOrder.size()); ++i) {
        auto it = skeleton.jointMap.find(boneOrder[i]);
        if (it != skeleton.jointMap.end()) {
            const Joint& joint = skeleton.joints[it->second];
            cluster.matrixPalette[i] = Multiply(joint.inverseBindPoseMatrix, joint.skeletonSpaceMatrix);
        }
        else {
            cluster.matrixPalette[i] = MakeIdentity4x4();
        }
    }
    std::copy(cluster.matrixPalette.begin(), cluster.matrixPalette.end(), cluster.mappedPalette);
}