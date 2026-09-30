#pragma once
// --- 標準・外部ライブラリ ---
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <memory>
#include <source_location>

// ルートシグネチャを簡単に構築するための便利クラス
class RootSignatureBuilder {

public:
	// CBV（定数バッファ: b0, b1
    void AddCBV(UINT shaderRegister, D3D12_SHADER_VISIBILITY visibility);
	// SRV（テクスチャなど: t0, t1
	void AddSRV(UINT shaderRegister, D3D12_SHADER_VISIBILITY visibility);

    // SRV（テクスチャなど: t0, t1
    void AddDescriptorTableSRV(UINT baseShaderRegister, D3D12_SHADER_VISIBILITY visibility);

    // サンプラー（s0
    //   wrapV=true：縦方向もリピートする（UV が 0〜1 を超えて繰り返すタイル貼りのモデル用）。
    //   既定は縦だけクランプ＝アトラスの上下の段がにじまない
    void AddDefaultSampler(UINT shaderRegister = 0, bool wrapV = false);

    // 構築してルートシグネチャを生成（where：呼び出し元。リークレポートに出る名前になる）
    void Build(ID3D12Device* device, Microsoft::WRL::ComPtr<ID3D12RootSignature>& outRootSig,
        const std::source_location& where = std::source_location::current());

    // Computeシェーダー専用 (IAフラグなし)
    void BuildForCompute(ID3D12Device* device,
        Microsoft::WRL::ComPtr<ID3D12RootSignature>& outRootSig,
        const std::source_location& where = std::source_location::current());


    // UAVをDescriptorTableで追加 (Computeシェーダー用)
    void AddDescriptorTableUAV(UINT baseShaderRegister,
        D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);

private:
	// ルートパラメータ、ディスクリプタレンジ、サンプラーの情報を保持
    std::vector<D3D12_ROOT_PARAMETER> parameters_;
    std::vector<std::unique_ptr<D3D12_DESCRIPTOR_RANGE>> ranges_;
    std::vector<D3D12_STATIC_SAMPLER_DESC> samplers_;

};