#pragma once
#include <string>
#include <d3d12.h>

// --- エンジン側のファイル ---
#include "engine/camera/Camera.h"
#include "engine/graphics/TextureManager.h"
#include "engine/3d/model/Model.h"

// =====================================================================
//  Skybox：空の箱（スカイボックス）。
//   6 面の絵（キューブマップ）を、カメラを包む球の内側に描いて、遠くの景色にする
// =====================================================================
class Skybox{
public:
	// 初期化
	void Initialize(const std::string& textureFilePath, ID3D12GraphicsCommandList* commandList);

	// 描画
	void Draw(ID3D12GraphicsCommandList* commandList, Camera* camera);

private:
	TextureData textureData_; // キューブマップテクスチャ
	Model* model_ = nullptr;  // 描画用の球体モデル
};