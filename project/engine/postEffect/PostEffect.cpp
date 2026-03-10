#include "PostEffect.h"
// --- 標準ライブラリ ---
#include "externals/nlohmann/json.hpp"
#include <fstream>
#include <iostream>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

// --- エンジン側のファイル ---
#include "engine/graphics/PipelineManager.h"
#include "engine/graphics/SrvManager.h"
#include "engine/graphics/ResourceFactory.h"


using json = nlohmann::json;

void PostEffect::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height) {
	// RenderTextureの生成と初期化
	renderTexture_ = std::make_unique<RenderTexture>();
	renderTexture_->Initialize(dxCommon, srvManager, width, height);

	resultTexture_ = std::make_unique<RenderTexture>();
	resultTexture_->Initialize(dxCommon, srvManager, width, height);
	// 時間管理用のバッファを生成してマッピングする
	timeResource_ = ResourceFactory::GetInstance()->CreateBufferResource(sizeof(float));
	timeResource_->Map(0, nullptr, reinterpret_cast< void** >( &timeData_ ));
	*timeData_ = 0.0f;
}

void PostEffect::Update(){
	if ( isActive_ ) {
		time_ += timeSpeed_; // 自分の持っているスピード分だけ進める
		if ( timeData_ ) {
			*timeData_ = time_;
		}
	}
}


uint32_t PostEffect::GetSrvIndex() const{
	if ( isActive_ ) {
		return resultTexture_->GetSrvIndex(); // ONなら2枚目(エフェクト後)を渡す
	} else {
		return renderTexture_->GetSrvIndex(); // OFFなら1枚目(エフェクト前)を渡す
	}
}

void PostEffect::Finalize() {
	// スマートポインタを明示的にリセットして、GPUのメモリリークを防ぐ
	if (renderTexture_) {
		renderTexture_.reset();
	}
	if ( resultTexture_ ) { resultTexture_.reset(); }

	if ( timeResource_ ) {
		timeResource_.Reset();
	}
}

void PostEffect::PreDrawScene(ID3D12GraphicsCommandList* commandList, DirectXCommon* dxCommon) {
	
	
	renderTexture_->PreDrawScene(commandList, dxCommon);
}

void PostEffect::PostDrawScene(ID3D12GraphicsCommandList* commandList, DirectXCommon* dxCommon) {
	
	
	renderTexture_->PostDrawScene(commandList, dxCommon);
}

void PostEffect::Draw(ID3D12GraphicsCommandList* commandList, DirectXCommon* dxCommon) {
	// -----------------------------------------------------------------
	// 【ステップ①】1枚目（素の絵）にエフェクトをかけて、2枚目（結果用）に保存する
	// -----------------------------------------------------------------

	// 描画先を「2枚目の画用紙」に切り替える（2枚目を書き込みモードにする）
	resultTexture_->PreDrawScene(commandList, dxCommon);

	// かけるエフェクトを決定する（OFFの場合はエフェクトなしのパイプラインにする）
	PostEffectType applyType = isActive_ ? currentType_ : PostEffectType::None;
	PipelineManager::GetInstance()->SetPostEffectPipeline(commandList, applyType);

	// 三角形を描画する設定
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 1枚目（読み込みモードになっているはずの画用紙）をシェーダーに渡す
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, renderTexture_->GetSrvIndex());

	commandList->SetGraphicsRootConstantBufferView(1, timeResource_->GetGPUVirtualAddress());


	// エフェクトをかけながら2枚目に描き込む！
	commandList->DrawInstanced(3, 1, 0, 0);

	// 2枚目の画用紙のお絵かきを終了する（2枚目を読み込みモードにする）
	resultTexture_->PostDrawScene(commandList, dxCommon);

	// -----------------------------------------------------------------
	// 【ステップ②】出来上がった2枚目の絵を、メイン画面（スワップチェーン）に直接描画する
	// -----------------------------------------------------------------

	// エフェクトなし（ただ画像を表示するだけ）のパイプラインをセット
	PipelineManager::GetInstance()->SetPostEffectPipeline(commandList, PostEffectType::None);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 完成した2枚目の画用紙をシェーダーに渡す
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, resultTexture_->GetSrvIndex());
	
	// メイン画面に描画する
	commandList->DrawInstanced(3, 1, 0, 0);

}

void PostEffect::DrawDebugUI(){
#ifdef USE_IMGUI
	if ( ImGui::Begin("インスペクター (詳細設定)") ) {
		if ( ImGui::CollapsingHeader("ポストエフェクト設定 (Post Effect)", ImGuiTreeNodeFlags_DefaultOpen) ) {
			// エフェクトの有効/無効を切り替えるチェックボックス
			ImGui::Checkbox("エフェクトを有効化", &isActive_);
			ImGui::Separator();

			// エフェクト名のリスト（enumの順番と合わせる）
			// コンボボックスの中身も分かりやすく日本語化！
			const char* effectNames[] = {
				"なし (None)",
				"グレースケール (Grayscale)",
				"セピア (Sepia)",
				"ビネット・周辺減光 (Vignetting)",
				"ぼかし弱 (Box Filter)",
				"ぼかし強 (Box Filter 5x5)",
				"綺麗にぼかす (Gaussian Filter)",
				"アウトライン・輪郭抽出 (Outline)",
				"放射状ブラー (Radial Blur)",
				"ノイズ・砂嵐 (Random Noise)"
			};

			int currentItem = static_cast< int >( currentType_ );

			// コンボボックスで切り替え
			if ( ImGui::Combo("エフェクトの種類", &currentItem, effectNames, IM_ARRAYSIZE(effectNames)) ) {
				currentType_ = static_cast< PostEffectType >( currentItem );
			}
			// ノイズエフェクトのときだけ、時間の進む速さを調整するスライダーを表示
			if ( currentType_ == PostEffectType::RandomNoise ) {
				ImGui::SliderFloat("ノイズの速度 (Time Speed)", &timeSpeed_, 0.0f, 0.5f);
				if ( ImGui::Button("速度リセット") ) {
					timeSpeed_ = 0.05f;
				}
			}

			ImGui::Separator();

			// セーブ＆ロードボタン
			if ( ImGui::Button("設定を保存") ) {
				Save();
			}
			ImGui::SameLine();
			if ( ImGui::Button("設定を読み込む") ) {
				Load();
			}
		}
	}
	ImGui::End();
#endif
}


void PostEffect::Save(const std::string& filePath) {
	json j;
	j["effectType"] = static_cast<int>(currentType_);

	j["isActive"] = isActive_;

	std::ofstream file(filePath);
	if (file.is_open()) {
		file << j.dump(4);
		file.close();
	}
}

void PostEffect::Load(const std::string& filePath) {
	std::ifstream file(filePath);
	if (!file.is_open()) {
		return; // ファイルがなければ何もしない
	}

	json j;
	file >> j;

	if (j.contains("effectType")) {
		int typeNum = j["effectType"];
		// 範囲外の数値が入らないように安全対策
		if (typeNum >= 0 && typeNum < static_cast<int>(PostEffectType::Count)) {
			currentType_ = static_cast<PostEffectType>(typeNum);
		}
	}
	if (j.contains("isActive")) {
		isActive_ = j["isActive"];
	}

}