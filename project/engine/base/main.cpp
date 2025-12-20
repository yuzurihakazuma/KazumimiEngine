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
#include <xaudio2.h>
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


using namespace logs;
using namespace MatrixMath;



//struct VertexData{
//	Vector4 position;
//	Vector2 texcoord;
//	Vector3 normal;
//};
//
//
//
//struct Material{
//	Vector4 color;
//	int32_t enableLighting;
//	float padding[3];
//	Matrix4x4 uvTransfrom; // UV変換行列
//};
//
//struct TransformationMatrix{
//	Matrix4x4 WVP;
//	Matrix4x4 World;
//};


struct ParticleForGPU{
	Matrix4x4 WVP;
	Matrix4x4 World;
	Vector4 color;
};

struct DirectionalLight{
	Vector4 color; // ライトの色
	Vector3 direction; // ライトの向き
	float intensity; // 輝度

};
//
//struct MaterialData{
//	std::string textrueFilePath; // テクスチャファイルのパス
//
//};
//
//struct ModelData{
//	std::vector<VertexData> vertices; // 頂点データ
//	std::vector<uint32_t> indices;
//	MaterialData material;
//};
// チャンクヘッダ
struct ChunkHeader{
	char id[4]; // チャンクID
	int32_t size; // チャンクのサイズ
};
// RIFFヘッダチャンク
struct RiffHeader{
	ChunkHeader chunk; // RIFF
	char type[4]; // WAVE
};
// FMTチャンク
struct ForamatChunk{
	ChunkHeader chunk; // FMT
	WAVEFORMATEX fmt;  // 波形フォーマット

};
// 音声データ
struct SoundData{
	// 波型フォーマット
	WAVEFORMATEX wfex;
	// バッファの先頭アドレス
	BYTE* pBuffer;
	// バッファのサイズ
	unsigned int bufferSize;

};
// ブレンドモード
enum BlendMods{
	// ブレンドなし
	kBlendModeNone,
	// 通常aブレンド:デフォルト Src * SrcA +Dest * (1-SrcA)
	kBlendModeNoraml,
	// 加算 Src *SrcA + Dest * 1
	kBlendModeAdd,
	// 減算 Dest* 1 - Src * SrcA
	kBlendModSubtract,
	// 乗算 Src * 0 + Dest * Src
	kBlendModeMultily,
	// スクリーン Src * (1 - Dest) + Dest * 1
	kBlendModeScreen,
	// 利用してはいけない
	kCountOfBlendMode,

};


// Transform変数を作る
Transform transform { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
Transform cameraTransfrom { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,-10.0f} };
Transform transformSprite { {1.0f,1.0f,1.0f,},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
Transform uvTransformSprite { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

LogManager logManager;// ログマネージャーのインスタンス
Input input; // 入力クラスのインスタンス



// パーティクル
struct Particle{
	Transform transform; // パーティクルの座標変換情報
	Vector3 velocity; // パーティクルの速度
	Vector4 color; // パーティクルの色
	float lifeTime; // パーティクルの寿命
	float currentTime; // パーティクルの現在の生存時間
};

std::random_device seedGenerator; // 乱数の種を生成するオブジェクト
std::mt19937 randomEngine(seedGenerator()); // メルセンヌ・ツイスタの乱数エンジン





#pragma region リソースリークチェック

struct D3DResourceLeakChecker{
	~D3DResourceLeakChecker(){
		Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
		if ( SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug))) ) {
			debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
		}
	}


};

#pragma endregion






#pragma region MatrialData構造体と読み込み関数

//MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename){
//	MaterialData materialData; // MaterialDataを構築
//	std::string line; // ファイルから読んだ1行を格納するもの
//	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
//	assert(file.is_open()); // 開けなかったら止める
//	while ( std::getline(file, line) ){
//		std::string identifier; // 行の先頭の識別子を格納する
//		std::istringstream s(line); // 先頭の認別子を読む
//		s >> identifier; // 先頭の識別子を読み込む
//
//		// identifierに応じた処理
//		if ( identifier == "map_Kd" ){
//			std::string textureFilename; // テクスチャファイル名を格納する変数
//			s >> textureFilename; // テクスチャファイル名を読み込む
//			// 連結してファイルパスにする
//			materialData.textrueFilePath = directoryPath + "/" + textureFilename;
//		}
//
//	}
//	return materialData; // 読み込んだMaterialDataを返す
//
//}



