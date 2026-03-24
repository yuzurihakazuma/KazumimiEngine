#include "Bloom.h"
#include "engine/base/DirectXCommon.h"
#include "engine/graphics/SrvManager.h"
#include "engine/graphics/PipelineManager.h"
#include "engine/graphics/ResourceFactory.h"
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif
#include <RootSignatureBuilder.h>
#include <GraphicsPipelineBuilder.h>

#include "externals/nlohmann/json.hpp"
#include <fstream>
using json = nlohmann::json;

// 初期化
void Bloom::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height) {
	// 1. 抽出結果を保存するキャンバスの用意
	extractTexture_ = std::make_unique<RenderTexture>();
	extractTexture_->Initialize(dxCommon, srvManager, width, height);

	bloomDataResource_ = ResourceFactory::GetInstance()->CreateBufferResource(sizeof(BloomData));
	bloomDataResource_->Map(0, nullptr, reinterpret_cast<void**>(&bloomData_));
	bloomData_->threshold = 1.0f; // 最初は「1.0」以上の明るさを抽出
	RootSignatureBuilder rsBuilder;
	rsBuilder.AddCBV(0, D3D12_SHADER_VISIBILITY_PIXEL);
	rsBuilder.AddDescriptorTableSRV(0, D3D12_SHADER_VISIBILITY_PIXEL);
	rsBuilder.AddDefaultSampler(0);
	rsBuilder.Build(dxCommon->GetDevice(), rootSignature_);

	auto vsBlob = dxCommon->GetShaderCompiler().CompileShader(L"resources/shaders/PostEffect/Fullscreen.VS.hlsl", L"vs_6_0");
	auto psBlob = dxCommon->GetShaderCompiler().CompileShader(L"resources/shaders/PostEffect/bloom/BloomExtract.PS.hlsl", L"ps_6_0");

	GraphicsPipelineBuilder psoBuilder;
	psoBuilder.SetRootSignature(rootSignature_.Get())
		.SetShaders(
			vsBlob.Get(), psBlob.Get()
		)
		.SetInputLayoutEmpty()
		.SetBlendMode(BlendMode::kNormal)
		.SetCullMode(D3D12_CULL_MODE_NONE)
		.SetDepthStencil(false, false);

	psoBuilder.Build(dxCommon->GetDevice(), pipelineState_);


	blurTextures_[0] = std::make_unique<RenderTexture>();
	blurTextures_[0]->Initialize(dxCommon, srvManager, width, height);
	blurTextures_[1] = std::make_unique<RenderTexture>();
	blurTextures_[1]->Initialize(dxCommon, srvManager, width, height);

	blurDataResource_ = ResourceFactory::GetInstance()->CreateBufferResource(sizeof(BlurData));
	blurDataResource_->Map(0, nullptr, reinterpret_cast<void**>(&blurData_));
	blurData_->texelSize[0] = 1.0f / width;
	blurData_->texelSize[1] = 1.0f / height;

	// ブラー用のルートシグネチャ (Extractと同じくCBVとSRVを1つずつ)
	RootSignatureBuilder blurRsBuilder;
	blurRsBuilder.AddCBV(0, D3D12_SHADER_VISIBILITY_PIXEL);
	blurRsBuilder.AddDescriptorTableSRV(0, D3D12_SHADER_VISIBILITY_PIXEL);
	blurRsBuilder.AddDefaultSampler(0);
	blurRsBuilder.Build(dxCommon->GetDevice(), blurRootSignature_);

	// ★ここがポイント！ VSは「Fullscreen」を使い回し、PSは「GaussianBlur」を使う！
	auto blurVsBlob = dxCommon->GetShaderCompiler().CompileShader(L"resources/shaders/PostEffect/Fullscreen.VS.hlsl", L"vs_6_0");
	auto blurPsBlob = dxCommon->GetShaderCompiler().CompileShader(L"resources/shaders/PostEffect/bloom/GaussianBlur.PS.hlsl", L"ps_6_0");

	GraphicsPipelineBuilder blurPsoBuilder;
	blurPsoBuilder.SetRootSignature(blurRootSignature_.Get())
		.SetShaders(blurVsBlob.Get(), blurPsBlob.Get())
		.SetInputLayoutEmpty()
		.SetBlendMode(BlendMode::kNormal)
		.SetCullMode(D3D12_CULL_MODE_NONE)
		.SetDepthStencil(false, false);
	blurPsoBuilder.Build(dxCommon->GetDevice(), blurPipelineState_);

	combineTexture_ = std::make_unique<RenderTexture>();
	combineTexture_->Initialize(dxCommon, srvManager, width, height);

	RootSignatureBuilder combineRsBuilder;
	// t0 と t1 の2つの画像を読み込めるようにする！
	combineRsBuilder.AddDescriptorTableSRV(0, D3D12_SHADER_VISIBILITY_PIXEL); // t0: メイン画面
	combineRsBuilder.AddDescriptorTableSRV(1, D3D12_SHADER_VISIBILITY_PIXEL); // t1: 光の画面
	combineRsBuilder.AddDefaultSampler(0); // s0: サンプラー
	combineRsBuilder.Build(dxCommon->GetDevice(), combineRootSignature_);

	auto combineVsBlob = dxCommon->GetShaderCompiler().CompileShader(L"resources/shaders/PostEffect/Fullscreen.VS.hlsl", L"vs_6_0");
	auto combinePsBlob = dxCommon->GetShaderCompiler().CompileShader(L"resources/shaders/PostEffect/bloom/BloomCombine.PS.hlsl", L"ps_6_0");

	GraphicsPipelineBuilder combinePsoBuilder;
	combinePsoBuilder.SetRootSignature(combineRootSignature_.Get())
		.SetShaders(combineVsBlob.Get(), combinePsBlob.Get())
		.SetInputLayoutEmpty()
		.SetBlendMode(BlendMode::kNormal) // シェーダーの中で足し算しているのでNormalでOK！
		.SetCullMode(D3D12_CULL_MODE_NONE)
		.SetDepthStencil(false, false);
	combinePsoBuilder.Build(dxCommon->GetDevice(), combinePipelineState_);

}


