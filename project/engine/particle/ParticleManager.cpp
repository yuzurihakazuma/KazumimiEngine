#include "ParticleManager.h"
#include "TextureManager.h"
#include "PipelineManager.h" // パイプライン設定のため必要なら
#include <cassert>
#include <random>
#include <numbers>

void ParticleManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager){
	assert(dxCommon);
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	// モデルデータの作成
	CreateModel();

	D3D12_HEAP_PROPERTIES uploadHeapProps {};
	uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC resourceDesc {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = ( sizeof(Vector4) + 0xFF ) & ~0xFF;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&uploadHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&materialResource_)
	);
	assert(SUCCEEDED(hr));

	// 白色(1,1,1,1)を書き込んでおく
	Vector4* data = nullptr;
	materialResource_->Map(0, nullptr, reinterpret_cast< void** >( &data ));
	*data = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialResource_->Unmap(0, nullptr);


}


void ParticleManager::Finalize(){
	// 各グループのインスタンシングリソースを解放
	for ( auto& [name, group] : particleGroups_ ) {
		group.instancingData = nullptr;
		group.instancingResource.Reset();
	}
	particleGroups_.clear();
	// 共通リソースの解放
	vertexResource_.Reset();

	materialResource_.Reset();

}

void ParticleManager::Update(Camera* camera){
	Matrix4x4 cameraMatrix = camera->GetWorldMatrix();
	Matrix4x4 viewProjection = camera->GetViewProjectionMatrix();

	// 1. 板ポリの裏面がカメラ側を向いているので、Y軸で180度(π)回転させて表を向ける行列
	Matrix4x4 backToFrontMatrix = MatrixMath::MakeRotateY(std::numbers::pi_v<float>);

	// 2. カメラの回転を適用する（ビルボード行列の作成）
	Matrix4x4 billboardMatrix = MatrixMath::Multiply(backToFrontMatrix, cameraMatrix);

	// 3. 平行移動成分は不要なので除去する（カメラの位置についてきてしまうのを防ぐため）
	billboardMatrix.m[3][0] = 0.0f;
	billboardMatrix.m[3][1] = 0.0f;
	billboardMatrix.m[3][2] = 0.0f;

	// 全てのグループについて処理
	for ( auto& [name, group] : particleGroups_ ) {
		// インスタンス数をリセット
		group.kNumInstance = 0;

		// 【最適化2】 更新と描画データ作成を1つのループにまとめる
		for ( auto it = group.particles.begin(); it != group.particles.end(); ) {

			// 1. 更新処理
			it->currentTime += 1.0f / 60.0f; // 時間を進める

			// 寿命尽きたら削除
			if ( it->currentTime >= it->lifeTime ) {
				it = group.particles.erase(it);
				continue; // 次の要素へ
			}

			// 移動処理
			it->transform.translate.x += it->velocity.x;
			it->transform.translate.y += it->velocity.y;
			it->transform.translate.z += it->velocity.z;

			// 2. 描画データの作成 (最大数を超えていなければ)
			if ( group.kNumInstance < kNumMaxInstance ) {
				// 行列計算
				Matrix4x4 scaleMatrix = MatrixMath::MakeScale(it->transform.scale);
				Matrix4x4 translateMatrix = MatrixMath::MakeTranslate(it->transform.translate);

				// World = Scale * Billboard * Translate
				Matrix4x4 worldMatrix = MatrixMath::Multiply(scaleMatrix, MatrixMath::Multiply(billboardMatrix, translateMatrix));
				Matrix4x4 wvp = MatrixMath::Multiply(worldMatrix, viewProjection);

				// GPU用データに書き込む
				ParticleForGPU& data = group.instancingData[group.kNumInstance];
				data.WVP = wvp;
				data.World = worldMatrix;
				data.color = it->color;

				// 個数カウントアップ
				group.kNumInstance++;
			}

			++it; // 次のパーティクルへ
		}
	}
}

void ParticleManager::Draw(ID3D12GraphicsCommandList* commandList){

	srvManager_->PreDraw();

	// 頂点バッファをセット (全グループ共通)
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

	// グループごとに描画命令を発行
	for ( auto& [name, group] : particleGroups_ ) {
		// 描画するものがなければスキップ
		if ( group.kNumInstance == 0 ) {
			continue;
		}
		// SRVのセット
 // RootParameter[1] : インスタンシングデータ (StructuredBuffer)
		srvManager_->SetGraphicsRootDescriptorTable(1, group.instancingSrvIndex);

		// RootParameter[2] : テクスチャ (Texture2D)
		srvManager_->SetGraphicsRootDescriptorTable(2, group.textureSrvIndex);

		// 描画 (6頂点 * インスタンス数)
		commandList->DrawInstanced(6, group.kNumInstance, 0, 0);
	}
}

