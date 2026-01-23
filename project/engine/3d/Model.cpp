#include "Model.h"
#include "Matrix4x4.h"
#include "TextureManager.h"
#include "SrvManager.h"
#include "Obj3dCommon.h"
#include "ModelCommon.h"
#include <numbers>
using namespace MatrixMath;

void Model::Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename){

	// 1. ModelCommonのポインタを記録
	this->modelCommon_ = modelCommon;



	// 2. モデルデータをファイルから読み込む (引数のパスを使う)
	modelData_ = LoadObjFile(directoryPath, filename);


	// 3. モデルの頂点座標を中心（原点）に合わせる
	AdjustModelCenter();

	// 4. マテリアルデータの読み込み (.mtlファイル)
	if ( modelData_.material.textureFilePath.empty() ) {
		modelData_.material.textureFilePath = "resources/uvChecker.png";
	}

	// 5. バッファの作成
	CreateBuffers();

}
// 球モデルの初期化
void Model::InitializeSphere(ModelCommon* modelCommon, int subdivision){

	this->modelCommon_ = modelCommon;
	modelData_ = {};


	// 2. 球体のデータを計算で生成
	const float kLonEvery = std::numbers::pi_v<float> *2.0f / float(subdivision);
	const float kLatEvery = std::numbers::pi_v<float> / float(subdivision);
	// 頂点データ生成
	for ( int latIndex = 0; latIndex < ( subdivision + 1 ); ++latIndex ) {
		float lat = -std::numbers::pi_v<float> / 2.0f + kLatEvery * latIndex;
		// 経度の方向に分割しながら線を描く
		for ( int lonIndex = 0; lonIndex < ( subdivision + 1 ); ++lonIndex ) {
			float lon = lonIndex * kLonEvery;
			VertexData vertex;

			// 座標
			vertex.position.x = std::cos(lat) * std::cos(lon);
			vertex.position.y = std::sin(lat);
			vertex.position.z = std::cos(lat) * std::sin(lon);
			vertex.position.w = 1.0f;
			// UV
			vertex.texcoord.x = float(lonIndex) / float(subdivision);
			vertex.texcoord.y = 1.0f - float(latIndex) / float(subdivision);
			// 法線
			vertex.normal.x = vertex.position.x;
			vertex.normal.y = vertex.position.y;
			vertex.normal.z = vertex.position.z;
			// 頂点データを追加
			modelData_.vertices.push_back(vertex);
		}
	}

	// インデックスデータ生成
	int vertexCountX = subdivision + 1;
	for ( int latIndex = 0; latIndex < subdivision; ++latIndex ) {
		for ( int lonIndex = 0; lonIndex < subdivision; ++lonIndex ) {
			uint32_t topLeft = latIndex * vertexCountX + lonIndex;
			uint32_t bottomLeft = ( latIndex + 1 ) * vertexCountX + lonIndex;
			uint32_t topRight = latIndex * vertexCountX + ( lonIndex + 1 );
			uint32_t bottomRight = ( latIndex + 1 ) * vertexCountX + ( lonIndex + 1 );

			modelData_.indices.push_back(topLeft);
			modelData_.indices.push_back(bottomLeft);
			modelData_.indices.push_back(topRight);
			modelData_.indices.push_back(topRight);
			modelData_.indices.push_back(bottomLeft);
			modelData_.indices.push_back(bottomRight);
		}
	}
	// テクスチャファイルパスはデフォルトのチェッカーテクスチャにしておく
	modelData_.material.textureFilePath = "resources/monsterBall.png";

	CreateBuffers(); // バッファの作成
}


