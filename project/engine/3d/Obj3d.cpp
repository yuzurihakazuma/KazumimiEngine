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

	// ワールド行列の計算 (SRT)
	Matrix4x4 worldMatrix = MakeAffine(transform.scale, transform.rotate, transform.translate);
	// ワールド × ビュー × プロジェクション行列
	Matrix4x4 worldViewProjectionMatrix;

	// カメラがあれば、カメラの行列を使う
	if ( camera_ ) {
		// カメラから「ビュー・プロジェクション行列」をもらう
		const Matrix4x4& viewProjectionMatrix = camera_->GetViewProjectionMatrix();

		// ワールド行列 × (ビュー × プロジェクション)
		worldViewProjectionMatrix = Multiply(worldMatrix, viewProjectionMatrix);
	} else {
		// カメラがないときは、とりあえずワールド行列だけ入れておく（描画は崩れるがエラー落ち防止）
		worldViewProjectionMatrix = worldMatrix;
	}
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
	commandList->SetGraphicsRootConstantBufferView(
		3, obj3dCommon_->GetLightResource()->GetGPUVirtualAddress()
	);

	if ( camera_ ) {
		commandList->SetGraphicsRootConstantBufferView(
			4, camera_->GetCameraResource()->GetGPUVirtualAddress()
		);
	}
	// 3. モデルの描画処理を呼び出す 
	// (ここで頂点、インデックス、マテリアル、テクスチャの設定とDrawCallが行われる)
	if ( model_ ) {
		model_->Draw();
	}
}