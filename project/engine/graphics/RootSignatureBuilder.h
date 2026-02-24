#pragma once
// --- 標準・外部ライブラリ ---
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <memory>

// ルートシグネチャを簡単に構築するための便利クラス
class RootSignatureBuilder {

public:
	// CBV（定数バッファ: b0, b1
    void AddCBV(UINT shaderRegister, D3D12_SHADER_VISIBILITY visibility);

    // SRV（テクスチャなど: t0, t1
    void AddDescriptorTableSRV(UINT baseShaderRegister, D3D12_SHADER_VISIBILITY visibility);

    // サンプラー（s0
    void AddDefaultSampler(UINT shaderRegister = 0);

    // 構築してルートシグネチャを生成
    void Build(ID3D12Device* device, Microsoft::WRL::ComPtr<ID3D12RootSignature>& outRootSig);
private:
	// ルートパラメータ、ディスクリプタレンジ、サンプラーの情報を保持
    std::vector<D3D12_ROOT_PARAMETER> parameters_;
    std::vector<std::unique_ptr<D3D12_DESCRIPTOR_RANGE>> ranges_;
    std::vector<D3D12_STATIC_SAMPLER_DESC> samplers_;

};