#pragma endregion



#pragma region OBJファイルを読み込む関数

//ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename){
//
//
//	ModelData modelData; // 構造するModelData
//	std::vector<Vector4> positions; // 位置
//	std::vector<Vector3> normals; // 法線
//	std::vector<Vector2> texcoords; // テクスチャ座標
//	std::string line; // ファイルから読んだ1行を格納するもの
//
//	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
//
//	assert(file.is_open()); // 開けなかったら止める
//
//	while ( std::getline(file, line) ) {
//
//		std::string identifier; // 行の先頭の識別子を格納する
//		std::istringstream s(line); // 先頭の認別子を読む
//		s >> identifier; // 先頭の識別子を読み込む
//		if ( identifier == "v" ) {
//			Vector4 position; // 位置を格納する変数
//			s >> position.x >> position.y >> position.z; // 位置を読み込む
//			// DirectX座標系に合わせて反転
//			//position.x *= -1.0f;
//			position.x *= 1.0f;
//			position.w = 1.0f;
//			positions.push_back(position); // 位置を格納する
//		} else if ( identifier == "vt" ) {
//			Vector2 texcoord; // テクスチャ座標を格納する変数
//			s >> texcoord.x >> texcoord.y; // テクスチャ座標を読み込む
//			texcoord.y = 1.0f - texcoord.y; // Y座標を反転する（OpenGLとDirectXの違い）
//			texcoords.push_back(texcoord); // テクスチャ座標を格納する
//
//		} else if ( identifier == "vn" ) {
//			Vector3 normal; // 法線を格納する変数
//			s >> normal.x >> normal.y >> normal.z; // 法線を読み込む
//			normal.x *= 1.0f;
//			//normal.x *= -1.0f;
//			normals.push_back(normal); // 法線を格納する
//		} else if ( identifier == "f" ) {
//			VertexData triangle[3]; // 三角形の頂点データを格納する配列
//			for ( int32_t faceVertex = 0; faceVertex < 3; ++faceVertex ) {
//				std::string vertexDefinition;
//				s >> vertexDefinition;
//
//				std::istringstream v(vertexDefinition);
//				uint32_t elementIndices[3] = {};
//				for ( int32_t element = 0; element < 3; ++element ) {
//					std::string index;
//					std::getline(v, index, '/');
//					elementIndices[element] = std::stoi(index);
//				}
//				triangle[faceVertex] = {
//					positions[elementIndices[0] - 1], // 位置は1オリジンなので-1する
//					texcoords[elementIndices[1] - 1], // テクスチャ座標も1オリジンなので-1する
//					normals[elementIndices[2] - 1] // 法線も1オリジンなので-1する
//				};
//
//
//
//
//			}
//
//			modelData.vertices.push_back(triangle[2]);
//			modelData.vertices.push_back(triangle[1]);
//			modelData.vertices.push_back(triangle[0]);
//			modelData.indices.push_back(static_cast< uint32_t >( modelData.vertices.size() - 3 ));
//			modelData.indices.push_back(static_cast< uint32_t >( modelData.vertices.size() - 2 ));
//			modelData.indices.push_back(static_cast< uint32_t >( modelData.vertices.size() - 1 ));
//		} else if ( identifier == "mtllib" ) {
//			// マテリアルの指定
//			std::string materialFilename; // マテリアル名を格納する変数
//			s >> materialFilename; // マテリアル名を読み込む
//			// マテリアルデータを読み込む
//			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
//		}
//	}
//	return modelData;
//}



#pragma endregion

#pragma region 音声データ関数

