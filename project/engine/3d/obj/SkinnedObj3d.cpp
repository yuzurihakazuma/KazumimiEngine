#include "SkinnedObj3d.h"

#include <cassert>
#include <cmath>

#include "Obj3dCommon.h"
#include "engine/3d/model/Model.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/camera/Camera.h"
#include "engine/math/Matrix4x4.h"
#include "engine/math/QuaternionMath.h"
#include "engine/base/DirectXCommon.h"
#include "engine/graphics/ResourceFactory.h"
#include "engine/graphics/SrvManager.h"
#include "engine/graphics/PipelineManager.h"
#include "engine/graphics/PipelineType.h"

using namespace MatrixMath;

// --------------------------------------------------
// 静的生成関数
// --------------------------------------------------
std::unique_ptr<SkinnedObj3d> SkinnedObj3d::Create(
    const std::string& modelName,
    const std::string& directoryPath,
    const std::string& animFilename
) {
    // ModelManager から登録済みモデルを探す
    Model* model = ModelManager::GetInstance()->FindModel(modelName);
    if (!model) { return nullptr; }

    auto obj = std::make_unique<SkinnedObj3d>();
    obj->Initialize(model, directoryPath, animFilename);
    return obj;
}

// --------------------------------------------------
// 初期化
// --------------------------------------------------
void SkinnedObj3d::Initialize(
    Model* model,
    const std::string& directoryPath,
    const std::string& animFilename
) {
    model_ = model;
    obj3dCommon_ = Obj3dCommon::GetInstance();

    auto dxCommon = obj3dCommon_->GetDxCommon();

    // WVP 定数バッファの作成（Obj3d と同じ）
    wvpResource_ = dxCommon->GetResourceFactory()->CreateBufferResource(sizeof(TransformationMatrix));
    assert(wvpResource_ != nullptr);
    wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));
    transformationMatrixData_->WVP = MakeIdentity4x4();
    transformationMatrixData_->World = MakeIdentity4x4();

    // ディゾルブ定数バッファの作成（Obj3d と同じ）
    dissolveResource_ = dxCommon->GetResourceFactory()->CreateBufferResource(sizeof(DissolveData));
    dissolveResource_->Map(0, nullptr, reinterpret_cast<void**>(&dissolveData_));
    dissolveData_->threshold = 0.0f;

    // スケルトンの作成
    skeleton_ = CreateSkeleton(model_->GetRootNode());

    // スキンクラスターの作成（inverseBindPose も設定される）
    skinCluster_ = CreateSkinCluster(
        skeleton_,
        model_->GetInverseBindPoseMap(),
        dxCommon
    );

    // アニメーションの読み込み
    animation_ = LoadAnimationFromFile(directoryPath, animFilename);
}

// --------------------------------------------------
// 更新（毎フレーム）
// --------------------------------------------------
void SkinnedObj3d::Update() {

    // --- アニメーション時間を進める ---
    animationTime_ += 1.0f / 60.0f;
    if (isLoop_) {
        // ループ再生
        animationTime_ = std::fmod(animationTime_, animation_.duration);
    }
    else {
        // 終端でとめる
        if (animationTime_ > animation_.duration) {
            animationTime_ = animation_.duration;
        }
    }

    // --- スケルトンにアニメーションを適用して行列を更新 ---
    UpdateSkeleton(skeleton_, animation_, animationTime_);

    // --- MatrixPalette を計算して GPU バッファに書き込む ---
    UpdateSkinCluster(skinCluster_, skeleton_);

    // --- WVP 行列の計算（Obj3d と同じ処理） ---
    Matrix4x4 worldMatrix = MakeAffine(scale_, rotation_, translate_);

    Matrix4x4 worldViewProjectionMatrix;
    if (camera_) {
        const Matrix4x4& viewProjectionMatrix = camera_->GetViewProjectionMatrix();
        worldViewProjectionMatrix = Multiply(worldMatrix, viewProjectionMatrix);
    }
    else {
        worldViewProjectionMatrix = worldMatrix;
    }

    Matrix4x4 worldInverseTranspose = Transpose(Inverse(worldMatrix));

    transformationMatrixData_->World = worldMatrix;
    transformationMatrixData_->WVP = worldViewProjectionMatrix;
    transformationMatrixData_->WorldInverseTranspose = worldInverseTranspose;
}

// --------------------------------------------------
// 描画
// --------------------------------------------------
void SkinnedObj3d::Draw() {
    ID3D12GraphicsCommandList* commandList = obj3dCommon_->GetDxCommon()->GetCommandList();
    assert(commandList != nullptr);

    // スキニング専用パイプラインをセット
    PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::SkinningObject3D);

    // [1] WVP 定数バッファ
    commandList->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());

    // [3] 平行光源
    commandList->SetGraphicsRootConstantBufferView(3, obj3dCommon_->GetLightResource()->GetGPUVirtualAddress());

    // [4] カメラ
    if (camera_) {
        commandList->SetGraphicsRootConstantBufferView(4, camera_->GetCameraResource()->GetGPUVirtualAddress());
    }

    // [5] 点光源
    commandList->SetGraphicsRootConstantBufferView(5, Obj3dCommon::GetInstance()->GetPointLightResource()->GetGPUVirtualAddress());

    // [6] スポットライト
    commandList->SetGraphicsRootConstantBufferView(6, Obj3dCommon::GetInstance()->GetSpotLightResource()->GetGPUVirtualAddress());

    // [7] ノイズテクスチャ
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(7, noiseTextureIndex_);

    // [8] ディゾルブ
    commandList->SetGraphicsRootConstantBufferView(8, dissolveResource_->GetGPUVirtualAddress());

    // [9] MatrixPalette（スキニング用・ここが Obj3d との違い）
    commandList->SetGraphicsRootDescriptorTable(9, skinCluster_.srvHandle);

    // モデル描画（[0] マテリアル・[2] テクスチャ・DrawCall は内部でやってくれる）
    if (model_) {
        model_->Draw();
    }
}

// --------------------------------------------------
// Setter
// --------------------------------------------------
void SkinnedObj3d::SetDissolveThreshold(float threshold) {
    if (dissolveData_) {
        dissolveData_->threshold = threshold;
    }
}
