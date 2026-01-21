#include "Framework.h"

void Framework::Initialize(){
	// ---------------------------------------------
	// 基盤システムの初期化
	// ---------------------------------------------
	// WindowProc
	windowProc_ = new WindowProc();
	WNDCLASS wc = {};
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.hbrBackground = reinterpret_cast< HBRUSH >( COLOR_WINDOW + 1 );
	wc.lpfnWndProc = windowProc_->WndProc;
	windowProc_->Initialize(wc, 1280, 720); // サイズは固定か変数化

	// DirectXCommon
	dxCommon_ = new DirectXCommon();
	dxCommon_->Initialize(windowProc_);

	// Input
	input_ = new Input();
	input_->Initialize(windowProc_->GetHwnd());

	// SrvManager
	srvManager_ = new SrvManager();
	srvManager_->Initialize(dxCommon_);
	dxCommon_->SetSrvManager(srvManager_);

	// ResourceFactory
	resourceFactory_ = new ResourceFactory();
	resourceFactory_->SetDevice(dxCommon_->GetDevice());
	dxCommon_->SetResourceFactory(resourceFactory_);

	// TextureManager
	TextureManager::GetInstance()->Initialize(dxCommon_->GetDevice(), dxCommon_, srvManager_);
	TextureManager::GetInstance()->SetResourceFactory(resourceFactory_);

	// PipelineManager
	PipelineManager::GetInstance()->Initialize(dxCommon_);

	// ImGuiManager
	imguiManager_ = new ImGuiManager();
	imguiManager_->Initialize(windowProc_, dxCommon_);

	// Audio
	AudioManager::GetInstance()->Initialize();

	// 描画共通クラス
	spriteCommon_ = new SpriteCommon();
	spriteCommon_->Initialize(dxCommon_);

	obj3dCommon_ = new Obj3dCommon();
	obj3dCommon_->Initialize(dxCommon_);

	ModelManager::GetInstance()->Initialize(dxCommon_);
	ParticleManager::GetInstance()->Initialize(dxCommon_, srvManager_);


	//const int kSubdivision = 16;
   //const int vertexCountX = kSubdivision + 1;
   //const int vertexCountY = kSubdivision + 1;

   //const int vertexNum = vertexCountX * vertexCountY;
   //const int indexNum = kSubdivision * kSubdivision * 6;

   //uint32_t latIndex;
   //uint32_t lonIndex;


   //Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSphere = resourceFactory->CreateBufferResource(sizeof(VertexData) * vertexNum);

   //// 頂点バッファビューを作成する
   //D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSphere {};
   //// リソースの先頭のアドレスから作成する
   //vertexBufferViewSphere.BufferLocation = vertexResourceSphere->GetGPUVirtualAddress();
   //// 使用するリソースのサイズは頂点6つ分のサイズ
   //vertexBufferViewSphere.SizeInBytes = sizeof(VertexData) * vertexNum;
   //// 1頂点あたりのサイズ
   //vertexBufferViewSphere.StrideInBytes = sizeof(VertexData);

   //VertexData* vertexDataSphere = nullptr;
   //// 書き込むためのアドレス取得
   //vertexResourceSphere->Map(0, nullptr, reinterpret_cast< void** >( &vertexDataSphere ));

   //// 経度分割1つ分の角度 
   //const float kLonEvery = std::numbers::pi_v<float>*2.0f / float(kSubdivision);
   //// 緯度分割1つ分の角度
   //const float kLatEvery = std::numbers::pi_v<float> / float(kSubdivision);
   //// 緯度の方向に分割
   //for ( latIndex = 0; latIndex < ( kSubdivision + 1 ); ++latIndex ) {

   //	float lat = -std::numbers::pi_v<float> / 2.0f + kLatEvery * latIndex;
   //	// 経度の方向に分割しながら線を描く
   //	for ( lonIndex = 0; lonIndex < ( kSubdivision + 1 ); ++lonIndex ) {

   //		float lon = lonIndex * kLonEvery;
   //		// 頂点データを描く
   //		//頂点A
   //		VertexData vertA = {
   //			{
   //				std::cosf(lat) * std::cosf(lon),
   //				std::sinf(lat),
   //				std::cosf(lat) * std::sinf(lon),
   //				1.0f
   //			},
   //			{
   //				float(lonIndex) / float(kSubdivision),
   //				1.0f - float(latIndex) / float(kSubdivision)
   //			},
   //			{
   //				std::cosf(lat) * std::cosf(lon),
   //				std::sinf(lat),
   //				std::cosf(lat) * std::sinf(lon),
   //			}
   //		};
   //		uint32_t start = ( latIndex * ( kSubdivision + 1 ) + lonIndex );
   //		vertexDataSphere[start] = vertA;

   //	}

   //}

   ////indexSphere用の頂点indexを作る1つ辺りのindexのサイズは32bit
   //Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSphere = CreateBufferResource(device, sizeof(uint32_t) * indexNum);

   //D3D12_INDEX_BUFFER_VIEW indexBufferViewSphere {}; // IBV
   //// リソースの先頭のアドレスから使う
   //indexBufferViewSphere.BufferLocation = indexResourceSphere->GetGPUVirtualAddress();
   ////使用するリソースのサイズ
   //indexBufferViewSphere.SizeInBytes = sizeof(uint32_t) * indexNum;

   //indexBufferViewSphere.Format = DXGI_FORMAT_R32_UINT; // indexはuint32_tとする

   //// indexリソースにデータを書き込む
   //uint32_t* indexDataSphere = nullptr;

   //indexResourceSphere->Map(0, nullptr, reinterpret_cast< void** >( &indexDataSphere ));

   //uint32_t idx = 0;
   //for ( uint32_t latIndex = 0; latIndex < kSubdivision; ++latIndex ) {
   //	for ( uint32_t lonIndex = 0; lonIndex < kSubdivision; ++lonIndex ) {
   //		// 緯度と経度のインデックスから頂点のインデックスを計算する
   //		uint32_t topLeft = latIndex * vertexCountX + lonIndex;
   //		uint32_t bottomLeft = ( latIndex + 1 ) * vertexCountX + lonIndex;
   //		uint32_t topRight = latIndex * vertexCountX + ( lonIndex + 1 );
   //		uint32_t bottomRight = ( latIndex + 1 ) * vertexCountX + ( lonIndex + 1 );

   //		// 1つめの三角形
   //		indexDataSphere[idx++] = topLeft;
   //		indexDataSphere[idx++] = bottomLeft;
   //		indexDataSphere[idx++] = topRight;

   //		// 2つめの三角形
   //		indexDataSphere[idx++] = topRight;
   //		indexDataSphere[idx++] = bottomLeft;
   //		indexDataSphere[idx++] = bottomRight;
   //	}
   //}

   //indexResourceSphere->Unmap(0, nullptr);


}

void Framework::Finalize(){
	// 各マネージャー終了
	AudioManager::GetInstance()->Finalize();
	ModelManager::GetInstance()->Finalize();
	ParticleManager::GetInstance()->Finalize();
	TextureManager::GetInstance()->Finalize();
	PipelineManager::GetInstance()->Finalize();

	// ImGui終了
	imguiManager_->Shutdown();
	delete imguiManager_;

	// 基盤システム解放
	delete obj3dCommon_;
	delete spriteCommon_;
	delete resourceFactory_;
	delete srvManager_;
	delete input_;
	delete dxCommon_;
	delete windowProc_;
}

void Framework::Update(){
	// 基盤更新
	windowProc_->Update();
	input_->Update();

	// ウィンドウが閉じられたら終了フラグを立てる
	if ( windowProc_->GetIsClosed() ) {
		endRequest_ = true;
	}
}

void Framework::Run(){
	
	// メインループ
	while ( true ) {
		// 毎フレーム更新
		Update();

		// 終了リクエストがあったらループを抜ける
		if ( IsEndRequest() ) {
			break;
		}

		// 描画
		Draw();
	}

}

bool Framework::IsEndRequest(){
	return endRequest_;
}