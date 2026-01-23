#include "ModelManager.h"


// 静的メンバ変数の実体定義
ModelManager* ModelManager::instance_ = nullptr;

void ModelManager::Initialize(DirectXCommon* dxCommon){
	modelCommon_ = new ModelCommon();
	modelCommon_->Initialize(dxCommon);

}

void ModelManager::CreateSphereModel(const std::string& modelName, int subdivision){

	// 重複読み込み防止：すでに同じ名前で登録されていたら何もしない
	if ( models_.contains(modelName) ) {
		return;
	}

	// 1. モデル生成
	std::unique_ptr<Model> newModel = std::make_unique<Model>();

	// 2. モデル初期化 (ModelManagerが持っているModelCommonを渡す)
	newModel->InitializeSphere(modelCommon_,subdivision);

	// 3. マップに登録 (moveで所有権をマップに移す)
	models_.insert(std::make_pair(modelName, std::move(newModel)));



}

void ModelManager::LoadModel(const std::string& modelName, const std::string& directoryPath, const std::string& filename){
	// 重複読み込み防止：すでに同じ名前で登録されていたら何もしない
	if ( models_.contains(modelName) ) {
		return;
	}

	// 1. モデル生成
	std::unique_ptr<Model> newModel = std::make_unique<Model>();

	// 2. モデル初期化 (ModelManagerが持っているModelCommonを渡す)
	newModel->Initialize(modelCommon_, directoryPath, filename);

	// 3. マップに登録 (moveで所有権をマップに移す)
	models_.insert(std::make_pair(modelName, std::move(newModel)));
}
// モデルの検索
Model* ModelManager::FindModel(const std::string& filePath){
	
	// 読み込みファイルを検索
	if ( models_.contains(filePath) ){
		// 見つかった場合はポインタを返す
		return models_.at(filePath).get();
	}
	
	
	// マップから検索
	return nullptr;
}
