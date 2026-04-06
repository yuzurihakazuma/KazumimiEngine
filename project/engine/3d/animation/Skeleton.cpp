#include "Skeleton.h"
#include "engine/math/Matrix4x4.h"

using namespace MatrixMath;

// Node ツリーを再帰的に走査して Joint 配列を作るヘルパー
int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints) {

    Joint joint;
    joint.name = node.name;
    joint.transform = node.transform;
    joint.localMatrix = node.localMatrix;
    joint.skeletonSpaceMatrix = MakeIdentity4x4(); // 後で計算するので今は単位行列
    joint.inverseBindPoseMatrix = MakeIdentity4x4(); // 後で Assimp の aiBone から上書きする
    joint.parent = parent;
    joint.index = static_cast<int32_t>(joints.size()); // push前のサイズ = 自分のインデックス

    joints.push_back(joint); // まず自分を配列に追加

    // 子ノードを再帰的に処理（自分のインデックスを親として渡す）
    for (const Node& child : node.children) {
        int32_t childIndex = CreateJoint(child, joint.index, joints);
        joints[joint.index].children.push_back(childIndex);
    }

    return joint.index;
}

// ルートノードから Skeleton を作成する
Skeleton CreateSkeleton(const Node& rootNode) {
    Skeleton skeleton;

    // 再帰的に全ジョイントを joints 配列に詰める
    skeleton.root = CreateJoint(rootNode, std::nullopt, skeleton.joints);

    // 名前 → インデックスの辞書を作る
    for (const Joint& joint : skeleton.joints) {
        skeleton.jointMap[joint.name] = joint.index;
    }

    return skeleton;
}

void UpdateSkeleton(Skeleton& skeleton, const Animation& animation, float time) {

    // --- Step1：各ジョイントのローカル行列をアニメーションで更新 ---
    for (Joint& joint : skeleton.joints) {

        // アニメーションデータの中からこのジョイントの名前を探す
        auto it = animation.nodeAnimations.find(joint.name);

        if (it != animation.nodeAnimations.end()) {
            // アニメーションデータがあれば、現在時刻のSRTをキーフレームから計算して上書き
            const NodeAnimation& nodeAnimation = it->second;
            joint.transform.translate = CalculateValue(nodeAnimation.translate.keyframes, time);
            joint.transform.rotate = CalculateValue(nodeAnimation.rotate.keyframes, time);
            joint.transform.scale = CalculateValue(nodeAnimation.scale.keyframes, time);
        }
        // アニメーションがないジョイントはLoadModel時の初期値（transform）をそのまま使う

        // transform（SRT）から localMatrix を毎フレーム再計算する
        joint.localMatrix = MakeAffineMatrix(
            joint.transform.scale,
            joint.transform.rotate,
            joint.transform.translate
        );
    }

    // --- Step2：親 → 子の順にスケルトン空間行列を計算 ---
    for (Joint& joint : skeleton.joints) {

        if (joint.parent) {
            // 親がいる場合：自分のローカル行列 × 親のスケルトン空間行列
            joint.skeletonSpaceMatrix = Multiply(
                joint.localMatrix,
                skeleton.joints[*joint.parent].skeletonSpaceMatrix
            );
        }
        else {
            // ルートジョイントはローカル行列がそのままスケルトン空間行列になる
            joint.skeletonSpaceMatrix = joint.localMatrix;
        }
    }
}
