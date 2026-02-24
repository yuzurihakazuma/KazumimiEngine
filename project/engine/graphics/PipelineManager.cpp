#include "PipelineManager.h"
// --- 標準ライブラリ ---
#include <cassert>

// --- エンジン側のファイル ---
#include "engine/base/DirectXCommon.h"
#include "engine/graphics/ShaderCompiler.h"
#include "engine/graphics/RootSignatureBuilder.h"
// 終了処理
void PipelineManager::Finalize(){

	spriteRootSignature_.Reset();
	spritePipelineState_.Reset();
	object3DRootSignature_.Reset();
	object3DPipelineState_.Reset();
	object3DPipelineStateNone_.Reset();
	particleRootSignature_.Reset();
	particlePipelineState_.Reset();
}


void PipelineManager::Initialize(DirectXCommon* dxCommon){
		
	assert(dxCommon);
	assert(dxCommon->GetDevice());
	assert(dxCommon->GetSrvManager() != nullptr);
	this->dxCommon_ = dxCommon;



	// スプライト用ルートシグネチャの作成
	CreateSpriteRootSignature();
	// スプライト用グラフィックスパイプラインの作成
	CreateSpriteGraphicsPipeline();

	// 3Dオブジェクト用ルートシグネチャの作成
	CreateObject3DRootSignature();
	// 3Dオブジェクト用グラフィックスパイプラインの作成
	CreateObject3DGraphicsPipeline();

	// パーティクル用ルートシグネチャの作成
	CreateParticleRootSignature();
	// パーティクル用グラフィックスパイプラインの作成
	CreateParticleGraphicsPipeline();


}

