#pragma once
// --- 標準ライブラリ ---
#include <wrl.h>
#include <d3d12.h>

// --- エンジン側のファイル ---
#include "engine/math/struct.h"

// 前方宣言
class DirectXCommon;


// 3Dオブジェクト共通クラス
class Obj3dCommon{
public:
	
	static Obj3dCommon* GetInstance(){
		static Obj3dCommon instance;
		return &instance;
	}
	
	
	
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
	// コンストラクタを private にして外部からの生成を禁止
	Obj3dCommon() = default;
	~Obj3dCommon() = default;
	Obj3dCommon(const Obj3dCommon&) = delete;
	Obj3dCommon& operator=(const Obj3dCommon&) = delete;

private:
	DirectXCommon* dxCommon_ = nullptr; // 所有しない参照

	// 平行光源リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalResourceLight_;
	DirectionalLight* directionalLightData_ = nullptr;
	// ライト用リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;

};

