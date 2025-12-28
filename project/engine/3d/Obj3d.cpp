#include "Obj3d.h"
#include "Matrix4x4.h"
#include "TextureManager.h"
#include "SrvManager.h"
#include "Obj3dCommon.h"
using namespace MatrixMath;


void Obj3d::Initialize(Obj3dCommon* obj3dCommon){

	this->obj3dCommon = obj3dCommon;

	assert(obj3dCommon != nullptr);
	assert(obj3dCommon->GetDxCommon() != nullptr);
	assert(obj3dCommon->GetDxCommon()->GetSrvManager() != nullptr);


	modelData_ = LoadObjFile("resources", "plane.obj");


	// ★追加：もしテクスチャのパスが空っぽなら、uvChecker.png を使うようにする
	if ( modelData_.material.textrueFilePath.empty() ) {
		modelData_.material.textrueFilePath = "resources/uvChecker.png";
	}

	assert(modelData_.vertices.size() > 0);

	TextureManager::GetInstance()->LoadTexture(
		modelData_.material.textrueFilePath
	);



	modelData_.material.textureIndex =
		TextureManager::GetInstance()->LoadTextureAndCreateSRV(
			modelData_.material.textrueFilePath,
			obj3dCommon->GetDxCommon()->GetCommandList()
		).srvIndex;



	textureHandle_ =
		obj3dCommon->GetDxCommon()
		->GetSrvManager()
		->GetGPUDescriptorHandle(
			modelData_.material.textureIndex
		);

	// 1. すべての頂点の合計を求める
	Vector3 center = { 0.0f, 0.0f, 0.0f };
	for ( const auto& v : modelData_.vertices ) {
		center.x += v.position.x;
		center.y += v.position.y;
		center.z += v.position.z;
	}

	// 2. 頂点数で割って中心座標を求める
	size_t vertexCount = modelData_.vertices.size();
	if ( vertexCount > 0 ) {
		center.x /= vertexCount;
		center.y /= vertexCount;
		center.z /= vertexCount;
	}

	// 3. すべての頂点座標から中心座標を引く
	for ( auto& v : modelData_.vertices ) {
		v.position.x -= center.x;
		v.position.y -= center.y;
		v.position.z -= center.z;
	}




	// 頂点リソースを作る
	vertexResource_ = obj3dCommon->GetDxCommon()->GetResourceFactory()->CreateBufferResource(sizeof(VertexData) * modelData_.vertices.size());

	assert(vertexResource_ != nullptr);

	// 頂点バッファビューを作成する
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	// 頂点リソースにデータを書き込む
	vertexResource_->Map(0, nullptr, reinterpret_cast< void** >( &vertexData_ ));
	std::copy(modelData_.vertices.begin(), modelData_.vertices.end(), vertexData_);

	// インデックスリソースも同様に
	indexResource_ = obj3dCommon->GetDxCommon()->GetResourceFactory()->CreateBufferResource(sizeof(uint32_t) * modelData_.indices.size());

	assert(indexResource_ != nullptr);


	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	indexBufferView_.SizeInBytes = UINT(sizeof(uint32_t) * modelData_.indices.size());
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;



	indexResource_->Map(0, nullptr, reinterpret_cast< void** >( &indexData_ ));
	std::copy(modelData_.indices.begin(), modelData_.indices.end(), indexData_);
	indexResource_->Unmap(0, nullptr);
	vertexResource_->Unmap(0, nullptr);


	// WVB用のリソースを作る。Matrix4x4 一つ分のサイズを用意する
	wvpResource_ = obj3dCommon->GetDxCommon()->GetResourceFactory()->CreateBufferResource(sizeof(TransformationMatrix));

	assert(wvpResource_ != nullptr && "WVPResource is NULL!");

	// データを書き込む

	// 書き込むためのアドレスを取得
	wvpResource_->Map(0, nullptr, reinterpret_cast< void** >( &transformationMatrixData_ ));

	// 単位行列を書き込んでおく
	transformationMatrixData_->WVP = MakeIdentity4x4();
	transformationMatrixData_->World = MakeIdentity4x4();

	// マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	materialResource_ = obj3dCommon->GetDxCommon()->GetResourceFactory()->CreateBufferResource(sizeof(Material));

	assert(materialResource_ != nullptr);

	// 書き込むためのアドレスを取得
	materialResource_->Map(0, nullptr, reinterpret_cast< void** >( &materialData_ ));
	// 今回は赤を書き込んでいる
	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData_->enableLighting = true;
	materialData_->uvTransfrom = MakeIdentity4x4();



	directionalResourceLight = obj3dCommon->GetDxCommon()->GetResourceFactory()->CreateBufferResource(sizeof(DirectionalLight));

	assert(directionalResourceLight != nullptr && "DirectionalLightResource is NULL!");

	directionalResourceLight->Map(0, nullptr, reinterpret_cast< void** >( &directionalLightData_ ));

	// デフォルト値はとりあえず以下のようにしておく
	directionalLightData_->color = { 1.0f,1.0f,1.0f,1.0f };
	directionalLightData_->direction = { 0.0f, -1.0f,0.0f };
	directionalLightData_->intensity = 1.0f;

	scale_ = { 1.0f, 1.0f, 1.0f };
	rotation_ = { 0.0f, 0.0f, 0.0f };
	position_ = { 0.0f, 0.0f, 0.0f };

}