SoundData SoundLoadWave(const char* filename){
	std::ifstream file(filename, std::ios::binary);
	assert(file.is_open());

	// RIFFヘッダー確認
	RiffHeader riff;
	file.read(( char* ) &riff, sizeof(riff));
	assert(strncmp(riff.chunk.id, "RIFF", 4) == 0);
	assert(strncmp(riff.type, "WAVE", 4) == 0);

	WAVEFORMATEX wfex = {};
	BYTE* pBuffer = nullptr;
	uint32_t bufferSize = 0;

	// チャンク走査
	ChunkHeader chunk;
	while ( file.read(( char* ) &chunk, sizeof(chunk)) ) {
		if ( strncmp(chunk.id, "fmt ", 4) == 0 ) {
			assert(chunk.size <= sizeof(WAVEFORMATEX));
			file.read(( char* ) &wfex, chunk.size);
		} else if ( strncmp(chunk.id, "data", 4) == 0 ) {
			pBuffer = new BYTE[chunk.size];
			file.read(( char* ) pBuffer, chunk.size);
			bufferSize = chunk.size;
		} else {
			// JUNK、LISTなどはスキップ
			file.seekg(chunk.size, std::ios_base::cur);
		}

		// 目的の両方を読み込んだら抜ける
		if ( wfex.cbSize != 0 && pBuffer != nullptr ) {
			break;
		}
	}

	file.close();

	SoundData soundData = {};
	soundData.wfex = wfex;
	soundData.pBuffer = pBuffer;
	soundData.bufferSize = bufferSize;
	return soundData;
}

void SoundUnload(SoundData* soundData){
	if ( soundData->pBuffer ) {
		delete[] soundData->pBuffer;
		soundData->pBuffer = nullptr;
	}
	soundData->bufferSize = 0;
	soundData->wfex = {};
}
// 音声再生関数
void SoundPlayWave(IXAudio2* xAudio2, const SoundData& soundData){
	// XAudio2 が無効（初期化失敗など）なら再生せずリターン
	if ( !xAudio2 ) {
		OutputDebugStringA("[エラー] SoundPlayWave: xAudio2 が null です。\n");
		return;
	}

	HRESULT result;

	// SourceVoice（個別の音声再生チャンネル）を生成
	// soundData.wfex は再生する音声のフォーマット情報
	IXAudio2SourceVoice* pSourceVoice = nullptr;
	result = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
	if ( FAILED(result) ) {
		OutputDebugStringA("[エラー] CreateSourceVoice に失敗\n");
		return;
	}

	// 再生用のバッファ構造体を用意
	XAUDIO2_BUFFER buf = {};
	buf.pAudioData = soundData.pBuffer;     // 音声データの先頭ポインタ
	buf.AudioBytes = soundData.bufferSize;  // 音声データのサイズ（バイト単位）
	buf.Flags = XAUDIO2_END_OF_STREAM;      // 再生終了を通知するフラグ

	// SourceVoice にバッファを登録（実際の音声データを送る）
	result = pSourceVoice->SubmitSourceBuffer(&buf);

	// 音声再生開始
	result = pSourceVoice->Start();
}




#pragma endregion

#pragma region Particle関数
//// パーティクル生成関数
//Particle MakeNewParticle(std::mt19937& randomEngine){
//	std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
//	std::uniform_real_distribution<float> distTime(1.0f, 3.0f);
//	Particle particle;
//	particle.transform.scale = { 1.0f,1.0f,1.0f };
//	particle.transform.rotate = { 0.0f,0.0f,0.0f };
//	particle.transform.translate = { distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) };
//	particle.velocity = { distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) };
//	particle.color = { distribution(randomEngine) , distribution(randomEngine) , distribution(randomEngine),1.0f };
//	particle.lifeTime = distTime(randomEngine);
//	particle.currentTime = 0;
//
//	return particle;
//
//}

#pragma endregion



// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int){

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
	
	// --------------------
	// オブジェクト生成
	// --------------------

	// 3Dオブジェクトクラスのインスタンス
	Obj3d* obj3d = new Obj3d();

	Sprite* sprite = new Sprite();
	

	// --------------------
    // ImGui
    // --------------------
	
	// ImGui 管理クラス
	ImGuiManager imgui;
	imgui.Initialize(
		windowProc.GetHwnd(),
		dxCommon->GetDevice(),
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		2,                              // Frame count
		dxCommon->GetSrvHeap()       // SRV ヒープ
	);

	// --------------------
	// 入力
	// --------------------

	// 入力クラスの初期化
	input.Initialize(windowProc.GetHwnd());



	//dxCommon->EndCommandRecording();


	D3DResourceLeakChecker leakCheck;

	

	// リソースチェック
	Microsoft::WRL::ComPtr<IDXGIDebug1> debug;

	logManager.Initialize(); // ログの初期化




	




	// ShaderCompilerのインスタンス
	ShaderCompiler shaderCompiler;
	shaderCompiler.Initialize(); // いまは何もしない（将来の拡張用）



	//
	//
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


#pragma region Particles

	//// パーティクルの数を定義
	//const uint32_t kNumMaxInstance = 10;
	//// Instancing用のTransformationMatrixリソースを作る
	//Microsoft::WRL::ComPtr<ID3D12Resource> instancingRessource = resourceFactory->CreateBufferResource(sizeof(ParticleForGPU) * kNumMaxInstance);
	//// 書き込むためのアドレスを取得
	//ParticleForGPU* instancingData = nullptr;
	//instancingRessource->Map(0, nullptr, reinterpret_cast< void** >( &instancingData ));

	//Particle particles[kNumMaxInstance];
	//std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);

	//// 単位行列を書き込んでおく
	//for ( uint32_t index = 0; index < kNumMaxInstance; ++index ){


	//	particles[index] = MakeNewParticle(randomEngine);
	//	instancingData[index].WVP = MakeIdentity4x4();
	//	instancingData[index].World = MakeIdentity4x4();
	//	instancingData[index].color = particles[index].color;

	//}

#pragma endregion

#pragma region CommandList


	// コマンドリストの記録開始
	dxCommon->PreDraw();

	// スプライト共通の描画前処理
	spriteCommon->PreDraw(commandList);
	// 3Dオブジェクト共通の描画前処理
	obj3dCommon->PreDraw(commandList);

	// 3Dオブジェクト初期化
	obj3d->Initialize(obj3dCommon);
	// スプライト初期化
	sprite->Initialize(spriteCommon);


	// 1枚目：uvChecker
	TextureData textrueResource = textureManager->LoadTextureAndCreateSRV("resources/uvChecker.png", commandList);

	// 2枚目：monsterBall
	TextureData textrueResource2 = textureManager->LoadTextureAndCreateSRV("resources/monsterBall.png", commandList);

	// モンスターボールを使うかどうか
	bool useMonsterBall = false;

	// 3枚目：fence
	TextureData textrueResource3 = textureManager->LoadTextureAndCreateSRV("resources/fence.png", commandList);

	bool useFence = false;

	// 4枚目：circle
	TextureData textrueResource5 = textureManager->LoadTextureAndCreateSRV("resources/circle.png", commandList);

	bool useCircle = false;

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



