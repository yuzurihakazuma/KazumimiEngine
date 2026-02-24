#include "Obj3d.h"

// --- 標準ライブラリ ---
#include <cassert>

// --- エンジン側のファイル ---
#include "Obj3dCommon.h" 
#include "engine/3d/model/Model.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/camera/Camera.h"
#include "engine/math/Matrix4x4.h"
#include "engine/base/DirectXCommon.h"
#include "engine/graphics/ResourceFactory.h"

using namespace MatrixMath;

std::unique_ptr<Obj3d> Obj3d::Create(const std::string& modelName, const Vector3& translate, const Vector3& rotate, const Vector3& scale) {

	// 1. モデルを検索
	Model* model = ModelManager::GetInstance()->FindModel(modelName);

	// モデルが見つからなかった場合は nullptr を返す
	if (!model) {
		return nullptr;
	}

	// 2. make_unique で生成（new の代わり）
	std::unique_ptr<Obj3d> obj3d = std::make_unique<Obj3d>();

	// 3. 初期化
	obj3d->Initialize(model);

	// 4. 各種パラメータをセット
	obj3d->SetTranslation(translate);
	obj3d->SetRotation(rotate);
	obj3d->SetScale(scale);

	// 5. そのまま返す（moveされるので所有権が移ります）
	return obj3d;
}


void Obj3d::Initialize(Model* model){

	// 引数の保存
	this->model_ = model;
	// Obj3dCommonの取得
	this->obj3dCommon_ = Obj3dCommon::GetInstance();
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
	// ワールド行列の逆行列
	Matrix4x4 worldInverse = Inverse(worldMatrix);

	
	// 逆行列の転置行列
	Matrix4x4 worldInverseTranspose = Transpose(worldInverse);

	// 定数バッファへ転送
	transformationMatrixData_->World = worldMatrix;
	transformationMatrixData_->WVP = worldViewProjectionMatrix;
	transformationMatrixData_->WorldInverseTranspose = worldInverseTranspose;
}

void Obj3d::Draw(){

	// コマンドリスト取得
	ID3D12GraphicsCommandList* commandList =
		obj3dCommon_->GetDxCommon()->GetCommandList();
	assert(commandList != nullptr);

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
	// 点光源の転送 -> RootParameter[5]
	commandList->SetGraphicsRootConstantBufferView(5, Obj3dCommon::GetInstance()->GetPointLightResource()->GetGPUVirtualAddress());
	// スポットライトの転送 -> RootParameter[6]
	commandList->SetGraphicsRootConstantBufferView(6, Obj3dCommon::GetInstance()->GetSpotLightResource()->GetGPUVirtualAddress());

	// 3. モデルの描画処理を呼び出す 
	// (ここで頂点、インデックス、マテリアル、テクスチャの設定とDrawCallが行われる)
	if ( model_ ) {
		model_->Draw();
	}
}