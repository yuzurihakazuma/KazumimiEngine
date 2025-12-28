#include "Obj3d.h"
#include "Matrix4x4.h"
#include "Obj3dCommon.h"
#include "Model.h"

using namespace MatrixMath;

void Obj3d::Initialize(Obj3dCommon* obj3dCommon, Model* model){

	// 引数のチェックと保存
	assert(obj3dCommon != nullptr);
	this->obj3dCommon_ = obj3dCommon;
	this->model_ = model;

	auto dxCommon = obj3dCommon_->GetDxCommon();

	// ---------------------------------------------------------
	// WVP用のリソースを作る (場所の情報なので Obj3d が持つ)
	// ---------------------------------------------------------
	wvpResource_ = dxCommon->GetResourceFactory()->CreateBufferResource(sizeof(TransformationMatrix));
	assert(wvpResource_ != nullptr);

	// データを書き込むためのアドレスを取得
	wvpResource_->Map(0, nullptr, reinterpret_cast< void** >( &transformationMatrixData_ ));

	// 単位行列を初期値として書き込んでおく
	transformationMatrixData_->WVP = MakeIdentity4x4();
	transformationMatrixData_->World = MakeIdentity4x4();

	// ---------------------------------------------------------
	// 平行光源用のリソースを作る (環境の情報なので Obj3d が持つ)
	// ---------------------------------------------------------
	directionalResourceLight_ = dxCommon->GetResourceFactory()->CreateBufferResource(sizeof(DirectionalLight));
	assert(directionalResourceLight_ != nullptr);

	directionalResourceLight_->Map(0, nullptr, reinterpret_cast< void** >( &directionalLightData_ ));

	// ライトのデフォルト値
	directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData_->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData_->intensity = 1.0f;

	// ---------------------------------------------------------
	// Transform変数の初期化
	// ---------------------------------------------------------
	scale_ = { 1.0f, 1.0f, 1.0f };
	rotation_ = { 0.0f, 0.0f, 0.0f };
	translate_ = { 0.0f, 0.0f, 0.0f };
}

void Obj3d::Update(){

	// Transform変数から行列計算用の構造体へコピー
	transform.translate = translate_;
	transform.rotate = rotation_;
	transform.scale = scale_;

	auto dxCommon = obj3dCommon_->GetDxCommon();

	// アスペクト比の計算
	float aspect =
		float(dxCommon->GetClientWidth()) /
		float(dxCommon->GetClientHeight());

	// ワールド行列の計算 (SRT)
	Matrix4x4 worldMatrix = MakeAffine(transform.scale, transform.rotate, transform.translate);

	// カメラ行列の計算 (本来はCameraクラスで行うべきだが、今はここに残す)
	Matrix4x4 cameraMatrix = MakeAffine(cameraTransfrom.scale, cameraTransfrom.rotate, cameraTransfrom.translate);
	Matrix4x4 viewMatrix = Inverse(cameraMatrix);

	// プロジェクション行列の計算
	Matrix4x4 projectionMatrix = PerspectiveFov(0.45f, aspect, 0.1f, 100.0f);

	// WVP行列の合成 (World * View * Projection)
	Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));

	// 定数バッファへ転送
	transformationMatrixData_->World = worldMatrix;
	transformationMatrixData_->WVP = worldViewProjectionMatrix;
}

void Obj3d::Draw(){

	// コマンドリスト取得
	auto commandList = obj3dCommon_->GetDxCommon()->GetCommandList();

	// 1. 座標情報の転送 (WVP) -> RootParameter[1]
	// ※PipelineManagerの設定（RootSignature）と番号を合わせてください
	commandList->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());

	// 2. ライト情報の転送 -> RootParameter[3]
	commandList->SetGraphicsRootConstantBufferView(3, directionalResourceLight_->GetGPUVirtualAddress());

	// 3. モデルの描画処理を呼び出す 
	// (ここで頂点、インデックス、マテリアル、テクスチャの設定とDrawCallが行われる)
	if ( model_ ) {
		model_->Draw();
	}
}