void  Obj3d::Update(){





	transform.translate = { position_.x, position_.y, 0.0f };
	transform.rotate = { 0.0f, 0.0f, rotation_.z };
	transform.scale = { scale_.x, scale_.y, 1.0f };

	auto dxCommon = obj3dCommon->GetDxCommon();
	float aspect =
		float(dxCommon->GetClientWidth()) /
		float(dxCommon->GetClientHeight());


	//plame
		//transform.rotate.y += 0.03f;
	Matrix4x4 worldMatrix = MakeAffine(transform.scale, transform.rotate, transform.translate);
	Matrix4x4 cameraMatrix = MakeAffine(cameraTransfrom.scale, cameraTransfrom.rotate, cameraTransfrom.translate);
	Matrix4x4 viewMatrix = Inverse(cameraMatrix); // ← 通常カメラの行列

	Matrix4x4 projectionMatrix = PerspectiveFov(1.0f, aspect, 0.1f, 100.0f);
	Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
	transformationMatrixData_->World = worldMatrix;
	transformationMatrixData_->WVP = worldViewProjectionMatrix;



}

void  Obj3d::Draw(){

	auto commandList = obj3dCommon->GetDxCommon()->GetCommandList();


	// VBV/IBV 再設定
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetIndexBuffer(&indexBufferView_);

	// トポロジ
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// WVPなど定数バッファ
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());
	// ここで更新してSpriteの画像を変えないようにする
	if ( textureHandle_.ptr != 0 ) { // ハンドルがセットされているか念のため確認
		commandList->SetGraphicsRootDescriptorTable(2, textureHandle_);
	}
	commandList->SetGraphicsRootConstantBufferView(3, directionalResourceLight->GetGPUVirtualAddress());

	// 描画
	commandList->DrawIndexedInstanced(static_cast< UINT >( modelData_.indices.size() ), 1, 0, 0, 0);




}


Obj3d::MaterialData Obj3d::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename){

	MaterialData materialData; // MaterialDataを構築
	std::string line; // ファイルから読んだ1行を格納するもの
	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open()); // 開けなかったら止める
	while ( std::getline(file, line) ){
		std::string identifier; // 行の先頭の識別子を格納する
		std::istringstream s(line); // 先頭の認別子を読む
		s >> identifier; // 先頭の識別子を読み込む

		// identifierに応じた処理
		if ( identifier == "map_Kd" ){
			std::string textureFilename; // テクスチャファイル名を格納する変数
			s >> textureFilename; // テクスチャファイル名を読み込む
			// 連結してファイルパスにする
			materialData.textrueFilePath = directoryPath + "/" + textureFilename;
		}

	}
	return materialData; // 読み込んだMaterialDataを返す

}

Obj3d::ModelData Obj3d::LoadObjFile(const std::string& directoryPath, const std::string& filename){

	ModelData modelData; // 構造するModelData
	std::vector<Vector4> positions; // 位置
	std::vector<Vector3> normals; // 法線
	std::vector<Vector2> texcoords; // テクスチャ座標
	std::string line; // ファイルから読んだ1行を格納するもの

	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く

	assert(file.is_open()); // 開けなかったら止める

	while ( std::getline(file, line) ) {

		std::string identifier; // 行の先頭の識別子を格納する
		std::istringstream s(line); // 先頭の認別子を読む
		s >> identifier; // 先頭の識別子を読み込む
		if ( identifier == "v" ) {
			Vector4 position; // 位置を格納する変数
			s >> position.x >> position.y >> position.z; // 位置を読み込む
			// DirectX座標系に合わせて反転
			//position.x *= -1.0f;
			position.x *= 1.0f;
			position.w = 1.0f;
			positions.push_back(position); // 位置を格納する
		} else if ( identifier == "vt" ) {
			Vector2 texcoord; // テクスチャ座標を格納する変数
			s >> texcoord.x >> texcoord.y; // テクスチャ座標を読み込む
			texcoord.y = 1.0f - texcoord.y; // Y座標を反転する（OpenGLとDirectXの違い）
			texcoords.push_back(texcoord); // テクスチャ座標を格納する

		} else if ( identifier == "vn" ) {
			Vector3 normal; // 法線を格納する変数
			s >> normal.x >> normal.y >> normal.z; // 法線を読み込む
			normal.x *= 1.0f;
			//normal.x *= -1.0f;
			normals.push_back(normal); // 法線を格納する
		} else if ( identifier == "f" ) {
			VertexData triangle[3]; // 三角形の頂点データを格納する配列
			for ( int32_t faceVertex = 0; faceVertex < 3; ++faceVertex ) {
				std::string vertexDefinition;
				s >> vertexDefinition;

				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3] = {};
				for ( int32_t element = 0; element < 3; ++element ) {
					std::string index;
					std::getline(v, index, '/');
					elementIndices[element] = std::stoi(index);
				}
				triangle[faceVertex] = {
					positions[elementIndices[0] - 1], // 位置は1オリジンなので-1する
					texcoords[elementIndices[1] - 1], // テクスチャ座標も1オリジンなので-1する
					normals[elementIndices[2] - 1] // 法線も1オリジンなので-1する
				};




			}

			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);
			modelData.indices.push_back(static_cast< uint32_t >( modelData.vertices.size() - 3 ));
			modelData.indices.push_back(static_cast< uint32_t >( modelData.vertices.size() - 2 ));
			modelData.indices.push_back(static_cast< uint32_t >( modelData.vertices.size() - 1 ));
		} else if ( identifier == "mtllib" ) {
			// マテリアルの指定
			std::string materialFilename; // マテリアル名を格納する変数
			s >> materialFilename; // マテリアル名を読み込む
			// マテリアルデータを読み込む
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
	}
	return modelData;
}


