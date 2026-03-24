#include "PostEffect.h"
// --- 標準ライブラリ ---
#include "externals/nlohmann/json.hpp"
#include <fstream>
#include <iostream>
#include <utility> 

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

// --- エンジン側のファイル ---
#include "engine/graphics/PipelineManager.h"
#include "engine/graphics/SrvManager.h"
#include "engine/graphics/ResourceFactory.h"
#include "engine/base/DirectXCommon.h"
#include "Bloom.h"

using json = nlohmann::json;

void PostEffect::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height) {
	
	dxCommon_ = dxCommon;

	// 配列にした2枚の画用紙を初期化
	for (int i = 0; i < 2; ++i) {
		renderTextures_[i] = std::make_unique<RenderTexture>();
		renderTextures_[i]->Initialize(dxCommon, srvManager, width, height);
	}
	// 1枚目を「エフェクト前」、2枚目を「エフェクト後」としてわかりやすく変数に入れる
	timeResource_ = ResourceFactory::GetInstance()->CreateBufferResource(sizeof(float));
	timeResource_->Map(0, nullptr, reinterpret_cast<void**>(&timeData_));
	*timeData_ = 0.0f;

	Bloom::GetInstance()->Initialize(dxCommon, SrvManager::GetInstance(), width, height);
	Bloom::GetInstance()->Load("resources/bloom.json");
}


void PostEffect::Update() {
	if (isActive_) {
		time_ += timeSpeed_; // 自分の持っているスピード分だけ進める
		if (timeData_) {
			*timeData_ = time_;
		}
	}
}


uint32_t PostEffect::GetSrvIndex() const {
	return renderTextures_[finalResultIndex_]->GetSrvIndex();
}

void PostEffect::Finalize() {
	// スマートポインタを明示的にリセットして、GPUのメモリリークを防ぐ
	for (int i = 0; i < 2; ++i) {
		if (renderTextures_[i]) {
			renderTextures_[i].reset();
		}
	}
	if (timeResource_) {
		timeResource_.Reset();
	}
	
	Bloom::GetInstance()->Finalize();
}

void PostEffect::PreDrawScene(ID3D12GraphicsCommandList* commandList) {


	renderTextures_[0]->PreDrawScene(commandList, dxCommon_);
}

void PostEffect::PostDrawScene(ID3D12GraphicsCommandList* commandList) {


	renderTextures_[0]->PostDrawScene(commandList, dxCommon_);
}

void PostEffect::Draw(ID3D12GraphicsCommandList* commandList) {
	
	auto SetRenderTargetToSwapchain = [&]() {
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = dxCommon_->GetBackBufferRtvHandle();
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon_->GetDsvHandle();
		commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
		D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(dxCommon_->GetClientWidth()), static_cast<float>(dxCommon_->GetClientHeight()), 0.0f, 1.0f };
		D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(dxCommon_->GetClientWidth()), static_cast<LONG>(dxCommon_->GetClientHeight()) };
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &scissorRect);
		};
	
	
	// エフェクト全体がOFFなら、そのまま画面に出して終了
	if (!isActive_) {
		SetRenderTargetToSwapchain();

		PipelineManager::GetInstance()->SetPostEffectPipeline(commandList, PostEffectType::None);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, renderTextures_[0]->GetSrvIndex());
		commandList->DrawInstanced(3, 1, 0, 0);
		return;
	}

	uint32_t src = 0;  // 最初の読み込み元は、ゲームが描かれた 0番
	uint32_t dest = 1; // 最初の書き込み先は 1番

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// -------------------------------------------------------------
	// ここにかけたいエフェクトを書き出す
	// -------------------------------------------------------------
	for (int i = 1; i < static_cast<int>(PostEffectType::Count); ++i) {
		// ON/OFFも指定できるバージョン
		ApplyEffect(commandList, dxCommon_, static_cast<PostEffectType>(i), activeEffects_[i], src, dest);
	}

	// -------------------------------------------------------------
	// 最終出力（すべてのバケツリレーが終わった最新の絵「src」を画面に出す）
	// -------------------------------------------------------------
	SetRenderTargetToSwapchain();
	// もしエフェクトが1つもかかっていないなら、最初から画面に出す（ピンポン描画をしない）
	finalResultIndex_ = src;

	PipelineManager::GetInstance()->SetPostEffectPipeline(commandList, PostEffectType::None);
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, renderTextures_[src]->GetSrvIndex());
	commandList->DrawInstanced(3, 1, 0, 0);
}


