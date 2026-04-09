#pragma once

// --- エンジン側のファイル ---
#include "engine/math/struct.h"
#include "engine/math/Matrix4x4.h"
#include "engine/math/QuaternionMath.h"
#include "engine/3d/animation/Animation.h"


// --- 標準ライブラリ ---
#include <vector>
#include <map>
#include <string>
#include <optional>

struct Node {
    QuaternionTransform transform;
    Matrix4x4 localMatrix;        // このノード自身のローカル変換行列
    std::string name;             // ノード名（アニメーションと紐付けるために使う）
    std::vector<Node> children;   // 子ノードのリスト（再帰構造）
};


struct Joint {
    QuaternionTransform transform;
    Matrix4x4 localMatrix;            // ローカル変換行列（アニメーションで毎フレーム更新）
    Matrix4x4 skeletonSpaceMatrix;    // スケルトン空間での最終行列（親と掛け合わせた結果）
    Matrix4x4 inverseBindPoseMatrix;  // バインドポーズの逆行列（Assimpのaiboneから取得）
    std::string name;                 // ジョイント名
    std::vector<int32_t> children;
    int32_t index;                    // この配列の中での自分のインデックス
    std::optional<int32_t> parent;    // 親ジョイントのインデックス（ルートはnullopt）
};

struct Skeleton {
    int32_t root;                            // ルートジョイントの index
    std::map<std::string, int32_t> jointMap; // ジョイント名 → インデックスの辞書
    std::vector<Joint> joints;               // 全ジョイントの配列
};


// Node構造体のツリーを再帰的にたどって、Joint構造体の配列を作成する関数
int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints);
// Node構造体のツリーを再帰的にたどって、Joint構造体の配列を作成する関数
Skeleton CreateSkeleton(const Node& rootNode);

// Skeletonのジョイント配列をアニメーションのノードアニメーションと照らし合わせて、各ジョイントのローカル変換行列を更新する関数
void UpdateSkeleton(Skeleton& skeleton, const Animation& animation, float time);

// Skeletonの現在のtransformからスケルトン空間行列を更新する関数
void UpdateSkeleton(Skeleton& skeleton);