#pragma region PSO

	//// RootSignature作成
	//D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature {};
	//descriptionRootSignature.Flags =
	//	D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	//// DescriptorRange
	//D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	//descriptorRange[0].BaseShaderRegister = 0; //0から始める
	//descriptorRange[0].NumDescriptors = 1; // 数は1つ
	//descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	//descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // offsetを自動計算



	//// PootParameter作成。複数設定できるので配列。
	//D3D12_ROOT_PARAMETER rootParameters[4] = {};
	//rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	//rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; //PixelShaderで使う 
	//rootParameters[0].Descriptor.ShaderRegister = 0; //レジスタ番号0とバインド 
	//rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	//rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; //VertexShaderで使う 
	//rootParameters[1].Descriptor.ShaderRegister = 0; //レジスタ番号0とバインド 
	//rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; //DescriptorTableを使う
	//rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	//rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange; // Tableの中身の配列を指定
	//rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange); // Tableで利用する数
	//rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	//rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShadaderで使う
	//rootParameters[3].Descriptor.ShaderRegister = 1; // レジスタ番号1を使う
	//descriptionRootSignature.pParameters = rootParameters; //ルートバラメータ配列へのポインタ
	//descriptionRootSignature.NumParameters = _countof(rootParameters); //配列の長さ

	//// Sampler
	//D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	//staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; //バイナリアフィルタ
	//staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; //0∼1の範囲側をリピート 
	//staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	//staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	//staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; //比較しない
	//staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX; // ありったけのMipmap
	//staticSamplers[0].ShaderRegister = 0; //レジスタ番号0を使う
	//staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	//descriptionRootSignature.pStaticSamplers = staticSamplers;
	//descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);




	//// シリアライズしてバイナリにする
	//Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	//Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	//HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature,
	//	D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	//if ( FAILED(hr) ) {
	//	logManager.Log(reinterpret_cast< char* >( errorBlob->GetBufferPointer() ));
	//	assert(false);

	//}

	//// バイナリを元に生成
	//Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	//hr = device->CreateRootSignature(0,
	//	signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
	//	IID_PPV_ARGS(&rootSignature));
	//assert(SUCCEEDED(hr));

	//// InputLayout
	//D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	//inputElementDescs[0].SemanticName = "POSITION";
	//inputElementDescs[0].SemanticIndex = 0;
	//inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	//inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	//inputElementDescs[1].SemanticName = "TEXCOORD";
	//inputElementDescs[1].SemanticIndex = 0;
	//inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	//inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	//inputElementDescs[2].SemanticName = "NORMAL";
	//inputElementDescs[2].SemanticIndex = 0;
	//inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	//inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	//D3D12_INPUT_LAYOUT_DESC inputLayoutDesc {};
	//inputLayoutDesc.pInputElementDescs = inputElementDescs;
	//inputLayoutDesc.NumElements = _countof(inputElementDescs);

	//// BlendStateの設定
	//D3D12_BLEND_DESC blendDesc {};
	//// 全ての色要素を書き込む
	//blendDesc.RenderTarget[0].RenderTargetWriteMask =
	//	D3D12_COLOR_WRITE_ENABLE_ALL;

	//blendDesc.RenderTarget[0].BlendEnable = TRUE;
	//// NormalBlend 通常ブレンド
	//blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	//blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	//blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;

	//// AddBlend 加算合成
	////blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	////blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	////blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;

	//// SubtractBlend 逆減算合成
	////blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	////blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
	////blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;

	//// MultiplyBlend 乗算合成
	////blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ZERO;
	////blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	////blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_SRC_COLOR;

	//// ScreenBlend スクリーン合成
	////blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
	////blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	////blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;


	//blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	//blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	//blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;




	//// RasterizerStateの設定
	//D3D12_RASTERIZER_DESC rasterizerDesc {};
	//// 裏面(時計周り)を表示しない
	//rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	//// 三角形の中を塗りつぶす
	//rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	//// Shaderをコンパイルする
	//Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob =
	//	shaderCompiler.CompileShader(L"resources/shaders/Object3D.VS.hlsl", L"vs_6_0");
	//assert(vertexShaderBlob != nullptr);

	//Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob =
	//	shaderCompiler.CompileShader(L"resources/shaders/Object3D.PS.hlsl", L"ps_6_0");
	//assert(pixelShaderBlob != nullptr);

	//// PSOを生成する
	//D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc {};
	//graphicsPipelineStateDesc.pRootSignature = rootSignature.Get(); // RootSignatrue
	//graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;  // InputLayout
	//graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
	//vertexShaderBlob->GetBufferSize() }; // VertexShader
	//graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
	//pixelShaderBlob->GetBufferSize() }; // PixelShader 
	//graphicsPipelineStateDesc.BlendState = blendDesc;// BlensState
	//graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;// RasterizerState
	//// 書き込むRTVの情報
	//graphicsPipelineStateDesc.NumRenderTargets = 1;
	//graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	//// 利用するトポロジ(形状)のタイプ。三角形
	//graphicsPipelineStateDesc.PrimitiveTopologyType =
	//	D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	//// どのように画面に色を打ち込むかの設定(気にしなくて良い)
	//graphicsPipelineStateDesc.SampleDesc.Count = 1;
	//graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	//// DepthStencilの設定
	//graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	//graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;


	//// 実際に生成
	//Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPinelineState = nullptr;
	//hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc,
	//	IID_PPV_ARGS(&graphicsPinelineState));
	//assert(SUCCEEDED(hr));


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

