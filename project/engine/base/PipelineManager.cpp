#include "PipelineManager.h"
#include "DirectXCommon.h"
#include <cassert>
#include "BlendMode.h"
#include <string> 

#include <d3d12.h>
#include <wrl.h>
#include <unordered_map>
#include "PipelineType.h"
#include "ShaderCompiler.h"

void PipelineManager::Finalize(){

	spriteRootSignature_.Reset();
	spritePipelineState_.Reset();
	object3DRootSignature_.Reset();
	object3DPipelineState_.Reset();
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
	case PipelineType::Particle:
		commandList->SetGraphicsRootSignature(particleRootSignature_.Get());
		commandList->SetPipelineState(particlePipelineState_.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		break;
    }
}
// ルートシグネチャの生成 Sprite用
void PipelineManager::CreateSpriteRootSignature(){
	//  共通関数を使って作成
	CreateRootSignatureCommon(spriteRootSignature_, false);


	

}
// グラフィックスパイプラインの生成 Sprite用
void PipelineManager::CreateSpriteGraphicsPipeline(){

	CreateGraphicsPipelineCommon(
		L"resources/shaders/Object3d.VS.hlsl",
		L"resources/shaders/Object3d.PS.hlsl",
		spriteRootSignature_.Get(),
		BlendMode::kNormal,         // 通常ブレンド
		D3D12_CULL_MODE_NONE,       // カリングなし
		false,                      // 深度書き込みしない
		spritePipelineState_
	);



}
// ルートシグネチャの生成 Object3D用
void PipelineManager::CreateObject3DRootSignature(){

	// 3Dモデルも通常通りなので false
	CreateRootSignatureCommon(object3DRootSignature_, false);


}
// グラフィックスパイプラインの生成 Object3D用
void PipelineManager::CreateObject3DGraphicsPipeline(){

	CreateGraphicsPipelineCommon(
		L"resources/shaders/Object3d.VS.hlsl",
		L"resources/shaders/Object3d.PS.hlsl",
		object3DRootSignature_.Get(),
		BlendMode::kNormal,         // 通常ブレンド
		D3D12_CULL_MODE_BACK,       // (※必要に応じてBACKに変更してください)
		false,                      // (※必要に応じてtrueに変更してください)
		object3DPipelineState_
	);



}
// ルートシグネチャの生成 Particle用
void PipelineManager::CreateParticleRootSignature(){

	// パーティクルはInstancingを使うので true
	CreateRootSignatureCommon(particleRootSignature_, true);


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

// ルートシグネチャの共通生成
void PipelineManager::CreateRootSignatureCommon(Microsoft::WRL::ComPtr<ID3D12RootSignature>& rootSignature, bool useInstancing){
	// RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature {};
	// IA入力レイアウトあり
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// テクスチャ用Range (共通)
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;// 0から始める
	descriptorRange[0].NumDescriptors = 1;// 数は1つ
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;// SRVを使う
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;// offsetを自動計算

	// RootParameter
	D3D12_ROOT_PARAMETER rootParameters[4] = {};

	// [0]: マテリアル (CBV)
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; //PixelShaderで使う
	rootParameters[0].Descriptor.ShaderRegister = 0; //レジスタ番号0とバインド

	// 分岐 
	if ( useInstancing ) {
		// パーティクル用 (Instancing / DescriptorTable)
		// staticにしてメモリを保持
		static D3D12_DESCRIPTOR_RANGE descriptorRangeInstancing[1] = {}; // インスタンシング用Range
		descriptorRangeInstancing[0].BaseShaderRegister = 1; // 0から始める
		descriptorRangeInstancing[0].NumDescriptors = 1; // 数は1つ
		descriptorRangeInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
		descriptorRangeInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // offsetを自動計算

		rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; //DescriptorTableを使う
		rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; //VertexShaderで使う
		rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeInstancing; // Tableの中身の配列を指定
		rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeInstancing); // Tableで利用する数
	} else {
		// 通常用 (CBV)
		rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
		rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; //VertexShaderで使う
		rootParameters[1].Descriptor.ShaderRegister = 0; //レジスタ番号0とバインド
	}

	// [2]: テクスチャ (DescriptorTable)
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; //DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange; // Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);// 

	// [3]: ライト (CBV)
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShadaderで使う
	rootParameters[3].Descriptor.ShaderRegister = 1; // レジスタ番号1を使う

	descriptionRootSignature.pParameters = rootParameters; //ルートバラメータ配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters); //配列の長さ

	// Sampler
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {}; // サンプラーは共通
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; //バイナリアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; //0∼1の範囲側をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP; //0∼1の範囲側をリピート
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP; //0∼1の範囲側をリピート
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; //比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX; // ありったけのMipmap
	staticSamplers[0].ShaderRegister = 0; //レジスタ番号0を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	descriptionRootSignature.pStaticSamplers = staticSamplers;// サンプラー配列へのポインタ
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers); // 配列の長さ

	// 生成
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr; // シリアライズ後のバイナリ
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;// エラー情報
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, // シリアライズ
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);// バージョン1
	// エラーならログ出力
	if ( FAILED(hr) ) {
		dxCommon_->GetLogManager().Log(reinterpret_cast< char* >( errorBlob->GetBufferPointer() )); // 
		assert(false); 
	}
	// RootSignature生成
	hr = dxCommon_->GetDevice()->CreateRootSignature(0,
		signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));
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