void Model::Draw(){
	// 1. コマンドリストを取得する
	// ModelCommon経由でDirectXCommonから取得
	ID3D12GraphicsCommandList* commandList = modelCommon_->GetDxCommon()->GetCommandList();

	// 2. 頂点バッファビュー(VBV)の設定
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

	// 3. インデックスバッファビュー(IBV)の設定
	commandList->IASetIndexBuffer(&indexBufferView_);

	// 4. マテリアル定数バッファの設定 (RootParameter 0番)
	// ※元のObj3d.cppで 0番 に設定していたものです
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	// プリミティブトポロジの設定（三角形リスト）
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


	// 5. テクスチャの設定 (RootParameter 2番)
	// ※元のObj3d.cppで 2番 に設定していたものです
	if ( textureHandle_.ptr != 0 ) {
		commandList->SetGraphicsRootDescriptorTable(2, textureHandle_);
	}

	// 6. 描画コマンドの発行
	// インデックスを使って描画
	commandList->DrawIndexedInstanced(static_cast< UINT >( modelData_.indices.size() ), 1, 0, 0, 0);


}

// .mtlファイルの読み込み
Model::MaterialData Model::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename){

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
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}

	}
	return materialData; // 読み込んだMaterialDataを返す

}
// .objファイルの読み込み
Model::ModelData Model::LoadObjFile(const std::string& directoryPath, const std::string& filename){

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
// モデルの頂点座標を中心（原点）に合わせる
void Model::AdjustModelCenter(){

	// 1. すべての頂点の合計を求める
	Vector3 center = { 0.0f, 0.0f, 0.0f };
	for ( const auto& v : modelData_.vertices ) {
		center.x += v.position.x;
		center.y += v.position.y;
		center.z += v.position.z;
	}

	// 2. 頂点数で割って中心座標を求める
	float vertexCount = static_cast< float >( modelData_.vertices.size() );
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




}
// バッファの作成をまとめた関数
void Model::CreateBuffers(){

	auto dxCommon = modelCommon_->GetDxCommon();
	auto commandList = dxCommon->GetCommandList();
	auto resourceFactory = dxCommon->GetResourceFactory();

		// SRV作成とインデックス取得
	modelData_.material.textureIndex = TextureManager::GetInstance()->LoadTextureAndCreateSRV(
		modelData_.material.textureFilePath,
		commandList
	).srvIndex;

	// テクスチャハンドルを取得
	textureHandle_ = dxCommon->GetSrvManager()->GetGPUDescriptorHandle(
		modelData_.material.textureIndex
	);


	// 2. 頂点リソースを作る
	vertexResource_ = resourceFactory->CreateBufferResource(sizeof(VertexData) * modelData_.vertices.size());
	assert(vertexResource_ != nullptr);

	// VBV
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	// データ書き込み
	VertexData* vertexMap = nullptr;
	vertexResource_->Map(0, nullptr, reinterpret_cast< void** >( &vertexMap ));
	std::copy(modelData_.vertices.begin(), modelData_.vertices.end(), vertexMap);
	
	// 3. インデックスリソースを作る
	indexResource_ = resourceFactory->CreateBufferResource(sizeof(uint32_t) * modelData_.indices.size());
	assert(indexResource_ != nullptr);

	// IBV
	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	indexBufferView_.SizeInBytes = UINT(sizeof(uint32_t) * modelData_.indices.size());
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	// データ書き込み
	uint32_t* indexMap = nullptr;
	indexResource_->Map(0, nullptr, reinterpret_cast< void** >( &indexMap ));
	std::copy(modelData_.indices.begin(), modelData_.indices.end(), indexMap);
	
	// 4. マテリアル用のリソースを作る
	materialResource_ = resourceFactory->CreateBufferResource(sizeof(Material));
	assert(materialResource_ != nullptr);

	// データ書き込み（ここは書き換え頻度が高いのでMapしっぱなしでもOKですが、今回はUnmapしない実装にします）
	// クラスメンバに Material* materialData_ がある前提です
	materialResource_->Map(0, nullptr, reinterpret_cast< void** >( &materialData_ ));

	// デフォルト値
	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData_->enableLighting = true;
	materialData_->uvTransform = MatrixMath::MakeIdentity4x4();
	materialData_->shininess = 32.0f;


}
