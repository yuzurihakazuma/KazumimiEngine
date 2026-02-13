#pragma once
// 独自クラス
#include "struct.h"
#include "Matrix4x4.h"

// 標準ライブラリ
#include <cstdint>
#include <d3d12.h>   
#include <wrl.h>
#include <string>

#include"Color.h"

// 前方宣言
class SpriteCommon;

class Sprite{
public:


	static Sprite* Create(const std::string& textureName, Vector2 position, Vector4 color = Colors::WHITE, Vector2 anchorpoint = { 0.5f, 0.5f });

	/// <summary>
	/// スプライト生成・初期化短縮関数
	/// </summary>
	/// <param name="textureIndex">テクスチャ番号</param>
	/// <param name="position">座標</param>
	/// <param name="color">色（省略可：デフォルトは白）</param>
	/// <param name="anchorpoint">アンカーポイント（省略可：デフォルトは中心 0.5, 0.5）</param>
	/// <returns>生成されたスプライトのポインタ</returns>
	static Sprite* Create(uint32_t textureIndex, Vector2 position, Vector4 color = Colors::WHITE, Vector2 anchorpoint = { 0.5f, 0.5f });



	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();
	/// <summary>
	/// 更新
	/// </summary>
	void Update();
	/// </summary>
	/// 描画
	/// </summary>
	void Draw();

	D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() { return vertexBufferView_; }

	
	// スプライトの位置を設定・取得する関数を追加
	const Vector2 GetPosition() const{ return position_; }
	void SetPosition(const Vector2& position){ position_ = position; }

	// スプライトの回転角度を設定・取得する関数を追加
	float GetRotation() const{ return rotation_; }
	void SetRotation(float rotation){ rotation_ = rotation; }



	// スプライトの色を設定・取得する関数を追加
	const Vector4& GetColor() const{ return materialData_->color; }
	void SetColor(const Vector4& color);

	// スプライトのアンカーポイントを設定・取得する関数を追加
	const Vector2 GetAnchorPoint() const{ return anchorPoint_; }
	void SetAnchorPoint(const Vector2& anchorPoint);

	// スプライトの左右クリップ設定・取得する関数を追加
	bool GetIsFlipX() const{ return isFlipX_; }
	void SetIsFlipX(bool isFlipX){ isFlipX_ = isFlipX; }
	// スプライトの上下クリップ設定・取得する関数を追加
	bool GetIsFlipY() const{ return isFlipY_; }
	void SetIsFlipY(bool isFlipY){ isFlipY_ = isFlipY; }


	//（画像の大きさ）をセットする
	const Vector2& GetSize() const { return size_; }
	void SetSize(const Vector2& size) { size_ = size; }


	void SetTexture(uint32_t textureIndex);

	void SetTextureHandle(D3D12_GPU_DESCRIPTOR_HANDLE textureHandle) {
		textureHandle_ = textureHandle;
	}


private:
	
	
	void CreateVertexBuffer();// バッファ作成関数群
	void CreateIndexBuffer(); // indexバッファ作成
	void CreateMaterialBuffer(); // マテリアルバッファ作成
	void CreateTransformationMatrixBuffer(); // 変換行列バッファ作成
	void UpdateVertexData(); // 頂点データの計算・転送

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
		float shininess;
	};

	struct TransformationMatrix{
		Matrix4x4 WVP;
		Matrix4x4 World;
		Matrix4x4 WorldInverseTranspose;
	};

	SpriteCommon* spriteCommon_ = nullptr; // 所有しない参照

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

	float width_ = 1280.0f; // 仮置き
	float height_ = 720.0f; //

	// indexSprite用の頂点indexを作る1つ辺りのindexのサイズは32bit
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;

	D3D12_INDEX_BUFFER_VIEW indexBufferView_ {}; // IBV

	// indexリソースにデータを書き込む
	uint32_t* indexData_ = nullptr;

	// このスプライトが使うテクスチャのハンドル
	D3D12_GPU_DESCRIPTOR_HANDLE textureHandle_ {};
	
	
	Vector2 position_ = { 0.0f,0.0f }; // スプライトの位置

	float rotation_ = 0.0f; // スプライトの回転角度

	Vector2 size_ = { 640.0f,360.0f }; // スプライトの拡大率

	Vector2 anchorPoint_ = { 0.0f,0.0f }; // スプライトの拡大率

	// 左右クリップ
	bool isFlipX_ = false;
	// 上下クリップ
	bool isFlipY_ = false;
	// テクスチャ左上座標
	Vector2 texuvLeftTop_ = { 0.0f,0.0f };
	// テクスチャ右下座標
	Vector2 texuvRightBottom_ = { 100.0f,100.0f };

	bool isDirty_ = true; // 頂点データ更新フラグ

};

