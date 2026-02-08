#pragma once
// 前方宣言
class DirectXCommon;

// モデル共通クラス
class ModelCommon{
public:
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(DirectXCommon* dxCommon);

	DirectXCommon* GetDxCommon() const;

private:
	
	
	DirectXCommon* dxCommon_ = nullptr;

};

