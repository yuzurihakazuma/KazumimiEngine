#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <memory>
#include <string>

// 
#include "engine/math/struct.h"
#include "engine/math/Matrix4x4.h"

#include "engine/3d/animation/Skeleton.h"
#include "engine/3d/animation/SkinCluster.h"
#include "engine/3d/animation/Animation.h"

// 前方宣言
class Obj3dCommon;
class Model;
class Camera;

class SkinnedObj3d {
public:
    //  WVP 構造体
    struct TransformationMatrix {
        Matrix4x4 WVP;
        Matrix4x4 World;
        Matrix4x4 WorldInverseTranspose;
    };

    // ディゾルブ用（Obj3d と合わせる）
    struct DissolveData {
        float threshold;
        float padding[3];
    };

    /// <summary>
    /// 静的生成関数
    /// modelName    : ModelManager に登録済みのモデル名
    /// directoryPath: アニメーションファイルのフォルダ
    /// animFilename : アニメーションファイル名
    /// </summary>
    static std::unique_ptr<SkinnedObj3d> Create(
        const std::string& modelName,
        const std::string& directoryPath,
        const std::string& animFilename
    );

    void Initialize(
        Model* model,
        const std::string& directoryPath,
        const std::string& animFilename
    );

    void Update();
    void Draw();

    // --- Getter / Setter ---
    void SetCamera(const Camera* camera) { camera_ = camera; }
    void SetTranslation(const Vector3& t) { translate_ = t; }
    void SetRotation(const Vector3& r) { rotation_ = r; }
    void SetScale(const Vector3& s) { scale_ = s; }
    void SetLoopAnimation(bool loop) { isLoop_ = loop; }
    void SetNoiseTexture(uint32_t index) { noiseTextureIndex_ = index; }
    void SetDissolveThreshold(float threshold);

    const Vector3& GetTranslation() const { return translate_; }
    const Vector3& GetRotation()    const { return rotation_; }
    const Vector3& GetScale()       const { return scale_; }

private:
    // トランスフォーム
    Vector3 scale_ = { 1.0f, 1.0f, 1.0f };
    Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };
    Vector3 translate_ = { 0.0f, 0.0f, 0.0f };

    // 外部参照（所有しない）
    Model* model_ = nullptr;
    const Camera* camera_ = nullptr;
    Obj3dCommon* obj3dCommon_ = nullptr;

    // WVP 定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource_;
    TransformationMatrix* transformationMatrixData_ = nullptr;

    // ディゾルブ定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> dissolveResource_;
    DissolveData* dissolveData_ = nullptr;
    uint32_t      noiseTextureIndex_ = 0;

    // スケルトン・スキニング
    Skeleton    skeleton_;
    SkinCluster skinCluster_;

    // アニメーション
    Animation animation_;
    float     animationTime_ = 0.0f;
    bool      isLoop_ = true;
};
