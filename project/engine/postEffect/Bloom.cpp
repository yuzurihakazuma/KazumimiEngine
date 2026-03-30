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


void Bloom::DrawExtract(ID3D12GraphicsCommandList* commandList, uint32_t maskSrvIndex) {
	// 1. 描画先を「Bloomの抽出用キャンバス」に切り替え
	extractTexture_->PreDrawScene(commandList, DirectXCommon::GetInstance());

	// 2. 自分専用のパイプラインをセット！
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// [0]番のポスト(CBV) に、Thresholdなどの「設定データ」を送る！
	commandList->SetGraphicsRootConstantBufferView(0, bloomDataResource_->GetGPUVirtualAddress());

	// [1]番のポスト(SRV) に、受け取った「マスク画像」を送る！
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(1, maskSrvIndex);
	// 4. 全画面に描画
	commandList->DrawInstanced(3, 1, 0, 0);
	extractTexture_->PostDrawScene(commandList, DirectXCommon::GetInstance());
}

// 修正後の Bloom.cpp (DrawCombine)
void Bloom::DrawCombine(ID3D12GraphicsCommandList* commandList, uint32_t mainSrvIndex){
	//  バックバッファではなく、combineTexture_ をお絵かき先にセットする！
	combineTexture_->PreDrawScene(commandList, DirectXCommon::GetInstance());

	// パイプラインなどの設定
	commandList->SetGraphicsRootSignature(combineRootSignature_.Get());
	commandList->SetPipelineState(combinePipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// テクスチャを渡す
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, mainSrvIndex);
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(1, blurTextures_[1]->GetSrvIndex());

	commandList->DrawInstanced(3, 1, 0, 0);

	//  お絵かき終了（読み込みモードに戻す）
	combineTexture_->PostDrawScene(commandList, DirectXCommon::GetInstance());
}

void Bloom::DrawDebugUI() {
#ifdef USE_IMGUI

	ImGui::Checkbox("発光 (Bloom)", &isEnabled_);

	// ONの時だけ設定をいじれるようにする
	if ( isEnabled_ ) {
		ImGui::Indent(); // 階層を見やすくするために少し右にずらす

		// 🚨変更箇所：ラベルを「光る基準値 (Threshold)」に変更
		// 閾値（どれだけ明るいと光るか）を調整
		ImGui::DragFloat("光る基準値 (Threshold)", &bloomData_->threshold, 0.01f, 0.0f, 10.0f);

		// 🚨追加箇所：マテリアルの発光パワー（どれだけ光るか）を調整するスライダー
		// 対象のマテリアルがセットされている場合のみ描画
		if ( targetEmissivePower_ ) {
			ImGui::DragFloat("光る強さ (Emissive Power)", targetEmissivePower_, 0.01f, 0.0f, 10.0f);
		} else {
			// セットされていない場合は赤文字で注意喚起
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "※対象マテリアルが設定されていません");
		}

		ImGui::Unindent();
	}
#endif
}


void Bloom::Render(ID3D12GraphicsCommandList* commandList, uint32_t colorSrvIndex, uint32_t maskSrvIndex){
	if ( !isEnabled_ ) {
		resultSrvIndex_ = colorSrvIndex; // OFFなら色をそのまま返す
		return;
	}

	//  「マスク画像(フラグ)」から光を抽出する！
	DrawExtract(commandList, maskSrvIndex);
	DrawBlur(commandList);
	//  「色画像」に光を合成する！
	DrawCombine(commandList, colorSrvIndex);

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