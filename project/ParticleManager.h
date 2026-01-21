#pragma once
#include <d3d12.h>
#include <vector>
#include <list>
#include <string>
#include <unordered_map>
#include <wrl.h>
#include "struct.h"
#include "Matrix4x4.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "Camera.h"
#include <random>

// パーティクル1粒のデータ
struct Particle{
    Transform transform;
    Vector3 velocity;
    Vector4 color;
    float lifeTime;
    float currentTime;
};
// エミッターのデータ
struct Emitter{
    Transform transform;   // 位置・回転・スケール
    uint32_t count;        // 一度に発生させる数
    float frequency;       // 発生頻度（秒単位：0.5なら0.5秒ごとに発生）
    float frequencyTime;   // 頻度計測用タイマー
};

// GPUに送るデータ構造
struct ParticleForGPU{
    Matrix4x4 WVP;
    Matrix4x4 World;
    Vector4 color;
};

// パーティクルグループ構造体
struct ParticleGroup{
    // マテリアルデータ
    std::string textureFilePath; // テクスチャパス
    uint32_t textureSrvIndex;    // テクスチャのSRVインデックス

    // パーティクルのリスト
    std::list<Particle> particles;

    // インスタンシング用データ
    uint32_t kNumInstance = 0;   // 現在の個数（最大数は定数で定義するか、メンバに持たせる）
    Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource; // インスタンシングリソース
    uint32_t instancingSrvIndex; // インスタンシングデータのSRVインデックス
    ParticleForGPU* instancingData = nullptr; // 書き込み用ポインタ
};

// マネージャクラス
class ParticleManager{
public: // 静的メンバ関数
    // シングルトンインスタンス取得
    static ParticleManager* GetInstance(){
        static ParticleManager instance;
        return &instance;
    }

public: // メンバ関数
    // 初期化
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

    // 更新
    void Update(Camera* camera);

    // 描画
    void Draw(ID3D12GraphicsCommandList* commandList);

    //  パーティクルグループの生成
    // name: グループ名 ("Smoke"など), textureFilePath: テクスチャのパス
    void CreateParticleGroup(const std::string& name, const std::string& textureFilePath);

    // パーティクルの発生 (グループ名を指定して発生させる)
    void Emit(const std::string& name, const Vector3& position, uint32_t count);

    // 終了処理
    void Finalize();

private: // シングルトン用（コンストラクタ隠蔽）
    ParticleManager() = default;
    ~ParticleManager() = default;
    ParticleManager(const ParticleManager&) = delete;
    ParticleManager& operator=(const ParticleManager&) = delete;

    void CreateModel();

    Particle MakeNewParticle(std::mt19937& engine, const Vector3& position);


private: // メンバ変数
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    //  パーティクルグループのコンテナ
    // グループ名をキーにして複数のグループを管理する
    std::unordered_map<std::string, ParticleGroup> particleGroups_;

    // --- 共通リソース (頂点データなど) ---
    // モデルデータ（板ポリゴン）
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ {};

    // 最大インスタンス数（1グループあたり）
    const uint32_t kNumMaxInstance = 1000;
};