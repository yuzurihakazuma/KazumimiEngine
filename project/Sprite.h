#pragma once
#include "struct.h"
#include "Matrix4x4.h"
#include"WindowProc.h"       
#include <d3d12.h>               
#include <cstdint>  
#include <wrl.h>                    
// 前方宣言
class SpriteCommon;

class Sprite{
public:
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(SpriteCommon* spriteCommon);
	/// <summary>
	/// 更新
	/// </summary>
	void Update();
	/// </summary>
	/// 描画
	/// </summary>
	void Draw();

	D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() { return vertexBufferView_; }

private:
	
	struct VertexData{
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
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

	SpriteCommon* spriteCommon = nullptr; // 所有しない参照

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;
	VertexData* vertexData_ = nullptr; // Mapされたアドレス

	// Sprite用のマテリアルリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;

	Material* materialData_ = nullptr;

	// Sprite用のTransformationMatirx用のリソースを作る。Matrix4x4 一つ分のサイズを用意する
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
	// データを書き込む
	TransformationMatrix* transformationMatirxData_ = nullptr;

	Transform transform { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	WindowProc windowProc;


	// indexSprite用の頂点indexを作る1つ辺りのindexのサイズは32bit
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;

	D3D12_INDEX_BUFFER_VIEW indexBufferView_ {}; // IBV

	// indexリソースにデータを書き込む
	uint32_t* indexData_ = nullptr;
};

