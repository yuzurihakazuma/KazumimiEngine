#pragma once
// 独自クラス
#include "engine/math/struct.h"
#include "engine/math/Matrix4x4.h"

// 標準ライブラリ
#include <cstdint>
#include <d3d12.h>   
#include <wrl.h>
#include <string>
#include <memory>
#include "engine/utils/Color.h"

namespace KazumimiEngine {

// 前方宣言
class SpriteCommon;

// =====================================================================
//  Sprite：2D の絵 1 枚（スプライト）。
//   テクスチャ・置き場所・大きさ・回転・色・切り出す範囲を持ち、画面のピクセルの座標で描く。
//   作るのは Create、毎フレーム Update → Draw（描く前の共通の設定は SpriteCommon::PreDraw が 1 回だけ行う）。
//   SetReveal で、なぞる順のマスクを使った「書かれていく」表示もできる（リザルトの花丸）
// =====================================================================
class Sprite {
public:


	static std::unique_ptr<Sprite> Create(const std::string& textureName, Vector2 position, const Vector4& color = Colors::WHITE, Vector2 anchorpoint = { 0.5f, 0.5f });

	/// <summary>
	/// スプライト生成・初期化短縮関数
	/// </summary>
	/// <param name="textureIndex">テクスチャ番号</param>
	/// <param name="position">座標</param>
	/// <param name="color">色（省略可：デフォルトは白）</param>
	/// <param name="anchorpoint">アンカーポイント（省略可：デフォルトは中心 0.5, 0.5）</param>
	/// <returns>生成されたスプライトのポインタ</returns>
	static std::unique_ptr<Sprite> Create(uint32_t textureIndex, Vector2 position, const Vector4& color = Colors::WHITE, Vector2 anchorpoint = { 0.5f, 0.5f });



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
	const Vector2 GetPosition() const { return position_; }
	void SetPosition(const Vector2& position) { position_ = position; }

	// スプライトの回転角度を設定・取得する関数を追加
	float GetRotation() const { return rotation_; }
	void SetRotation(float rotation) { rotation_ = rotation; }



	// スプライトの色を設定・取得する関数を追加
	const Vector4& GetColor() const { return materialData_->color; }
	void SetColor(const Vector4& color);

	// スプライトのアンカーポイントを設定・取得する関数を追加
	const Vector2 GetAnchorPoint() const { return anchorPoint_; }
	void SetAnchorPoint(const Vector2& anchorPoint);

	// スプライトの左右クリップ設定・取得する関数を追加
	bool GetIsFlipX() const { return isFlipX_; }
	void SetIsFlipX(bool isFlipX) {
		isFlipX_ = isFlipX;
		UpdateVertexData();
	}
	// スプライトの上下クリップ設定・取得する関数を追加
	bool GetIsFlipY() const { return isFlipY_; }
	void SetIsFlipY(bool isFlipY) {
		isFlipY_ = isFlipY;
		UpdateVertexData();
	}


	//（画像の大きさ）をセットする
	const Vector2& GetSize() const { return size_; }
	void SetSize(const Vector2& size) {
		size_ = size;
		UpdateVertexData();

	}


	void SetTexture(uint32_t textureIndex);

	void SetTextureHandle(D3D12_GPU_DESCRIPTOR_HANDLE textureHandle) {
		textureHandle_ = textureHandle;
	}

	// テクスチャの切り抜き範囲を設定する関数 (ピクセル単位で指定)
	void SetTextureRect(float startX, float startY, float width, float height);

	// 描き順のマスク付きの絵を、描いた順に少しずつ出す（花丸を一筆ずつ描くなど。ディゾルブと同じ考え方）。
	//   絵は「R＝濃さ・G＝形（ふちのなめらかさ）・B＝描く順（早く描く所ほど明るい）」で作る。色は SetColor の色。
	//   progress＝0 で何も出ず、1 で全部出る。ClearReveal でふつうの絵の描き方へ戻る
	void SetReveal(float progress);
	void ClearReveal();

	// 四すみの位置を直接指定して描く（台形など、長方形でない形。位置は SetPosition からのずれ）。
	//   指定している間は大きさ・アンカー・反転は使わない。ClearCorners で元の長方形に戻る
	void SetCorners(const Vector2& topLeft, const Vector2& topRight, const Vector2& bottomLeft, const Vector2& bottomRight);
	void ClearCorners();
private:


	void CreateVertexBuffer();// バッファ作成関数群
	void CreateIndexBuffer(); // indexバッファ作成
	void CreateMaterialBuffer(); // マテリアルバッファ作成
	void CreateTransformationMatrixBuffer(); // 変換行列バッファ作成
	void UpdateVertexData(); // 頂点データの計算・転送

private:

	// スプライトの頂点 1 つ（位置・UV・法線）。GPU へ送る形
	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	// スプライトのマテリアルの定数バッファ（色・UV の変換など）。GPU へ送る形
	struct Material {
		Vector4 color;
		int32_t enableLighting;
		float padding[3];
		Matrix4x4 uvTransform; // UV変換行列
		float shininess;
		float reveal;          // 描き順のマスクのしきい値（負＝使わない。Sprite2d.PS.hlsl）
	};

	// スプライトの座標変換の定数バッファ（WVP・ワールド・法線用の行列）。GPU へ送る形
	struct TransformationMatrix {
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
	TransformationMatrix* transformationMatrixData_ = nullptr;

	Transform transform_{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	
	// indexSprite用の頂点indexを作る1つ辺りのindexのサイズは32bit
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;

	D3D12_INDEX_BUFFER_VIEW indexBufferView_{}; // IBV

	// indexリソースにデータを書き込む
	uint32_t* indexData_ = nullptr;

	// このスプライトが使うテクスチャのハンドル
	D3D12_GPU_DESCRIPTOR_HANDLE textureHandle_{};


	Vector2 position_ = { 0.0f,0.0f }; // スプライトの位置

	float rotation_ = 0.0f; // スプライトの回転角度

	Vector2 size_ = { 640.0f,360.0f }; // スプライトの拡大率

	Vector2 anchorPoint_ = { 0.0f,0.0f }; // スプライトの拡大率

	uint32_t textureIndex_ = 0;


	// 左右クリップ
	bool isFlipX_ = false;
	// 上下クリップ
	bool isFlipY_ = false;

	Vector2 textureLeftTop_ = { 0.0f, 0.0f }; // 切り抜き開始位置（ピクセル）
	bool    useCorners_ = false;                // SetCorners で四すみを直接指定しているか
	Vector2 corners_[4] {};                     // 左上・右上・左下・右下
	Vector2 textureSize_ = { 100.0f, 100.0f }; // 切り抜きサイズ（ピクセル）

	bool isDirty_ = true; // 頂点データ更新フラグ

	static constexpr float kRevealOff = -1.0f; // Material::reveal：描き順のマスクを使わない

};

} // namespace KazumimiEngine