// 工程①：高輝度抽出を描画する
void Bloom::DrawBlur(ID3D12GraphicsCommandList* commandList) {
	// 共通設定
	commandList->SetGraphicsRootSignature(blurRootSignature_.Get());
	commandList->SetPipelineState(blurPipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// --- 1パス目：横方向にぼかす (抽出画像  blurTextures_[0]) ---
	blurTextures_[0]->PreDrawScene(commandList, DirectXCommon::GetInstance());
	blurData_->direction[0] = 1.0f; // X方向
	blurData_->direction[1] = 0.0f;
	commandList->SetGraphicsRootConstantBufferView(0, blurDataResource_->GetGPUVirtualAddress());
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(1, extractTexture_->GetSrvIndex());
	commandList->DrawInstanced(3, 1, 0, 0);

	blurTextures_[0]->PostDrawScene(commandList, DirectXCommon::GetInstance());

	// --- 2パス目：縦方向にぼかす (blurTextures_[0]  blurTextures_[1]) ---
	blurTextures_[1]->PreDrawScene(commandList, DirectXCommon::GetInstance());
	blurData_->direction[0] = 0.0f;
	blurData_->direction[1] = 1.0f; // Y方向
	commandList->SetGraphicsRootConstantBufferView(0, blurDataResource_->GetGPUVirtualAddress());
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(1, blurTextures_[0]->GetSrvIndex());
	commandList->DrawInstanced(3, 1, 0, 0);

	blurTextures_[1]->PostDrawScene(commandList, DirectXCommon::GetInstance());
}


void Bloom::DrawExtract(ID3D12GraphicsCommandList* commandList, uint32_t srcSrvIndex) {
	// 1. 描画先を「Bloomの抽出用キャンバス」に切り替え
	extractTexture_->PreDrawScene(commandList, DirectXCommon::GetInstance());

	// 2. 自分専用のパイプラインをセット！
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 3. データを渡す (パラメータ0番: CBV, 1番: SRV)
	commandList->SetGraphicsRootConstantBufferView(0, bloomDataResource_->GetGPUVirtualAddress()); // b0: しきい値
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(1, srcSrvIndex); // t0: 元の画面画像

	// 4. 全画面描画
	commandList->DrawInstanced(3, 1, 0, 0);

	extractTexture_->PostDrawScene(commandList, DirectXCommon::GetInstance());
}
// 工程③ 元の画像と光を合成する！
void Bloom::DrawCombine(ID3D12GraphicsCommandList* commandList, uint32_t mainSrvIndex) {
	// 1. 描画先を「最終キャンバス」に切り替え
	combineTexture_->PreDrawScene(commandList, DirectXCommon::GetInstance());

	// 2. パイプライン設定
	commandList->SetGraphicsRootSignature(combineRootSignature_.Get());
	commandList->SetPipelineState(combinePipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 3. テクスチャを2枚渡す！
	// t0: 元の画面 (PostEffectの結果)
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, mainSrvIndex);
	// t1: ぼかした光 (Blurの結果)
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(1, blurTextures_[1]->GetSrvIndex());

	// 4. 全画面に描画
	commandList->DrawInstanced(3, 1, 0, 0);

	combineTexture_->PostDrawScene(commandList, DirectXCommon::GetInstance());
}

void Bloom::DrawDebugUI() {
#ifdef USE_IMGUI
	ImGui::Begin("Bloom Settings");

	// ★ ON/OFFのチェックボックス
	ImGui::Checkbox("Enable Bloom", &isEnabled_);

	// ONの時だけ設定をいじれるようにする
	if (isEnabled_) {
		ImGui::DragFloat("Threshold (光る基準値)", &bloomData_->threshold, 0.01f, 0.0f, 5.0f);
	}

	ImGui::Separator();

	// ★ セーブボタン
	if (ImGui::Button("Save Settings")) {
		Save("resources/bloom.json");
	}

	ImGui::End();
#endif
}


void Bloom::Render(ID3D12GraphicsCommandList* commandList, uint32_t baseSrvIndex) {
	// もしBloomがOFFなら、計算は一切せず「元の画像」をそのまま結果として返す！
	if (!isEnabled_) {
		resultSrvIndex_ = baseSrvIndex;
		return;
	}

	// BloomがONなら、3つの工程を順番に実行する！
	DrawExtract(commandList, baseSrvIndex);
	DrawBlur(commandList);
	DrawCombine(commandList, baseSrvIndex);

	// 合成まで終わった「最終結果」を結果として返す！
	resultSrvIndex_ = combineTexture_->GetSrvIndex();
}

void Bloom::Save(const std::string& filePath) {
	json j;
	j["isEnabled"] = isEnabled_;
	j["threshold"] = bloomData_->threshold;

	std::ofstream file(filePath);
	if (file.is_open()) {
		file << j.dump(4);
		file.close();
	}
}

void Bloom::Load(const std::string& filePath) {
	std::ifstream file(filePath);
	if (file.is_open()) {
		json j;
		file >> j;
		isEnabled_ = j.value("isEnabled", true);
		bloomData_->threshold = j.value("threshold", 1.0f);
	}
}


void Bloom::Finalize() {
	// 抽出用のリソース解放
	if (extractTexture_) { extractTexture_.reset(); }
	bloomDataResource_.Reset();
	rootSignature_.Reset();
	pipelineState_.Reset();

	// ブラー用のリソース解放
	if (blurTextures_[0]) { blurTextures_[0].reset(); }
	if (blurTextures_[1]) { blurTextures_[1].reset(); }
	blurDataResource_.Reset();
	blurRootSignature_.Reset();
	blurPipelineState_.Reset();

	// 合成用のリソース解放
	if (combineTexture_) { combineTexture_.reset(); }
	combineRootSignature_.Reset();
	combinePipelineState_.Reset();
}