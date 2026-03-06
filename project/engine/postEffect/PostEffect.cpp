#include "PostEffect.h"
// --- 標準ライブラリ ---
#include "externals/imgui/imgui.h"
#include "externals/nlohmann/json.hpp"
#include <fstream>
#include <iostream>


// --- エンジン側のファイル ---
#include "engine/graphics/PipelineManager.h"
#include "engine/graphics/SrvManager.h"



using json = nlohmann::json;

void PostEffect::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height) {
	// RenderTextureの生成と初期化
	renderTexture_ = std::make_unique<RenderTexture>();
	renderTexture_->Initialize(dxCommon, srvManager, width, height);
}

void PostEffect::Finalize() {
	// スマートポインタを明示的にリセットして、GPUのメモリリークを防ぐ
	if (renderTexture_) {
		renderTexture_.reset();
	}
}

void PostEffect::PreDrawScene(ID3D12GraphicsCommandList* commandList, DirectXCommon* dxCommon) {
	// エフェクトが無効な場合は何もしない
	if (!isActive_){
		return;
	}
	
	renderTexture_->PreDrawScene(commandList, dxCommon);
}

void PostEffect::PostDrawScene(ID3D12GraphicsCommandList* commandList, DirectXCommon* dxCommon) {
	// エフェクトが無効な場合は何もしない
	if (!isActive_) {
		return;
	}
	
	renderTexture_->PostDrawScene(commandList, dxCommon);
}

void PostEffect::Draw(ID3D12GraphicsCommandList* commandList) {
	// エフェクトが無効な場合は何もしない
	if (!isActive_) {
		return;
	}
	
	// 1. パイプライン設定
	PipelineManager::GetInstance()->SetPostEffectPipeline(commandList, currentType_);

	// 2. トポロジー設定（三角形で描画）
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 3. SRV（描き込んだ画像）をシェーダーに渡す
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, renderTexture_->GetSrvIndex());

	// 4. 描画コマンド（画面全体を覆う三角形）
	commandList->DrawInstanced(3, 1, 0, 0);
}

void PostEffect::DrawDebugUI() {
#ifdef USE_IMGUI
	if (ImGui::Begin("Inspector")) {
		if (ImGui::CollapsingHeader("Post Effect Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
			// エフェクトの有効/無効を切り替えるチェックボックス
			ImGui::Checkbox("Enable Post Effect", &isActive_);
			ImGui::Separator();

			// エフェクト名のリスト（enumの順番と合わせる）
			const char* effectNames[] = {
				"None", "Grayscale", "Sepia", "Vignetting",
				"Box Filter", "Box Filter 5x5", "Gaussian Filter"
			};

			int currentItem = static_cast<int>(currentType_);

			// コンボボックスで切り替え
			if (ImGui::Combo("Effect Type", &currentItem, effectNames, IM_ARRAYSIZE(effectNames))) {
				currentType_ = static_cast<PostEffectType>(currentItem);
			}

			ImGui::Separator();

			// セーブ＆ロードボタン
			if (ImGui::Button("Save Effect Settings")) {
				Save();
			}
			ImGui::SameLine();
			if (ImGui::Button("Load Effect Settings")) {
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