#pragma region ModelDataを使った実装

	
	//// モデルを読み込む
	//ModelData modelData = LoadObjFile("resources", "plane.obj");
	//// 1. すべての頂点の合計を求める
	//Vector3 center = { 0.0f, 0.0f, 0.0f };
	//for ( const auto& v : modelData.vertices ) {
	//	center.x += v.position.x;
	//	center.y += v.position.y;
	//	center.z += v.position.z;
	//}

	//// 2. 頂点数で割って中心座標を求める
	//size_t vertexCount = modelData.vertices.size();
	//if ( vertexCount > 0 ) {
	//	center.x /= vertexCount;
	//	center.y /= vertexCount;
	//	center.z /= vertexCount;
	//}

	//// 3. すべての頂点座標から中心座標を引く
	//for ( auto& v : modelData.vertices ) {
	//	v.position.x -= center.x;
	//	v.position.y -= center.y;
	//	v.position.z -= center.z;
	//}

	//// 頂点リソースを作る
	//Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = resourceFactory->CreateBufferResource(sizeof(VertexData) * modelData.vertices.size());

	//// 頂点バッファビューを作成する
	//D3D12_VERTEX_BUFFER_VIEW vertexBufferView {};
	//vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	//vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
	//vertexBufferView.StrideInBytes = sizeof(VertexData);

	//// 頂点リソースにデータを書き込む
	//VertexData* vertexData = nullptr;
	//vertexResource->Map(0, nullptr, reinterpret_cast< void** >( &vertexData ));
	//std::copy(modelData.vertices.begin(), modelData.vertices.end(), vertexData);

	//// インデックスリソースも同様に
	//Microsoft::WRL::ComPtr<ID3D12Resource> indexResource = resourceFactory->CreateBufferResource(sizeof(uint32_t) * modelData.indices.size());
	//D3D12_INDEX_BUFFER_VIEW indexBufferView {};
	//indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
	//indexBufferView.SizeInBytes = UINT(sizeof(uint32_t) * modelData.indices.size());
	//indexBufferView.Format = DXGI_FORMAT_R32_UINT;

	//uint32_t* indexData = nullptr;
	//indexResource->Map(0, nullptr, reinterpret_cast< void** >( &indexData ));
	//std::copy(modelData.indices.begin(), modelData.indices.end(), indexData);
	//indexResource->Unmap(0, nullptr);
#pragma endregion

#pragma region Material用

	//// マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	//Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = resourceFactory->CreateBufferResource(sizeof(Material));
	//// マテリアルにデータを書き込む
	//Material* materialData = nullptr;
	//// 書き込むためのアドレスを取得
	//materialResource->Map(0, nullptr, reinterpret_cast< void** >( &materialData ));
	//// 今回は赤を書き込んでいる
	//materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	//materialData->enableLighting = true;
	//materialData->uvTransfrom = MakeIdentity4x4();


#pragma endregion

