#include"Matrix4x4.h"
#include <Windows.h>
#include <cstdint>
#include <string>
#include <format>
#include <filesystem>
#include <fstream>// ファイルを書いたり読み込んだりするライブラリ
#include <chrono> // 時間を扱うライブラリ 
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <dxgidebug.h>
#include <dxcapi.h>
#include <numbers>
#include<sstream>
#include <wrl.h>
#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョンを指定
#include <dinput.h>
// Debug用のあれやこれを使えるようにする
#include <dbghelp.h>
#include <strsafe.h>
#include "externals/DirectXTex/d3dx12.h"
#include "externals/DirectXTex/DirectXTex.h"
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"Dbghelp.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"dxcompiler.lib")
#pragma comment(lib,"xaudio2.lib")
#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")

#pragma comment(lib, "ole32.lib")         // COMライブラリ CoInitializeEx などに必要
#pragma comment(lib, "shlwapi.lib")        // Shell API に必要
#pragma comment(lib, "windowscodecs.lib")  // WIC（テクスチャ読み込み）に必要

#include <random>


#include "DebugCamera.h"
#include "struct.h"
#include "WindowProc.h"
#include "LogManager.h"
#include "CrashDumper.h"
#include "ShaderCompiler.h"
#include "ImGuiManager.h"
#include"Input.h"
#include "DirectXCommon.h"
#include"SpriteCommon.h"
#include"Sprite.h"
#include "TextureManager.h"
#include "SrvManager.h"
#include "PipelineManager.h"
#include "Obj3dCommon.h"
#include "Obj3d.h"
#include "ModelCommon.h" 
#include "Model.h"
#include "ModelManager.h"
#include "Camera.h"
#include "ParticleManager.h"
#include "AudioManager.h"
#include "ResourceFactory.h"
#include "ResourceLeakChecker.h"
using namespace logs;
using namespace MatrixMath;


LogManager logManager;// ログマネージャーのインスタンス
Input input; // 入力クラスのインスタンス

