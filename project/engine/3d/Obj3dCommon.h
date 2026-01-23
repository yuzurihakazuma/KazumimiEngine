#pragma once
#include <wrl.h>
#include <d3d12.h>
#include"DirectXCommon.h"
#include "LogManager.h"
#include"ShaderCompiler.h"
#include "struct.h"

class Obj3dCommon{
public:
	// ライト構造体
	struct DirectionalLight{
		Vector4 color;     // ライトの色
		Vector3 direction; // ライトの向き
		float intensity;   // 輝度
	};

	// ライトデータの取得
	DirectionalLight* GetLightData(){ return directionalLightData_; }
	// ライトリソースの取得
	ID3D12Resource* GetLightResource() const{ return directionalLightResource_.Get(); }

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(DirectXCommon* dxCommon);
	// 共通の描画設定
	void PreDraw(ID3D12GraphicsCommandList* commandList);
	DirectXCommon* GetDxCommon() const{ return dxCommon_; }

private:

	DirectXCommon* dxCommon_ = nullptr; // 所有しない参照

	// 平行光源リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalResourceLight_;
	DirectionalLight* directionalLightData_ = nullptr;
	// ライト用リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;

};

