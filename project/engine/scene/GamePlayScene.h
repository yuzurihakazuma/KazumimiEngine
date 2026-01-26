#pragma once
#include "DirectXCommon.h"
#include "Input.h"
#include "SpriteCommon.h"
#include "Obj3dCommon.h"
#include "WindowProc.h" 

// ゲーム内で使うクラス
#include "Camera.h"
#include "Sprite.h"
#include "Obj3d.h"
#include "ParticleManager.h" 



	// ゲームプレイシーン
class GamePlayScene{
public:
	// 初期化
	void Initialize();
	// 終了
	void Finalize();
	// 更新
	void Update();
	// 描画
	void Draw();

private: // メンバ変数

	// カメラ
	Camera* camera_ = nullptr;

	// 3Dオブジェクト
	std::vector<Obj3d*> object3ds_;
	Obj3d* fence_ = nullptr;
	Obj3d* sphere_ = nullptr;

	// スプライト
	std::vector<Sprite*> sprites_;
	Sprite* sprite_ = nullptr;

	// テクスチャデータ
	TextureData textureResource_;
	TextureData textureResource2_;
	TextureData textureResource3_;
	TextureData textureResource5_;

	// デプスステンシル
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;

	// ゲーム用パラメータ
	Vector3 groundPos_ = { 0.0f, 0.0f, 0.0f };
	Vector3 groundScale_ = { 1.0f, 1.0f, 1.0f };
	Vector3 spherePos_ = { 0.0f, 0.0f, 0.0f };
	Vector3 sphereScale_ = { 1.0f, 1.0f, 1.0f };
	std::string bgmFile_ = "resources/BGM.wav";
};