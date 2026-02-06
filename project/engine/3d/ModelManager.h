#pragma once
#include <cstddef>
#include <map>
#include <string>
#include <memory>
// 前方宣言
class Model;
class ModelCommon;
class DirectXCommon;

// モデルマネージャークラス
class ModelManager{
public:
	// / 初期化
	void Initialize(DirectXCommon* dxCommon);
	// 球モデルの作成
	void CreateSphereModel(const std::string& modelName, int subdivision);


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
	

private:

	// コンストラクタ
	ModelManager();

	// デストラクタ
	~ModelManager();
	// コピーコンストラクタ（禁止）
	ModelManager(const ModelManager&) = delete;
	// コピー代入演算子（禁止）
	ModelManager& operator=(const ModelManager&) = delete;
	// 静的メンバ変数の定義
	static ModelManager* instance_;


private:

	// ModelCommonのポインタ
	std::unique_ptr<ModelCommon> modelCommon_ = nullptr;

	// モデルの取得
	std::map<std::string, std::unique_ptr<Model>> models_;

};