void PipelineManager::SetPipeline(
    ID3D12GraphicsCommandList* commandList,
    PipelineType type
){
    assert(commandList);

    switch ( type ) {
    case PipelineType::Sprite:
        commandList->SetGraphicsRootSignature(spriteRootSignature_.Get());
        commandList->SetPipelineState(spritePipelineState_.Get());
        commandList->IASetPrimitiveTopology(
            D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
        );
        break;
	case PipelineType::Object3D:
		commandList->SetGraphicsRootSignature(object3DRootSignature_.Get());
		commandList->SetPipelineState(object3DPipelineState_.Get());
		break;
	case PipelineType::Object3D_CullNone:
		commandList->SetGraphicsRootSignature(object3DRootSignature_.Get());
		commandList->SetPipelineState(object3DPipelineStateNone_.Get());
		break;
	case PipelineType::Particle:
		commandList->SetGraphicsRootSignature(particleRootSignature_.Get());
		commandList->SetPipelineState(particlePipelineState_.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		break;
	
    }
}
// ルートシグネチャの生成 Sprite用
void PipelineManager::CreateSpriteRootSignature(){

	RootSignatureBuilder builder;
	builder.AddCBV(0, D3D12_SHADER_VISIBILITY_PIXEL);                // [0]: マテリアル (b0)
	builder.AddCBV(0, D3D12_SHADER_VISIBILITY_VERTEX);               // [1]: 座標 (b0)
	builder.AddDescriptorTableSRV(0, D3D12_SHADER_VISIBILITY_PIXEL); // [2]: テクスチャ (t0)
	builder.AddCBV(1, D3D12_SHADER_VISIBILITY_PIXEL);                // [3]: ライト1 (b1)
	builder.AddCBV(2, D3D12_SHADER_VISIBILITY_PIXEL);                // [4]: ライト2 (b2)
	builder.AddDefaultSampler(0);                                    // サンプラー (s0)

	// 構築して spriteRootSignature_ に入れる！
	builder.Build(dxCommon_->GetDevice(), spriteRootSignature_);
	

}
// グラフィックスパイプラインの生成 Sprite用
void PipelineManager::CreateSpriteGraphicsPipeline(){

	CreateGraphicsPipelineCommon(
		L"resources/shaders/Sprite2d.VS.hlsl",
		L"resources/shaders/Sprite2d.PS.hlsl",
		spriteRootSignature_.Get(),
		BlendMode::kNormal,         // 通常ブレンド
		D3D12_CULL_MODE_NONE,       // カリングなし
		false,                      // 深度書き込みしない
		spritePipelineState_
	);



}
// ルートシグネチャの生成 Object3D用
void PipelineManager::CreateObject3DRootSignature(){

	RootSignatureBuilder builder;
	builder.AddCBV(0, D3D12_SHADER_VISIBILITY_PIXEL);                // [0]: マテリアル (b0)
	builder.AddCBV(0, D3D12_SHADER_VISIBILITY_VERTEX);               // [1]: 座標 (b0)
	builder.AddDescriptorTableSRV(0, D3D12_SHADER_VISIBILITY_PIXEL); // [2]: テクスチャ (t0)
	builder.AddCBV(1, D3D12_SHADER_VISIBILITY_PIXEL);                // [3]: ライト1 (b1)
	builder.AddCBV(2, D3D12_SHADER_VISIBILITY_PIXEL);                // [4]: ライト2 (b2)
	builder.AddDefaultSampler(0);                                    // サンプラー (s0)

	// 構築して object3DRootSignature_ に入れる！
	builder.Build(dxCommon_->GetDevice(), object3DRootSignature_);

}
// グラフィックスパイプラインの生成 Object3D用
void PipelineManager::CreateObject3DGraphicsPipeline(){
	// カリングあり用
	CreateGraphicsPipelineCommon(
		L"resources/shaders/Object3d.VS.hlsl",
		L"resources/shaders/Object3d.PS.hlsl",
		object3DRootSignature_.Get(),
		BlendMode::kNormal,         // 通常ブレンド
		D3D12_CULL_MODE_BACK,      
		true,                      
		object3DPipelineState_
	);
	// カリングなし用
	CreateGraphicsPipelineCommon(
		L"resources/shaders/Object3d.VS.hlsl",
		L"resources/shaders/Object3d.PS.hlsl",
		object3DRootSignature_.Get(),
		BlendMode::kNormal,
		D3D12_CULL_MODE_NONE,  
		true,
		object3DPipelineStateNone_ 
	);

}
// ルートシグネチャの生成 Particle用
void PipelineManager::CreateParticleRootSignature(){

	RootSignatureBuilder builder;
	builder.AddCBV(0, D3D12_SHADER_VISIBILITY_PIXEL);                 // [0]: マテリアル (b0)

	// パーティクルはインスタンシングで描画するため、座標用のCBVは用意せず、SRVでテクスチャバッファを渡す
	builder.AddDescriptorTableSRV(1, D3D12_SHADER_VISIBILITY_VERTEX);

	builder.AddDescriptorTableSRV(0, D3D12_SHADER_VISIBILITY_PIXEL);  // [2]: テクスチャ (t0)
	builder.AddCBV(1, D3D12_SHADER_VISIBILITY_PIXEL);                 // [3]: ライト1 (b1)
	builder.AddCBV(2, D3D12_SHADER_VISIBILITY_PIXEL);                 // [4]: ライト2 (b2)
	builder.AddDefaultSampler(0);                                     // サンプラー (s0)

	// 構築して particleRootSignature_ に入れる！
	builder.Build(dxCommon_->GetDevice(), particleRootSignature_);

}
// グラフィックスパイプラインの生成 Particle用
void PipelineManager::CreateParticleGraphicsPipeline(){


	CreateGraphicsPipelineCommon(
		L"resources/shaders/Particle.VS.hlsl",
		L"resources/shaders/Particle.PS.hlsl",
		particleRootSignature_.Get(),
		BlendMode::kAdd,            // 加算ブレンド
		D3D12_CULL_MODE_NONE,       // カリングなし
		false,                      // 深度書き込みしない
		particlePipelineState_
	);



}


// グラフィックスパイプラインの共通生成
void PipelineManager::CreateGraphicsPipelineCommon(
	const std::wstring& vsPath, // 頂点シェーダーのパス
	const std::wstring& psPath, // ピクセルシェーダーのパス
	ID3D12RootSignature* rootSig, 		 // ルートシグネチャ
	BlendMode blendMode, 					// ブレンドモード
	D3D12_CULL_MODE cullMode, 				// カリングモード
	bool isDepthWrite, 					// 深度書き込みの有無
	Microsoft::WRL::ComPtr<ID3D12PipelineState>& pipelineState// 生成結果
){
	// 1. シェーダーコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob =
		dxCommon_->GetShaderCompiler().CompileShader(vsPath, L"vs_6_0");
	assert(vertexShaderBlob != nullptr);
	// ピクセルシェーダー
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob =
		dxCommon_->GetShaderCompiler().CompileShader(psPath, L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	// 2. InputLayout 
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};// 頂点レイアウト
	inputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }; // xyz座標(4要素)
	inputElementDescs[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }; // uv座標(2要素)
	inputElementDescs[2] = { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }; // 法線ベクトル(3要素)
	// InputLayout設定
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc {}; // 入力レイアウト
	inputLayoutDesc.pInputElementDescs = inputElementDescs; // 頂点レイアウト配列へのポインタ
	inputLayoutDesc.NumElements = _countof(inputElementDescs); // 配列の長さ

	// 3. BlendState (引数から取得)
	D3D12_BLEND_DESC blendDesc = GetBlendDesc(blendMode);

	// 4. RasterizerState (引数を使用)
	D3D12_RASTERIZER_DESC rasterizerDesc {}; // ラスタライザステート
	rasterizerDesc.CullMode = cullMode; // カリングモード
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID; // 塗りつぶし

	// 5. DepthStencilState (引数を使用)
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc {}; // デプスステンシルステート
	depthStencilDesc.DepthEnable = true; // 深度テスト有効
	// trueならALL(書き込む)、falseならZERO(書き込まない)
	depthStencilDesc.DepthWriteMask = isDepthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;// 深度書き込みの有無
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;// 深度比較関数
	depthStencilDesc.StencilEnable = false; // ステンシルテスト無効

	// 6. PSO生成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc {};// グラフィックスパイプラインステート設定
	psoDesc.pRootSignature = rootSig;// ルートシグネチャ
	psoDesc.InputLayout = inputLayoutDesc; // 入力レイアウト
	psoDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() }; // 頂点シェーダー
	psoDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() }; // ピクセルシェーダー
	psoDesc.BlendState = blendDesc; // ブレンドステート
	psoDesc.RasterizerState = rasterizerDesc; // ラスタライザステート
	psoDesc.NumRenderTargets = 1; // RTVの数
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // RTVのフォーマット
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // プリミティブ形状
	psoDesc.SampleDesc.Count = 1; // サンプリング数
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK; // サンプルマスク
	psoDesc.DepthStencilState = depthStencilDesc; // デプスステンシルステート
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT; // DSVのフォーマット

	// 結果を保存
	pipelineState = nullptr;
	HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)); // PSO生成
	assert(SUCCEEDED(hr));
}