#pragma region 平行光源

	//Microsoft::WRL::ComPtr<ID3D12Resource> directionalResourceLight = resourceFactory->CreateBufferResource(sizeof(DirectionalLight));

	//DirectionalLight* directionalLightData = nullptr;

	//directionalResourceLight->Map(0, nullptr, reinterpret_cast< void** >( &directionalLightData ));

	//// デフォルト値はとりあえず以下のようにしておく
	//directionalLightData->color = { 1.0f,1.0f,1.0f,1.0f };
	//directionalLightData->direction = { 0.0f, -1.0f,0.0f };
	//directionalLightData->intensity = 1.0f;



#pragma endregion

#pragma region WVP


	//// WVB用のリソースを作る。Matrix4x4 一つ分のサイズを用意する
	//Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource = resourceFactory->CreateBufferResource(sizeof(TransformationMatrix));

	//// データを書き込む
	//TransformationMatrix* transformationMatrixData = nullptr;

	//// 書き込むためのアドレスを取得
	//wvpResource->Map(0, nullptr, reinterpret_cast< void** >( &transformationMatrixData ));

	//// 単位行列を書き込んでおく
	//transformationMatrixData->WVP = MakeIdentity4x4();


#pragma endregion




#pragma region サウンド

	Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
	IXAudio2MasteringVoice* masterVoice = nullptr;
	// XAudioエンジンのinstanceを生成
	HRESULT result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	// マスターボイスをを生成
	result = xAudio2->CreateMasteringVoice(&masterVoice);

	// 音声読み込み
	SoundData soundData1 = SoundLoadWave("resources/BGM.wav");
	if ( FAILED(result) ) {
		OutputDebugStringA("[エラー] XAudio2 の初期化に失敗\n");
		xAudio2 = nullptr;
		// ここで return またはフラグ立てしてもOK
	}