std::random_device seedGenerator; // 乱数の種を生成するオブジェクト
std::mt19937 randomEngine(seedGenerator()); // メルセンヌ・ツイスタの乱数エンジン

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int){

	//ResourceLeakChecker leakChecker; // リソースリークチェッカーのインスタンス

#ifdef _DEBUG
	//ResourceLeakChecker leakChecker;

#endif // _DEBUG



	// COMの初期化
	CoInitializeEx(0, COINIT_MULTITHREADED);

	// 誰も補掟しなかった場合に(Unhandled),補掟する関数を登録
	// main関数は始まってすぐに登録するといい
	CrashDumper::Install();

	// --------------------
	// ウィンドウ生成
	// --------------------

	// ウィンドウのタイトル
	WindowProc windowProc;

	WNDCLASS wc = {}; // ウィンドウクラスの初期化
	wc.style = CS_HREDRAW | CS_VREDRAW; // ウィンドウのスタイル
	wc.hbrBackground = reinterpret_cast< HBRUSH >( COLOR_WINDOW + 1 ); // 背景色
	wc.lpfnWndProc = windowProc.WndProc; // ウィンドウプロシージャの関数ポインタ

	// ウィンドウプロシージャの初期化
	windowProc.Initialize(wc, windowProc.GetClientWidth(), windowProc.GetClientHeight());

	// --------------------
	// DirectX 共通
	// --------------------

	// DirectX共通初期化クラスのインスタンス
	DirectXCommon* dxCommon = new DirectXCommon();

	// DirectX共通初期化
	dxCommon->Initialize(&windowProc);

	dxCommon->GetShaderCompiler().Initialize();

	// DirectXデバイスの取得
	ID3D12Device* device = dxCommon->GetDevice();
	assert(device != nullptr && "Failed to create D3D12 Device. Check Graphics Tools.");
	// コマンドリストの取得
	ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();
	assert(commandList != nullptr);

	// --------------------
	// SRV マネージャ
	// --------------------

	// SRVマネージャーのインスタンス
	SrvManager* srvManager = new SrvManager();
	// SRVマネージャーの初期化
	srvManager->Initialize(dxCommon);

	// DirectXCommonにSRVマネージャーをセット
	dxCommon->SetSrvManager(srvManager);

	// ShaderCompilerの初期化
	//dxCommon->GetShaderCompiler().Initialize();


	// --------------------
	// ResourceFactory
	// --------------------

	ResourceFactory* resourceFactory = new ResourceFactory(); // リソースファクトリーのインスタンス

	// リソースファクトリーの初期化
	resourceFactory->SetDevice(dxCommon->GetDevice());
	// DirectXCommonにリソースファクトリーをセット
	dxCommon->SetResourceFactory(resourceFactory);

	// --------------------
	// TextureManager
	// --------------------

	// テクスチャマネージャーの初期化
	TextureManager* textureManager = TextureManager::GetInstance();
	textureManager->Initialize(
		device,
		dxCommon,
		srvManager
	);

	textureManager->SetResourceFactory(resourceFactory);

	// --------------------
	// PipelineManager
	// --------------------

	PipelineManager::GetInstance()->Initialize(dxCommon);

	// --------------------
	// Sprite / Obj3d Common
	// --------------------

	// スプライト共通初期化クラスのインスタンス
	SpriteCommon* spriteCommon = new SpriteCommon();

	// スプライト共通初期化
	spriteCommon->Initialize(dxCommon);

	// 3Dオブジェクト共通初期化クラスのインスタンス
	Obj3dCommon* obj3dCommon = new Obj3dCommon();

	// 3Dオブジェクト共通初期化
	obj3dCommon->Initialize(dxCommon);


	// モデルマネージャーの初期化
	ModelManager::GetInstance()->Initialize(dxCommon);
	// パーティクルマネージャーの初期化
	ParticleManager::GetInstance()->Initialize(dxCommon, srvManager);


	// コマンドリストの記録開始
	dxCommon->BeginCommandRecording();


	// --------------------
	// モデルデータの読み込み (アセットのロード)
	// --------------------

	// 例1：平面モデル
	ModelManager::GetInstance()->LoadModel("Ground", "resources", "fence.obj");

	ModelManager::GetInstance()->LoadModel("Player", "resources", "axis.obj");

	ParticleManager::GetInstance()->CreateParticleGroup("Circle", "resources/circle.png");


	// --------------------
	// テクスチャの読み込み
	// --------------------

	// 1枚目：uvChecker
	TextureData textrueResource = textureManager->LoadTextureAndCreateSRV("resources/uvChecker.png", commandList);
	// 2枚目：monsterBall
	TextureData textrueResource2 = textureManager->LoadTextureAndCreateSRV("resources/monsterBall.png", commandList);
	// 3枚目：fence
	TextureData textrueResource3 = textureManager->LoadTextureAndCreateSRV("resources/fence.png", commandList);
	// 4枚目：circle
	TextureData textrueResource5 = textureManager->LoadTextureAndCreateSRV("resources/circle.png", commandList);
	// コマンドリストの記録終了
	dxCommon->EndCommandRecording();

	// モンスターボールを使うかどうか
	bool useMonsterBall = false;

	bool useFence = false;

	bool useCircle = false;


	// --------------------
	// オブジェクト生成
	// --------------------

	Sprite* sprite = new Sprite();


	// --------------------
	// ImGui
	// --------------------

	// ImGui 管理クラス
	ImGuiManager imgui;
	imgui.Initialize(&windowProc, dxCommon);

	// --------------------
	// 入力
	// --------------------

	// 入力クラスの初期化
	input.Initialize(windowProc.GetHwnd());


	//ResourceLeakChecker leakCheck;



	// リソースチェック
	Microsoft::WRL::ComPtr<IDXGIDebug1> debug;

	logManager.Initialize(); // ログの初期化





	//#ifdef _DEBUG
	//
	//	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
	//	if ( SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))) ) {
	//		// デバックレイヤーを有効化する
	//		debugController->EnableDebugLayer();
	//		// さらにGPU側でもチェックを行うようにする
	//		debugController->SetEnableGPUBasedValidation(TRUE);
	//	}
	//#endif // _DEBUG





	//



	//#ifdef _DEBUG
	//
	//	Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
	//	if ( SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue))) ) {
	//		// やばいエラー時に止まる
	//		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
	//		// エラー時に止まる
	//		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
	//		// 警告時に泊まる
	//		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
	//		// 抑制するメッセージのＩＤ
	//		D3D12_MESSAGE_ID denyIds[] = {
	//			// windows11でのDXGIデバックレイヤーとDX12デバックレイヤーの相互作用バグによるエラーメッセージ
	//			// https://stackoverflow.com/questions/69805245/directx-12-application-is-crashing-in-windows-11
	//			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE };
	//		// 抑制するレベル
	//		D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
	//		D3D12_INFO_QUEUE_FILTER filter {};
	//		filter.DenyList.NumIDs = _countof(denyIds);
	//		filter.DenyList.pIDList = denyIds;
	//		filter.DenyList.NumSeverities = _countof(severities);
	//		filter.DenyList.pSeverityList = severities;
	//		// 指定したメッセージの表示wp抑制する
	//		infoQueue->PushStorageFilter(&filter);
	//	}
	//
	//#endif // _DEBUG


#pragma region CommandList



	// コマンドリストの記録開始
	dxCommon->PreDraw();
	// スプライト初期化
	sprite->Initialize(spriteCommon);

	// スプライト共通の描画前処理
	spriteCommon->PreDraw(commandList);
	// 3Dオブジェクト共通の描画前処理
	obj3dCommon->PreDraw(commandList);





	// DepthStencilTextureをウィンドウのサイズで作成
	Microsoft::WRL::ComPtr<ID3D12Resource> deptStencilResource = textureManager->CreateDepthStencilTextureResource(windowProc.GetClientWidth(), windowProc.GetClientHeight());

	// DepthStenciLstateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc {};
	// Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	// Depthの書き込みを行わない
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	// 比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	depthStencilDesc.StencilEnable = false;


	// コマンドリストのクローズと実行、バッファの入れ替えまで行う
	dxCommon->PostDraw();

#pragma endregion



#pragma region 頂点データの作成とビュー

	//// 実際に頂点リソースを作る
	//Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = CreateBufferResource(device, sizeof(VertexData) * 6);

	//// 頂点バッファビューを作成する
	//D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	//// リソースの先頭のアドレスから使う
	//vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	//// 使用するリソースのサイズは頂点3つ分
	//vertexBufferView.SizeInBytes = sizeof(VertexData) * 6;
	//// 1頂点あたりのサイズ
	//vertexBufferView.StrideInBytes = sizeof(VertexData);

	//// 頂点リソースにデータを書き込む
	//VertexData* vertexData = nullptr;
	//// 書き込むためのアドレス取得
	//vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	//// 左下
	//vertexData[0] = { -0.5f,-0.5f,0.0f,1.0f };
	//vertexData[0].texcoord = { 0.0f,1.0f };
	//// 上
	//vertexData[1] = { 0.0f,0.5f,0.0f,1.0f };
	//vertexData[1].texcoord = { 0.5f,0.0f };
	//// 右下
	//vertexData[2] = { 0.5f,-0.5f,0.0f,1.0f };
	//vertexData[2].texcoord = { 1.0f,1.0f };


	//// 左下2
	//vertexData[3].position = { -0.5f,-0.5f,0.5f,1.0f };
	//vertexData[3].texcoord = { 0.0f,1.0f };
	//// 上2
	//vertexData[4].position = { 0.0f,0.0f,0.0f,1.0f };
	//vertexData[4].texcoord = { 0.5f,0.0f };
	//// 右下2
	//vertexData[5].position = { 0.5f,-0.5f,-0.5f,1.0f };
	//vertexData[5].texcoord = { 1.0f,1.0f };


#pragma endregion

#pragma region Spriteの実装

	std::vector<Sprite*> sprites;
	// 5枚生成するループ
	for ( uint32_t i = 0; i < 5; ++i ) {
		Sprite* newSprite = new Sprite();
		newSprite->Initialize(spriteCommon);

		// テクスチャを設定
		newSprite->SetTextureHandle(srvManager->GetGPUDescriptorHandle(textrueResource.srvIndex));

		// 位置をずらす（例：横に200ずつズレる）
		Vector2 pos = { 100.0f + ( i * 200.0f ), 360.0f };
		newSprite->SetPosition(pos);

		// 必要ならサイズや色もここで設定
		newSprite->SetScale({ 100.0f, 100.0f });

		sprites.push_back(newSprite);
	}


	//Vector2 poition = sprite->GetPosition();
	//float rotation = sprite->GetRotation();
	//Vector4 color = sprite->GetColor();
	//Vector2 scale = sprite->GetScale();

	sprite->SetTextureHandle(srvManager->GetGPUDescriptorHandle(textrueResource.srvIndex));



#pragma endregion

#pragma region Sphereの実装

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
#pragma endregion



	// --------------------
	// オブジェクトの生成・配置
	// --------------------

	// カメラの生成
	Camera* camera = new Camera(windowProc.GetClientWidth(), windowProc.GetClientHeight());

	camera->SetTranslation({ 0.0f, 0.0f, -10.0f }); // 少し後ろに下げる
	// 回転はなし
	camera->SetRotation({ 0.0f,0.0f,0.0f });



	// 複数のObj3dを管理するリスト（配列）
	std::vector<Obj3d*> object3ds;

	Obj3d* groundObj = new Obj3d();
	// "Ground" で登録したモデルを取り出してセット
	Model* modelGround = ModelManager::GetInstance()->FindModel("Ground");
	groundObj->Initialize(obj3dCommon, modelGround);
	groundObj->SetCamera(camera); // カメラをセット

	Vector3 groundPos = { 0.0f, 0.0f, 0.0f };
	Vector3 groundRot = { 0.0f, 0.0f, 0.0f };
	Vector3 groundScale = { 1.0f, 1.0f, 1.0f };

	groundObj->SetTranslation(groundPos);
	groundObj->SetScale(groundScale);
	object3ds.push_back(groundObj);






	//// "Player" で登録したモデルを取り出す
	//Model* modelPlayer = ModelManager::GetInstance()->FindModel("Player");

	//Obj3d* playerObj = new Obj3d();
	//playerObj->Initialize(obj3dCommon, modelPlayer);

	//// ★追加：プレイヤーにも同じカメラをセットする！
	//playerObj->SetCamera(camera);

	//// 場所を指定 (例: 真ん中)
	//playerObj->SetTranslation({ 0.0f, 0.0f, 0.0f });
	//// こちらも明示的に 1.0f に設定（デフォルトでも1ですが念のため）
	//playerObj->SetScale({ 1.0f, 1.0f, 1.0f });
	//// リストに追加
	//object3ds.push_back(playerObj);

#pragma region indexを使った実装sphere

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



#pragma endregion






#pragma region サウンド

	// ---------------------------------------
	// オーディオの初期化
	// ---------------------------------------


	AudioManager::GetInstance()->Initialize();

	// 読み込みたいファイル名を定義（変数にしておくと便利です）
	std::string soundFile = "resources/BGM.wav";

	// 読み込み実行
	AudioManager::GetInstance()->LoadWave(soundFile);

#pragma endregion



	while ( !windowProc.GetIsClosed() ){

		// ウィンドウ
		windowProc.Update();

		// メインループ内
		input.Update(); // ← 追加：Inputの更新処理

		// カメラの更新
		camera->Update();

		// キーボード情報の取得開始

		if ( input.Triggerkey(DIK_SPACE) ){
			// クラス経由で再生（引数はファイル名）
			AudioManager::GetInstance()->PlayWave(soundFile);
		}

		// Pキーが押されたらパーティクルを発生させる
		if ( input.Triggerkey(DIK_P) ) {
			// グループ名, 発生位置, 発生数
			ParticleManager::GetInstance()->Emit("Circle", { 0.0f, 0.0f, 0.0f }, 10);
		}
		// パーティクルの更新
		ParticleManager::GetInstance()->Update(camera);

		//-------------------------------
		// model描画
		//------------------------------- 

		// ★リストに入っている全てのオブジェクトを更新
		for ( Obj3d* obj : object3ds ) {
			obj->Update();
		}


		//-------------------------------
		// Sprite
		//-------------------------------

		for ( Sprite* sprite : sprites ) {
			// もしアニメーションさせたいならここで値を変更
			// Vector2 pos = sprite->GetPosition();
			// pos.x += 1.0f;
			// sprite->SetPosition(pos);

			sprite->Update();
		}

		/*sprite->SetPosition(poition);
		sprite->SetRotation(rotation);
		sprite->SetColor(color);
		sprite->SetScale(scale);*/


		//-------------------------------
		//ImGui
		//-------------------------------
#ifdef USE_IMGUI



		// ImGuiの開始処理
		imgui.Begin();

		// 開発用UIの処理。実際に開発用のUIを出す場合はここをゲーム固有の処理に置き換える
		ImGui::ShowDemoWindow();

		ImGui::Begin("Fence Control"); // ウィンドウの開始

		// 値を操作するスライダー類
		ImGui::DragFloat3("Position", &groundPos.x, 0.1f); // 位置 (感度0.1)
		ImGui::DragFloat3("Rotation", &groundRot.x, 0.01f); // 回転 (感度0.01)
		ImGui::DragFloat3("Scale", &groundScale.x, 0.01f); // スケール (感度0.01)

		ImGui::End(); // ウィンドウの終了

		groundObj->SetTranslation(groundPos);
		groundObj->SetRotation(groundRot); // ※SetRotationがObj3dにある前提
		groundObj->SetScale(groundScale);


		

#endif // USE_IMGUI


		//directionalLightData->direction = Normalize(directionalLightData->direction);

		//Matrix4x4 uvTransformMatrix = MakeScale(uvTransformSprite.scale);
		//uvTransformMatrix = Multiply(uvTransformMatrix, MakeRotateZ(uvTransformSprite.rotate.z));
		//uvTransformMatrix = Multiply(uvTransformMatrix, MakeTranslate(uvTransformSprite.translate));
		//materialDataSprite->uvTransfrom = uvTransformMatrix;


		// 画面クリアなど描画前処理
		dxCommon->PreDraw();

		srvManager->PreDraw();


		obj3dCommon->PreDraw(commandList);

		//-------------------------------
		//// Sphereの描画
		//-------------------------------

//		commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSphere); // SphereVBVを設定


		//// 形状の種類を設定（ここでは三角形リスト）
		//commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		//// インデックスバッファのビューを設定
		//commandList->IASetIndexBuffer(&indexBufferViewSphere);

		//// ルートパラメータ 0：マテリアル（定数バッファ）
		//commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());

		//// ルートパラメータ 1：WVP行列（定数バッファ）
		//commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());

		//// ルートパラメータ 2：テクスチャ（SRV）
		//// チェックボックスの状態によって使うテクスチャを切り替える
		//commandList->SetGraphicsRootDescriptorTable(
		//	2,
		//	useMonsterBall ? textureSrvHandleGPU : textureSrvHandleGPU2
		//);

		//// ルートパラメータ 3：ディレクショナルライト（定数バッファ）
		//commandList->SetGraphicsRootConstantBufferView(3, directionalResourceLight->GetGPUVirtualAddress());

		//// 描画コマンドの発行（Draw Call）
		//// 1インスタンスあたりのインデックス数：sphereVertexNum
		//commandList->DrawIndexedInstanced(indexNum, 1, 0, 0, 0);



		//// 全てのスプライトを描画
		//for ( Sprite* sprite : sprites ) {
		//	sprite->Draw();
		//}

	// オブジェクトを描画
		for ( Obj3d* obj : object3ds ) {
			obj->Draw();
		}

		// パーティクル用パイプラインに切り替え
		PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Particle);
		// 2. 描画コマンド発行
		ParticleManager::GetInstance()->Draw(commandList);


#ifdef USE_IMGUI
		// ⑤ ImGui end → 描画コマンドを積む
		imgui.End(commandList);


#endif // USE_IMGUI

		// DirectX 描画後処理（Present / フェンス）
		dxCommon->PostDraw();

		if ( input.Triggerkey(DIK_ESCAPE) ) {
			break;
		}
	}

	// imguiの終了処理
	imgui.Shutdown();


#pragma region オブジェクトを解放


	// オーディオの終了処理
	AudioManager::GetInstance()->Finalize();
	ModelManager::GetInstance()->Finalize();
	ParticleManager::GetInstance()->Finalize();
	TextureManager::GetInstance()->Finalize();
	PipelineManager::GetInstance()->Finalize();

	CloseWindow(windowProc.GetHwnd());

	//出力ウィンドウへの文字出力
	logManager.Log("HelloWored\n");
	logManager.Log(logManager.ConvertString(std::format(L"WSTRING{}\n", windowProc.GetClientWidth())));

	logManager.Finalize();

	
	delete camera;
	// ポインタの解放
	for ( auto obj : object3ds ){
		delete obj;
	}
	for ( auto sprite : sprites ){
		delete sprite;
	}

	textrueResource.resource.Reset();
	textrueResource2.resource.Reset();
	textrueResource3.resource.Reset();
	textrueResource5.resource.Reset();
	deptStencilResource.Reset();


	delete sprite;
	delete obj3dCommon;
	delete spriteCommon;
	delete resourceFactory;
	delete srvManager;
	

#pragma endregion


	delete dxCommon;


	// COMの終了処理
	CoUninitialize();

	return 0;

}