// ポストエフェクトの描画（巨大な三角形を描く）※エフェクトの種類を指定して、ON/OFFも指定できるバージョン
void PostEffect::ApplyEffect(
	ID3D12GraphicsCommandList* commandList, DirectXCommon* dxCommon,
	PostEffectType type, bool isEffectActive, uint32_t& src, uint32_t& dest)
{
	// チェックボックスがOFFなら何もしないで次へ！
	if (!isEffectActive) {
		return;
	}

	// 1. 書き込み先の画用紙をセット
	renderTextures_[dest]->PreDrawScene(commandList, dxCommon);

	// 2. パイプライン（シェーダー）をセット
	PipelineManager::GetInstance()->SetPostEffectPipeline(commandList, type);

	// 3. 読み込み元の絵（前までの結果）をセット
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, renderTextures_[src]->GetSrvIndex());

	// 4. 特殊なパラメータ渡し（ノイズの時間など）
	commandList->SetGraphicsRootConstantBufferView(1, timeResource_->GetGPUVirtualAddress());

	// 5. 描画！
	commandList->DrawInstanced(3, 1, 0, 0);

	// 6. 書き込み終了
	renderTextures_[dest]->PostDrawScene(commandList, dxCommon);

	// 7. ピンポン！（次に向けて、読み書きの番号を入れ替える）
	std::swap(src, dest);
}

// デバッグ用UIの描画
void PostEffect::DrawDebugUI() {
#ifdef USE_IMGUI
	if (ImGui::Begin("インスペクター (詳細設定)")) {

		static const char* effectNames[] = {
			"None (使わない)",
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
		// --- ポストエフェクトのON/OFF設定 ---
		if (ImGui::CollapsingHeader("ポストエフェクト設定 (Post Effect)", ImGuiTreeNodeFlags_DefaultOpen)) {

			ImGui::Checkbox("エフェクト全体を有効化", &isActive_);
			ImGui::Separator();

			if (isActive_) {
				// それぞれのエフェクトのON/OFFを切り替えるチェックボックスを出す
				for (int i = 1; i < static_cast<int>(PostEffectType::Count); ++i) {
					ImGui::Checkbox(effectNames[i], &activeEffects_[i]);

					// ノイズ(RandomNoise)がONの時だけ、スピード調整スライダーを出す
					if (i == static_cast<int>(PostEffectType::RandomNoise) && activeEffects_[i]) {
						ImGui::Indent(); // ちょっと右にずらす
						ImGui::SliderFloat("ノイズの速度", &timeSpeed_, 0.0f, 0.5f);
						if (ImGui::Button("速度リセット")) { timeSpeed_ = 0.05f; }
						ImGui::Unindent();
					}
				}
			}

			ImGui::Separator();

			if (ImGui::Button("設定を保存")) { Save(); }
			ImGui::SameLine();
			if (ImGui::Button("設定を読み込む")) { Load(); }
		}

		// --- 現在適用中のエフェクト表示 ---
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[現在適用中のエフェクト]"); // 緑色で文字を表示
		bool hasActiveEffect = false;
		if (isActive_) {
			for (int i = 1; i < static_cast<int>(PostEffectType::Count); ++i) {
				if (activeEffects_[i]) {
					// ★一番上で作っているので、ここからも無事にアクセスできる！
					ImGui::BulletText("%s", effectNames[i]);
					hasActiveEffect = true;
				}
			}
		}

		// 1つもかかっていない場合の表示
		if (!isActive_ || !hasActiveEffect) {
			ImGui::Text("  (なし)");
		}

	}
	ImGui::End();
#endif
}
// 設定をJSONファイルに保存する関数
void PostEffect::Save(const std::string& filePath) {
	json j;

	// 大元スイッチと時間スピードを保存
	j["isActive"] = isActive_;
	j["timeSpeed"] = timeSpeed_;

	// ★ 配列の中身をすべて保存 (例: "1": true, "2": false ...)
	for (int i = 1; i < static_cast<int>(PostEffectType::Count); ++i) {
		j["activeEffects"][std::to_string(i)] = activeEffects_[i];
	}

	std::ofstream file(filePath);
	if (file.is_open()) {
		file << j.dump(4);
		file.close();
	}
}
// JSONファイルから設定を読み込む関数
void PostEffect::Load(const std::string& filePath) {
	std::ifstream file(filePath);
	if (!file.is_open()) {
		return; // ファイルがなければ何もしない
	}

	json j;
	file >> j;

	if (j.contains("isActive")) {
		isActive_ = j["isActive"];
	}
	if (j.contains("timeSpeed")) {
		timeSpeed_ = j["timeSpeed"];
	}

	// ★ JSONから配列にON/OFFの設定を復元する
	if (j.contains("activeEffects")) {
		for (int i = 1; i < static_cast<int>(PostEffectType::Count); ++i) {
			std::string key = std::to_string(i);
			if (j["activeEffects"].contains(key)) {
				activeEffects_[i] = j["activeEffects"][key];
			}
		}
	}
}