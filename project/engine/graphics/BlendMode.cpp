#include "BlendMode.h"

D3D12_BLEND_DESC GetBlendDesc(BlendMode blendMode){
	// 共通設定（デフォルトは全要素書き込み、ブレンド有効）
	D3D12_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;

	// α値自体のブレンド設定（基本は共通でOK）
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

	switch ( blendMode ) {
	case BlendMode::kNone:
		// ブレンド無効
		blendDesc.RenderTarget[0].BlendEnable = FALSE;
		break;

	case BlendMode::kNormal:
		// 通常αブレンド: Src * SrcA + Dest * (1 - SrcA)
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		break;

	case BlendMode::kAdd:
		// 加算合成: Src * SrcA + Dest * 1
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		break;

	case BlendMode::kSubtract:
		// 減算合成: Dest * 1 - Src * SrcA
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT; // 逆減算
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		break;

	case BlendMode::kMultiply:
		// 乗算合成: Src * 0 + Dest * SrcColor
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ZERO;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_SRC_COLOR;
		break;

	case BlendMode::kScreen:
		// スクリーン合成: Src * (1 - Dest) + Dest * 1
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		break;
	}

	return blendDesc;
}
