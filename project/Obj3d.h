#pragma once
#include "struct.h"
#include "Matrix4x4.h"
#include"WindowProc.h"       
#include <d3d12.h>               
#include <cstdint>  
#include <wrl.h>           
#include "Obj3dCommon.h"
// モデル3Dクラス
class Obj3d{

public:

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(Obj3dCommon* obj3dCommon);
	/// <summary>
	/// 更新
	/// </summary>
	void Update();
	/// <summary>
	/// 描画
	/// </summary>
	void Draw();
	// Getter
	Microsoft::WRL::ComPtr<ID3D12Resource> GetVertexBuffer() const{ return vertexResource_; }
	D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const{ return vertexBufferView_; }
	Microsoft::WRL::ComPtr<ID3D12Resource> GetIndexBuffer() const{ return indexResource_; }
	D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const{ return indexBufferView_; }
	uint32_t GetIndexCount() const{ return indexCount_; }

private:

	struct VertexData{
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};
	struct MaterialData{
		std::string textrueFilePath; // テクスチャファイルのパス
	};


	struct ModelData{
		std::vector<VertexData> vertices; // 頂点データ
		std::vector<uint32_t> indices;
		MaterialData material;
	};

	struct Material{
		Vector4 color;
		int32_t enableLighting;
		float padding[3];
		Matrix4x4 uvTransfrom; // UV変換行列
	};

	struct TransformationMatrix{
		Matrix4x4 WVP;
		Matrix4x4 World;
	};

	Obj3dCommon* obj3dCommon = nullptr; // 所有しない参照
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;
	VertexData* vertexData_ = nullptr; // Mapされたアドレス
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
	D3D12_INDEX_BUFFER_VIEW indexBufferView_;
	uint32_t* indexData_ = nullptr; // Mapされたアドレス
	uint32_t indexCount_ = 0;
	// モデル用のマテリアルリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* materialData_ = nullptr;
	Transform transform { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };


	MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

};

