#pragma once
#include "Framework.h" 

#include "Camera.h"
#include "Sprite.h"
#include "Obj3d.h"

// テクスチャデータ構造体
class Game : public Framework{
public:
	// オーバーライド 
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private:
	// カメラ
	Camera* camera_ = nullptr;
	// 3Dオブジェクト
	std::vector<Obj3d*> object3ds_;
	// スプライト
	std::vector<Sprite*> sprites_;
	// 単一スプライト
	Sprite* sprite_ = nullptr;


	Obj3d* fence_ = nullptr;
	Obj3d* sphere_ = nullptr;

	// テクスチャデータなど
	TextureData textureResource_;
	TextureData textureResource2_;
	TextureData textureResource3_;
	TextureData textureResource5_;
	// デプスステンシルバッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;

	// ゲーム用変数
	Vector3 groundPos_ = { 0.0f, 0.0f, 0.0f };
	Vector3 groundScale_ = { 1.0f, 1.0f, 1.0f };

	Vector3 spherePos_ = { 0.0f, 0.0f, 0.0f };
	Vector3 sphereScale_ = { 1.0f, 1.0f, 1.0f };

	std::string bgmFile_ = "resources/BGM.wav";
};