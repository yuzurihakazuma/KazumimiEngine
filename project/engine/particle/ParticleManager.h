#pragma once
// --- 標準・外部ライブラリ ---
#include <d3d12.h>
#include <vector>
#include <list>
#include <string>
#include <unordered_map>
#include <wrl.h>
#include <random>

// --- エンジン側のファイル ---
#include "engine/math/struct.h"

// 前方宣言
class DirectXCommon;
class SrvManager;
class Camera;

// パーティクルの設定構造体
struct ParticleSetting{
    Vector3 minVelocity = { -0.05f,  0.1f, -0.05f }; // 最小速度
    Vector3 maxVelocity = { 0.05f,  0.3f,  0.05f }; // 最大速度
    Vector4 startColor = { 1.0f,   1.0f,  1.0f,  1.0f }; // 初期色（白）
    float gravity = -0.005f; // 重力（マイナスなら落ちる、プラスなら昇る）
    float minLifeTime = 0.5f;    // 最短寿命
    float maxLifeTime = 1.5f;    // 最長寿命
    float startScale = 1.0f;    // 発生時の大きさ
    float endScale = 0.0f;    // 消滅時の大きさ（0なら徐々に消える、大きくすれば爆発）
    bool isBillboard = true;    // カメラの方を向くかどうか
};

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

	ParticleSetting setting; // パーティクルの設定

	std::vector<Particle> particles; // CPU側で管理するパーティクルのコンテナ

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

	// デバッグ用UIの描画
    void DrawDebugUI();


    // パーティクルの発生 (グループ名を指定して発生させる)
    void Emit(const std::string& name, const Vector3& position, uint32_t count);

    // 終了処理
    void Finalize();

	// セーブ・ロード
    void Save();
    void Load();

public: // ゲッター・セッター

    size_t GetParticleCount(const std::string& name) const;
    uint32_t GetInstanceCount(const std::string& name) const;

	// パーティクル設定のセッター
    void SetParticleSetting(const std::string& name, const ParticleSetting& setting);

private: // シングルトン用（コンストラクタ隠蔽）
    ParticleManager() = default;
    ~ParticleManager() = default;
    ParticleManager(const ParticleManager&) = delete;
    ParticleManager& operator=(const ParticleManager&) = delete;

    void CreateModel();


    // ランダムなパーティクルを生成する関数
	Particle MakeNewParticle(std::mt19937& engine, const Vector3& position, const ParticleSetting& setting); 

private: // メンバ変数
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;

    //  パーティクルグループのコンテナ
    // グループ名をキーにして複数のグループを管理する
    std::unordered_map<std::string, ParticleGroup> particleGroups_;

    // --- 共通リソース (頂点データなど) ---
    // モデルデータ（板ポリゴン）
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ {};

    // 最大インスタンス数（1グループあたり）
    const uint32_t kNumMaxInstance = 10000;
};