#pragma endregion



	while ( !windowProc.GetIsClosed() ){

		// ウィンドウ
		windowProc.Update();

		// メインループ内
		input.Update(); // ← 追加：Inputの更新処理


		// キーボード情報の取得開始

		// スペースキーが押されたら音を鳴らす
		if ( input.Triggerkey(DIK_SPACE) ){
			SoundPlayWave(xAudio2.Get(), soundData1);

		}

		//-------------------------------
		// model描画
		//------------------------------- 

		//plame
		//transform.rotate.y += 0.03f;
		//Matrix4x4 worldMatrix = MakeAffine(transform.scale, transform.rotate, transform.translate);
		//Matrix4x4 cameraMatrix = MakeAffine(cameraTransfrom.scale, cameraTransfrom.rotate, cameraTransfrom.translate);
		//Matrix4x4 viewMatrix = Inverse(cameraMatrix); // ← 通常カメラの行列

		//Matrix4x4 projectionMatrix = PerspectiveFov(1.0f, float(windowProc.GetClientWidth()) / float(windowProc.GetClientHeight()), 0.1f, 100.0f);
		//Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
		//transformationMatrixData->World = worldMatrix;
		//transformationMatrixData->WVP = worldViewProjectionMatrix;

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
		// ImGuiの開始処理
		imgui.Begin();

		// 開発用UIの処理。実際に開発用のUIを出す場合はここをゲーム固有の処理に置き換える
		ImGui::ShowDemoWindow();


		////ImGui::ColorEdit4("Color", &materialData->color.x);
		//ImGui::SliderAngle("RotateX", &transformSprite.rotate.x, -500, 500);
		//ImGui::SliderAngle("RotateY", &transformSprite.rotate.y, -500, 500);
		//ImGui::SliderAngle("RotateZ", &transformSprite.rotate.z, -500, 500);
		//ImGui::DragFloat3("transform", &transformSprite.translate.x, -180, 180);
		//ImGui::DragFloat3("transformSprite", &transform.translate.x);
		//ImGui::SliderAngle("RotateXSprite", &transform.rotate.x, -500, 500);
		//ImGui::SliderAngle("RotateYSprite", &transform.rotate.y, -500, 500);
		//ImGui::SliderAngle("RotateZSprite", &transform.rotate.z, -500, 500);

		//if ( ImGui::Checkbox("useMonsterBall", &useMonsterBall) ) {
		//	if ( useMonsterBall ) {
		//		useFence = false;
		//		useCircle = false;
		//	}
		//}

		//if ( ImGui::Checkbox("fence", &useFence) ) {
		//	if ( useFence ) {
		//		useMonsterBall = false;
		//		useCircle = false;
		//	}
		//}

		//if ( ImGui::Checkbox("circle", &useCircle) ) {
		//	if ( useCircle ) {
		//		useMonsterBall = false;
		//		useFence = false;
		//	}
		//}

		//ImGui::ColorEdit4("LightColor", &directionalLightData->color.x);
		//ImGui::SliderFloat("LightX", &directionalLightData->direction.x, -10.0f, 10.0f);
		//ImGui::SliderFloat("LightY", &directionalLightData->direction.y, -10.0f, 10.0f);
		//ImGui::SliderFloat("LightZ", &directionalLightData->direction.z, -10.0f, 10.0f);
		//ImGui::DragFloat2("UVTransform", &uvTransformSprite.translate.x, 0.01f, -10.0f, 10.0f);
		//ImGui::DragFloat2("UVScale", &uvTransformSprite.scale.x, 0.01f, -10.0f, 10.0f);
		//ImGui::SliderAngle("UVRotate", &uvTransformSprite.rotate.z);



		//directionalLightData->direction = Normalize(directionalLightData->direction);

		//Matrix4x4 uvTransformMatrix = MakeScale(uvTransformSprite.scale);
		//uvTransformMatrix = Multiply(uvTransformMatrix, MakeRotateZ(uvTransformSprite.rotate.z));
		//uvTransformMatrix = Multiply(uvTransformMatrix, MakeTranslate(uvTransformSprite.translate));
		//materialDataSprite->uvTransfrom = uvTransformMatrix;









		// 画面クリアなど描画前処理
		dxCommon->PreDraw();

		srvManager->PreDraw();


		obj3dCommon->PreDraw(commandList);




		// モデル描画前にしっかり PSO と RootSignature を再設定
		/*commandList->SetGraphicsRootSignature(spriteCommon->GetRootSignature());
		commandList->SetPipelineState(spriteCommon->GetPipelineState());*/

		//// VBV/IBV 再設定
		//commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		//commandList->IASetIndexBuffer(&indexBufferView);

		//// トポロジ
		//commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		//// WVPなど定数バッファ
		//commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
		//commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());
		//commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
		//commandList->SetGraphicsRootConstantBufferView(3, directionalResourceLight->GetGPUVirtualAddress());

		//// 描画
		//commandList->DrawIndexedInstanced(static_cast< UINT >( modelData.indices.size() ), 1, 0, 0, 0);


		//// 条件に応じて切り替え
		//if ( useFence ) {
		//	selectedTextureHandle = textureSrvHandleGPU3; // 3番目（Fence）
		//} else if ( useMonsterBall ) {
		//	selectedTextureHandle = textureSrvHandleGPU2; // 2番目（MonsterBall）
		//}

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



		// 全てのスプライトを描画
		for ( Sprite* sprite : sprites ) {
			sprite->Draw();
		}

		obj3d->Draw();


		// ⑤ ImGui end → 描画コマンドを積む
		imgui.End(commandList);

		// DirectX 描画後処理（Present / フェンス）
		dxCommon->PostDraw();

		if ( input.Triggerkey(DIK_ESCAPE) ) {
			break;
		}
	}
	// imguiの終了処理
	imgui.Shutdown();

#pragma region オブジェクトを解放

	//CloseHandle(fenceEvent);
	//XAudio2解放
	xAudio2.Reset();
	// 音声データ解放
	SoundUnload(&soundData1);


	CloseWindow(windowProc.GetHwnd());

	//出力ウィンドウへの文字出力
	logManager.Log("HelloWored\n");
	logManager.Log(logManager.ConvertString(std::format(L"WSTRING{}\n", windowProc.GetClientWidth())));

	logManager.Finalize();

#pragma endregion




	// COMの終了処理
	CoUninitialize();

	return 0;

}