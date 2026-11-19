#include "ModelCommon.h"
// --- 標準ライブラリ ---
#include <cassert>

// --- エンジン側のファイル ---
#include "engine/base/DirectXCommon.h"

namespace KazumimiEngine {

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

} // namespace KazumimiEngine
