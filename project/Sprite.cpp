#include "Sprite.h"
#include"SpriteCommon.h"


using namespace MatrixMath;

void Sprite::Initialize(SpriteCommon* spriteCommon){

	this->spriteCommon = spriteCommon;

    // 頂点リソースを作成（例: 4頂点）
    vertexResource_ = spriteCommon->GetDxCommon()->GetResourceFactory()->CreateBufferResource(sizeof(VertexData) * 4);

    // ビューの設定
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(VertexData) * 4;
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    // マップしてポインタ取得
    vertexResource_->Map(0, nullptr, reinterpret_cast< void** >( &vertexData_ ));

	// 1枚目の三角形
	// 左上
	vertexData_[0].position = { 0.0f, 0.0f, 0.0f, 1.0f };
	vertexData_[0].texcoord = { 0.0f, 0.0f };
	vertexData_[0].normal = { 0.0f, 0.0f, -1.0f };

	// 左下
	vertexData_[1].position = { 0.0f, 360.0f, 0.0f, 1.0f };
	vertexData_[1].texcoord = { 0.0f, 1.0f };
	vertexData_[1].normal = { 0.0f, 0.0f, -1.0f };

	// 右上
	vertexData_[2].position = { 640.0f, 0.0f, 0.0f, 1.0f };
	vertexData_[2].texcoord = { 1.0f, 0.0f };
	vertexData_[2].normal = { 0.0f, 0.0f, -1.0f };

	// 右下
	vertexData_[3].position = { 640.0f, 360.0f, 0.0f, 1.0f };
	vertexData_[3].texcoord = { 1.0f, 1.0f };
	vertexData_[3].normal = { 0.0f, 0.0f, -1.0f };
	// Sprite用のマテリアルリソースを作る
	materialResource_ = spriteCommon->GetDxCommon()->GetResourceFactory()->CreateBufferResource(sizeof(Material));

	// Mapしてデータを書き込む。色は白設定しておく
	materialResource_->Map(0, nullptr, reinterpret_cast< void** >( &materialData_ ));

	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	// SpriteはLightingしないのでfalseを設定する
	materialData_->enableLighting = false;
	// UVTransformはSpriteでは使うので設定しておく。今回は単位行列を設定しておく
	materialData_->uvTransfrom = MakeIdentity4x4();


	indexResource_ = spriteCommon->GetDxCommon()->GetResourceFactory()->CreateBufferResource(sizeof(uint32_t) * 6);

	// リソースの先頭のアドレスから使う
	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();

	//使用するリソースのサイズはindex6つ分のサイズ 
	indexBufferView_.SizeInBytes = sizeof(uint32_t) * 6;

	// indexはuint32_tとする
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	indexResource_->Map(0, nullptr, reinterpret_cast< void** >( &indexData_ ));

	indexData_[0] = 0;
	indexData_[1] = 2;
	indexData_[2] = 1;
	indexData_[3] = 1;
	indexData_[4] = 2;
	indexData_[5] = 3;

	indexResource_->Unmap(0, nullptr);




	transformationMatrixResource_ = spriteCommon->GetDxCommon()->GetResourceFactory()->CreateBufferResource(sizeof(TransformationMatrix));

	// 書き込むためのアドレスを取得
	transformationMatrixResource_->Map(0, nullptr, reinterpret_cast< void** >( &transformationMatirxData_ ));
	// 単位行列を書き込んでおく
	transformationMatirxData_->WVP = MakeIdentity4x4();
	transformationMatirxData_->World = MakeIdentity4x4();

}

void Sprite::Update(){

	
	Matrix4x4 worldMatrix = MakeAffine(transform.scale, transform.rotate, transform.translate);
	Matrix4x4 viewMatrix = MakeIdentity4x4();
	Matrix4x4 projectionMatrix = Orthographic(0.0f, 0.0f, width_, height_, 0.0f, 100.0f);
	Matrix4x4 viewProjection = Multiply(viewMatrix, projectionMatrix);
	Matrix4x4 worldViewProjectionMatrix = Multiply(viewProjection, worldMatrix);
	transformationMatirxData_->WVP = worldViewProjectionMatrix;
	transformationMatirxData_->World = worldMatrix;



}

void Sprite::Draw(){

	auto commandList = spriteCommon->GetDxCommon()->GetCommandList();

	// トポロジの設定
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	// VBVを設定 : IBVを設定
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetIndexBuffer(&indexBufferView_);

	// マテリアル定数バッファ
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

	// 行列定数バッファ
	commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());

	// RootSignatureが共通である以上、バインドしないとエラーまたは不定動作になります。
	//commandList->SetGraphicsRootConstantBufferView(3, directionalResourceLight->GetGPUVirtualAddress());

	// ここで更新してSpriteの画像を変えないようにする
	if ( textureHandle_.ptr != 0 ) { // ハンドルがセットされているか念のため確認
		commandList->SetGraphicsRootDescriptorTable(2, textureHandle_);
	}

	//描画！(DraoCall/ドローコール)
	commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);




}
