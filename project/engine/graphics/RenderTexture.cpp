#include "RenderTexture.h"

// --- エンジン側のファイル ---
#include "engine/base/DirectXCommon.h"
#include "engine/graphics/SrvManager.h"
#include "engine/graphics/ResourceFactory.h"

void RenderTexture::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height){

	// RTV用のリソースを生成
	resource_ = ResourceFactory::GetInstance()->CreateRenderTextureResource(width, height, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, clearColor_);

	// RTV用のディスクリプタを作成
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc {};

	// フォーマット
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	// ビューの次元
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;


	static uint32_t s_rtvIndex = 2; // メイン画面が0と1を使っているので2からスタート
	rtvHandle_ = dxCommon->GetRtvHandle(s_rtvIndex);
	s_rtvIndex++;
	// RTVを作成
	dxCommon->GetDevice()->CreateRenderTargetView(resource_.Get(), &rtvDesc, rtvHandle_);

	// SRV用のディスクリプタを作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};

	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	// SRV用のインデックスを割り当てる
	srvIndex_ = srvManager->Allocate();

	// SRVを作成
	dxCommon->GetDevice()->CreateShaderResourceView(resource_.Get(), &srvDesc, srvManager->GetCPUDescriptorHandle(srvIndex_));

}


// 描画先を RenderTexture に切り替える処理
void RenderTexture::PreDrawScene(ID3D12GraphicsCommandList* commandList, DirectXCommon* dxCommon){
	
	// 描画先をこのテクスチャに切り替える
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = resource_.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	commandList->ResourceBarrier(1, &barrier);
	
	
	
	// ハンドル取得
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon->GetDsvHandle();

	// 1. 描画先を RenderTexture と DSV に切り替え
	commandList->OMSetRenderTargets(1, &rtvHandle_, FALSE, &dsvHandle);

	// 2. クリア処理
	const float clearColor[4] = { clearColor_.x, clearColor_.y, clearColor_.z, clearColor_.w };
	commandList->ClearRenderTargetView(rtvHandle_, clearColor, 0, nullptr);
	commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// 3. ビューポートとシザー矩形の設定
	D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast< float >( dxCommon->GetClientWidth() ), static_cast< float >( dxCommon->GetClientHeight() ), 0.0f, 1.0f };
	D3D12_RECT scissorRect = { 0, 0, static_cast< LONG >( dxCommon->GetClientWidth() ), static_cast< LONG >( dxCommon->GetClientHeight() ) };
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);
}

// 描画先を Swapchain（メイン画面）に戻す処理
void RenderTexture::PostDrawScene(ID3D12GraphicsCommandList* commandList, DirectXCommon* dxCommon){
	// RenderTexture を「描画用(RT) → 読み込み用(SRV)」に戻すバリアのみ行う
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = resource_.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	commandList->ResourceBarrier(1, &barrier);


}

void RenderTexture::Resize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height){

	resource_.Reset(); // 既存のリソースを解放

	resource_ = ResourceFactory::GetInstance()->CreateRenderTextureResource(width, height, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, clearColor_);


	// RTV用のディスクリプタを作成
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc {};

	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	dxCommon->GetDevice()->CreateRenderTargetView(resource_.Get(), &rtvDesc, rtvHandle_);


	// SRV用のディスクリプタを作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};

	
	
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	
	dxCommon->GetDevice()->CreateShaderResourceView(resource_.Get(), &srvDesc, srvManager->GetCPUDescriptorHandle(srvIndex_));


}
