#include "Model.h"
#include "Matrix4x4.h"
#include "TextureManager.h"
#include "SrvManager.h"
#include "Obj3dCommon.h"
using namespace MatrixMath;

void Model::Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename){

	// 1. ModelCommonのポインタを記録
	this->modelCommon_ = modelCommon;

	// 2. モデルデータをファイルから読み込む (引数のパスを使う)
	modelData_ = LoadObjFile(directoryPath, filename);

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

	// ★テクスチャがない場合のデフォルト処理
	if ( modelData_.material.textureFilePath.empty() ) {
		modelData_.material.textureFilePath = "resources/uvChecker.png";
	}

	// 3. テクスチャの読み込みとSRV作成
	// コマンドリストが必要なので、ModelCommon経由で取得する
	auto dxCommon = modelCommon_->GetDxCommon();
	auto commandList = dxCommon->GetCommandList();

	TextureManager::GetInstance()->LoadTexture(
		modelData_.material.textureFilePath
	);

	modelData_.material.textureIndex =
		TextureManager::GetInstance()->LoadTextureAndCreateSRV(
			modelData_.material.textureFilePath,
			commandList
		).srvIndex;

	// TextureHandleは描画時に使うので取得しておく
	textureHandle_ = dxCommon->GetSrvManager()->GetGPUDescriptorHandle(
		modelData_.material.textureIndex
	);


	// 4. 頂点リソースを作る
	// ResourceFactoryもModelCommon経由で取得
	auto resourceFactory = dxCommon->GetResourceFactory();

	vertexResource_ = resourceFactory->CreateBufferResource(sizeof(VertexData) * modelData_.vertices.size());
	assert(vertexResource_ != nullptr);

	// 頂点バッファビューを作成する
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	// 頂点リソースにデータを書き込む
	vertexResource_->Map(0, nullptr, reinterpret_cast< void** >( &vertexData_ ));
	std::copy(modelData_.vertices.begin(), modelData_.vertices.end(), vertexData_);


	// 5. インデックスリソースを作る
	indexResource_ = resourceFactory->CreateBufferResource(sizeof(uint32_t) * modelData_.indices.size());
	assert(indexResource_ != nullptr);

	// インデックスバッファビューを作成する
	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	indexBufferView_.SizeInBytes = UINT(sizeof(uint32_t) * modelData_.indices.size());
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	// インデックスリソースにデータを書き込む
	indexResource_->Map(0, nullptr, reinterpret_cast< void** >( &indexData_ ));
	std::copy(modelData_.indices.begin(), modelData_.indices.end(), indexData_);

	// ※Unmapするかどうかは元の実装方針に合わせます（MapしっぱなしならそのままでOK）


	// 6. マテリアル用のリソースを作る
	materialResource_ = resourceFactory->CreateBufferResource(sizeof(Material));
	assert(materialResource_ != nullptr);

	// データを書き込むためのアドレスを取得
	materialResource_->Map(0, nullptr, reinterpret_cast< void** >( &materialData_ ));

	// デフォルト値を書き込んでおく（色は白にしておくのが一般的）
	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData_->enableLighting = true;
	materialData_->uvTransform = MakeIdentity4x4();

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

	// 5. テクスチャの設定 (RootParameter 2番)
	// ※元のObj3d.cppで 2番 に設定していたものです
	if ( textureHandle_.ptr != 0 ) {
		commandList->SetGraphicsRootDescriptorTable(2, textureHandle_);
	}

	// 6. 描画コマンドの発行
	// インデックスを使って描画
	commandList->DrawIndexedInstanced(static_cast< UINT >( modelData_.indices.size() ), 1, 0, 0, 0);


}


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