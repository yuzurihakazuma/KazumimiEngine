#include "ModelCommon.h"
#include "DirectXCommon.h" 
#include <cassert>

void ModelCommon::Initialize(DirectXCommon* dxCommon){

	// NULLチェック
	assert(dxCommon);
	// メンバ変数にセット
	this->dxCommon_ = dxCommon;


}

DirectXCommon* ModelCommon::GetDxCommon() const{
	assert(dxCommon_);
	
	return dxCommon_; 

}

