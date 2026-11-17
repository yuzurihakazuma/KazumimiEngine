#pragma once
// --- エンジン側のファイル ---
#include "engine/graphics/TextureManager.h"

#include <string>
#include <d3d12.h>

class Camera;
class Model;

// =====================================================================
//  Skybox：いちばん奥の空（スカイドーム）。6 面の絵（キューブマップ）を、カメラを包む球の内側に描く。
//   ・カメラの向きだけを使い、位置は使わない（どこへ動いても、空は無限に遠くに見える）
//   ・奥行きを「いちばん奥（1.0）」に決め打ちし、奥行きは書き込まない。ブロックや地面にめり込むことはない
//   ・3D の物より先に描く（物が上から描かれる＝物の無いすき間だけが空になる）
// =====================================================================
class Skybox{
public:
	// 初期化：キューブマップ（.dds）を読む。球のモデルが無ければ作る
	void Initialize(const std::string& textureFilePath);

	// 描画（3D の物より先に。シーンの描き先・奥行きは準備済みであること）
	void Draw(ID3D12GraphicsCommandList* commandList, Camera* camera);

private:
	TextureData textureData_; // キューブマップテクスチャ
	Model* model_ = nullptr;  // 描画用の球体モデル
};
