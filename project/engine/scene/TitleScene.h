#pragma once
#include "IScene.h"
#include <vector>
#include <memory>// ゲーム内で使うクラス
#include "Matrix4x4.h"
#include "TextureManager.h"
// 前方宣言
class Camera;
class Sprite;
class Obj3d;
class DirectXCommon;
class Input;



	// ゲームプレイシーン
class TitleScene : public IScene{
public:
	// 初期化
	void Initialize() override;
	// 終了
	void Finalize() override;
	// 更新
	void Update() override;
	// 描画
	void Draw() override;

	TitleScene();

	~TitleScene();

private: // メンバ変数

	// カメラ
	std::unique_ptr<Camera> camera_ = nullptr;

	// 3Dオブジェクト
	std::vector<std::unique_ptr<Obj3d>> object3ds_;
	std::unique_ptr<Obj3d> fence_ = nullptr;
	std::unique_ptr<Obj3d> sphere_ = nullptr;

	// スプライト
	std::vector<std::unique_ptr<Sprite>> sprites_;
	std::unique_ptr<Sprite> sprite_ = nullptr;

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
};