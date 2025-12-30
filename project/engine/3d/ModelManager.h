#pragma once
#include <cstddef>
#include <map>
#include <string>
#include <memory>
#include "Model.h"
#include "ModelCommon.h"
#include "DirectXCommon.h"


// モデルマネージャークラス
class ModelManager{
public:
	// / 初期化
	void Initialize(DirectXCommon* dxCommon);




	// シングルストーンのインスタンスを取得
	static ModelManager* GetInstance(){
		if ( instance_ == nullptr ) {
			instance_ = new ModelManager();
		}
		return instance_;
	}
	// インスタンスの破棄
	void Finalize(){
		if ( instance_ != nullptr ) {
			delete instance_;
			instance_ = nullptr;
		}
	}
	// モデルのロード
	void LoadModel(const std::string& modelName, const std::string& directoryPath, const std::string& filename);
	// モデルの検索
	Model* FindModel(const std::string& filePath);
	// モデルの取得
	std::map<std::string,std::unique_ptr<Model>> models_;


private:

	ModelCommon* modelCommon_ = nullptr;


	// コンストラクタ
	ModelManager() = default;

	// デストラクタ
	~ModelManager() = default;
	// コピーコンストラクタ（禁止）
	ModelManager(const ModelManager&) = delete;
	// コピー代入演算子（禁止）
	ModelManager& operator=(const ModelManager&) = delete;
	// 静的メンバ変数の定義
	static ModelManager* instance_;





};