void ParticleManager::CreateParticleGroup(const std::string& name, const std::string& textureFilePath){
	// 既に同じ名前のグループがあれば何もしない
	if ( particleGroups_.contains(name) ) {
		return;
	}

	// 新しいグループを作成して登録
	ParticleGroup& group = particleGroups_[name];
	group.textureFilePath = textureFilePath;

	// dxCommonからコマンドリストを取得して渡す必要がある
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	// テクスチャを読み込み、SRVを作成し、そのデータを取得する
	TextureData textureData = TextureManager::GetInstance()->LoadTextureAndCreateSRV(textureFilePath, commandList);

	// 取得したSRVインデックスをグループに保存
	group.textureSrvIndex = textureData.srvIndex;



	// インスタンシング用リソースの生成
	D3D12_HEAP_PROPERTIES uploadHeapProps {};
	uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC resourceDesc {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeof(ParticleForGPU) * kNumMaxInstance;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&uploadHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&group.instancingResource)
	);
	assert(SUCCEEDED(hr));

	// SRVを作成 (StructuredBufferとして)
	group.instancingSrvIndex = srvManager_->Allocate();
	srvManager_->CreateSRVforStructuredBuffer(
		group.instancingSrvIndex,
		group.instancingResource.Get(),
		kNumMaxInstance,
		sizeof(ParticleForGPU)
	);

	// 書き込み用ポインタを取得しておく
	group.instancingResource->Map(0, nullptr, reinterpret_cast< void** >( &group.instancingData ));
	group.kNumInstance = 0;
}

void ParticleManager::Emit(const std::string& name, const Vector3& position, uint32_t count){
	if ( !particleGroups_.contains(name) ) {
		return;
	}

	ParticleGroup& group = particleGroups_[name];

	// ランダム生成器の準備
	std::random_device seed_gen;
	std::mt19937 engine(seed_gen());

	// ループの中身が超スッキリ！
	for ( uint32_t i = 0; i < count; ++i ) {
		// 関数を呼ぶだけで1粒作れる
		Particle newParticle = MakeNewParticle(engine, position);
		group.particles.push_back(newParticle);
	}
}
void ParticleManager::CreateModel(){
	// -------------------------------------------------
	// 1. 共通の頂点データ(板ポリゴン)を作成
	// -------------------------------------------------
	struct VertexData{
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	// 6頂点で四角形を作る (2枚の三角形)
	VertexData vertices[] = {
		// 三角形1
		{{-1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}}, // 左下
		{{-1.0f,  1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}}, // 左上
		{{ 1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}}, // 右下
		// 三角形2
		{{-1.0f,  1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}}, // 左上
		{{ 1.0f,  1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}}, // 右上
		{{ 1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}}, // 右下
	};

	// リソース生成 (アップロードヒープ)
	// ★ DirectXCommonなどに CreateBufferResource というヘルパー関数があれば
	//    それを使うともっと短くなりますが、今はそのままでOKです。
	D3D12_HEAP_PROPERTIES uploadHeapProps {};
	uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC resourceDesc {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeof(vertices);
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&uploadHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertexResource_)
	);
	assert(SUCCEEDED(hr));

	// VBV作成
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(vertices);
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	// データを転送
	void* data = nullptr;
	vertexResource_->Map(0, nullptr, &data);
	std::memcpy(data, vertices, sizeof(vertices));
	vertexResource_->Unmap(0, nullptr);
}



Particle ParticleManager::MakeNewParticle(std::mt19937& engine, const Vector3& position){
	// 分布の定義（ここも引数で調整できるようにするとさらに汎用的になりますが、一旦固定で）
	std::uniform_real_distribution<float> distPos(-1.0f, 1.0f);
	std::uniform_real_distribution<float> distVel(-0.05f, 0.05f);
	std::uniform_real_distribution<float> distColor(0.5f, 1.0f);
	std::uniform_real_distribution<float> distTime(1.0f, 3.0f);

	Particle p;
	p.transform.scale = { 1.0f, 1.0f, 1.0f };
	p.transform.rotate = { 0.0f, 0.0f, 0.0f };

	// ランダムな位置と速度を設定
	p.transform.translate = {
		position.x + distPos(engine),
		position.y + distPos(engine),
		position.z + distPos(engine)
	};
	p.velocity = { distVel(engine), distVel(engine), distVel(engine) };
	p.color = { distColor(engine), distColor(engine), distColor(engine), 1.0f };
	p.lifeTime = distTime(engine);
	p.currentTime = 0.0f;

	return p;
}

size_t ParticleManager::GetParticleCount(const std::string& name) const{
	auto it = particleGroups_.find(name);
	if ( it == particleGroups_.end() ) {
		return 0;
	}
	return it->second.particles.size();
}

uint32_t ParticleManager::GetInstanceCount(const std::string& name) const{
	auto it = particleGroups_.find(name);
	if ( it == particleGroups_.end() ) {
		return 0;
	}
	return it->second.kNumInstance;
}
