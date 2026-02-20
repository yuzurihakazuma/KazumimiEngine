#pragma once
#include <string>
#include <vector>
#include <d3d12.h>
#include <wrl.h>

// User Headers
#include "struct.h"
#include "Matrix4x4.h"

// 前方宣言
class ModelCommon;

class Model{

public: // サブクラス定義

	struct VertexData{
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	struct MaterialData{
		std::string textureFilePath;
		uint32_t textureIndex = 0;
	};

	struct ModelData{
		std::vector<VertexData> vertices; // 頂点データ
		std::vector<uint32_t> indices;    // インデックスデータ
		MaterialData material;            // マテリアルデータ
	};
	// 定数バッファ用データ構造体
	struct Material{
		Vector4 color;// 材質の色
		int32_t enableLighting;// ライティングの有効無効
		float padding[3];       // 4 bytes * 3 = 12 bytes (行列を16バイト境界に合わせるための詰め物)
		Matrix4x4 uvTransform;
		float shininess;       
	};

public: // メンバ関数

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename);
	// <summary>
	/// 球モデルの初期化
	/// </summary>
	void InitializeSphere(ModelCommon* modelCommon, int subdivision);


	/// <summary>
	/// 描画
	/// </summary>
	void Draw();

private: // 内部関数

	// .mtlファイルの読み込み
	static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

	// .objファイルの読み込み
	static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

	// モデルの頂点座標を中心（原点）に合わせる
	void AdjustModelCenter();
	// / バッファの作成
	void CreateBuffers();

private: // メンバ変数

	ModelCommon* modelCommon_ = nullptr;

	// モデルデータ (CPU側)
	ModelData modelData_;

	// バッファリソース (GPU側)
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;

	// バッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ {};
	D3D12_INDEX_BUFFER_VIEW indexBufferView_ {};

	// データを書き込むためのポインタ (Map用)
	Material* materialData_ = nullptr;

	// テクスチャハンドル
	D3D12_GPU_DESCRIPTOR_HANDLE textureHandle_ {};

};