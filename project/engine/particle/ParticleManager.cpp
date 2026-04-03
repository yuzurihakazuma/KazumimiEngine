#include "ParticleManager.h"
#include "ParticleManager.h"

// --- 標準ライブラリ ---
#include <cassert>
#include <random>
#include <numbers>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include "externals/nlohmann/json.hpp"
#include <fstream>
using json = nlohmann::json;

// --- エンジン側のファイル ---
#include "engine/graphics/TextureManager.h"
#include "engine/graphics/PipelineManager.h" 
#include "engine/base/DirectXCommon.h"
#include "engine/graphics/SrvManager.h"
#include "engine/camera/Camera.h"

// --- 数学関数（新しく分けたもの） ---
#include "engine/math/Matrix4x4.h"
#include "engine/math/VectorMath.h"

// 名前空間の使用を宣言
using namespace MatrixMath;
using namespace VectorMath;

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

	// 3. 平行移動成分は不要なので除去する
	billboardMatrix.m[3][0] = 0.0f;
	billboardMatrix.m[3][1] = 0.0f;
	billboardMatrix.m[3][2] = 0.0f;

	// 全てのグループについて処理
	for ( auto& [name, group] : particleGroups_ ) {
		// インスタンス数をリセット
		group.kNumInstance = 0;

		//  【最適化】vector と Swap&Pop を使った超高速ループ
		for ( int i = 0; i < group.particles.size(); ) {
			Particle& p = group.particles[i];

			// 1. 更新処理
			p.currentTime += 1.0f / 60.0f; // 時間を進める

			// 寿命尽きたら「Swap & Pop」で超高速削除
			if ( p.currentTime >= p.lifeTime ) {
				group.particles[i] = group.particles.back(); // 一番後ろを今の場所に持ってくる
				group.particles.pop_back();                  // 一番後ろを消す
				continue; // 次のループへ（iは増やさない！）
			}

			// 重力を足す（設定から！）
			p.velocity.y += group.setting.gravity;

			// 移動処理
			p.transform.translate.x += p.velocity.x;
			p.transform.translate.y += p.velocity.y;
			p.transform.translate.z += p.velocity.z;

			// フェードアウト＆徐々にサイズ変更
			float lifeRatio = p.currentTime / p.lifeTime;
			p.color.w = 1.0f - lifeRatio; // 徐々に透明に

			float currentScale = group.setting.startScale + ( group.setting.endScale - group.setting.startScale ) * lifeRatio;
			p.transform.scale = { currentScale, currentScale, currentScale };

			// 2. 描画データの作成 (最大数を超えていなければ)
			if ( group.kNumInstance < kNumMaxInstance ) {
				// 行列計算
				Matrix4x4 scaleMatrix = MatrixMath::MakeScale(p.transform.scale);
				Matrix4x4 translateMatrix = MatrixMath::MakeTranslate(p.transform.translate);

				// ビルボード処理（設定でONの時だけカメラを向く）
				Matrix4x4 rotateMatrix = group.setting.isBillboard ? billboardMatrix : MatrixMath::MakeIdentity4x4();

				// World = Scale * Rotate * Translate
				Matrix4x4 worldMatrix = MatrixMath::Multiply(scaleMatrix, MatrixMath::Multiply(rotateMatrix, translateMatrix));
				Matrix4x4 wvp = MatrixMath::Multiply(worldMatrix, viewProjection);

				// GPU用データに書き込む
				ParticleForGPU& data = group.instancingData[group.kNumInstance];
				data.WVP = wvp;
				data.World = worldMatrix;
				data.color = p.color;

				// 個数カウントアップ
				group.kNumInstance++;
			}

			++i; // 処理が終わったら次のパーティクルへ
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
		//  設定(group.setting)を渡して1粒作る
		Particle newParticle = MakeNewParticle(engine, position, group.setting);
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



Particle ParticleManager::MakeNewParticle(std::mt19937& engine, const Vector3& position, const ParticleSetting& setting){

	// 設定（setting）の数値を使ってランダムの幅を決める
	std::uniform_real_distribution<float> distPos(-0.5f, 0.5f); // 発生位置のバラつき
	std::uniform_real_distribution<float> distVelX(setting.minVelocity.x, setting.maxVelocity.x);
	std::uniform_real_distribution<float> distVelY(setting.minVelocity.y, setting.maxVelocity.y);
	std::uniform_real_distribution<float> distVelZ(setting.minVelocity.z, setting.maxVelocity.z);
	std::uniform_real_distribution<float> distTime(setting.minLifeTime, setting.maxLifeTime);

	Particle p;
	// 初期サイズを設定から持ってくる
	p.transform.scale = { setting.startScale, setting.startScale, setting.startScale };
	p.transform.rotate = { 0.0f, 0.0f, 0.0f };

	// ランダムな位置と速度を設定
	p.transform.translate = {
		position.x + distPos(engine),
		position.y + distPos(engine),
		position.z + distPos(engine)
	};
	// ランダムな速度と、設定した色を入れる
	p.velocity = { distVelX(engine), distVelY(engine), distVelZ(engine) };
	p.color = setting.startColor;
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


void ParticleManager::DrawDebugUI(){
#ifdef USE_IMGUI
	// 他のツールと同じ "インスペクター (詳細設定)" ウィンドウの中にまとめる
	if ( ImGui::Begin("インスペクター (詳細設定)") ) {
		if ( ImGui::CollapsingHeader("パーティクルエディタ (Particle Editor)", ImGuiTreeNodeFlags_DefaultOpen) ) {

			// もしグループが1つも無ければメッセージを出して終了
			if ( particleGroups_.empty() ) {
				ImGui::Text("パーティクルグループがありません。");
			} else {

				// 🌟 1. 編集するグループをドロップダウン（コンボボックス）で選ぶ
				static std::string selectedGroupName = particleGroups_.begin()->first;
				if ( ImGui::BeginCombo("編集グループ", selectedGroupName.c_str()) ) {
					for ( auto& pair : particleGroups_ ) {
						bool isSelected = ( selectedGroupName == pair.first );
						if ( ImGui::Selectable(pair.first.c_str(), isSelected) ) {
							selectedGroupName = pair.first;
						}
						if ( isSelected ) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}

				// 🌟 2. 選択されたグループの設定(setting)を直接書き換えるUI
				auto it = particleGroups_.find(selectedGroupName);
				if ( it != particleGroups_.end() ) {
					ParticleGroup& group = it->second;

					ImGui::Text("現在数 [ CPU: %zu / GPU: %u ]", group.particles.size(), group.kNumInstance);
					ImGui::Separator();

					// --- 動きの設定 ---
					ImGui::Text("【 動き (Movement) 】");
					ImGui::DragFloat3("最小速度", &group.setting.minVelocity.x, 0.01f);
					ImGui::DragFloat3("最大速度", &group.setting.maxVelocity.x, 0.01f);
					ImGui::DragFloat("重力", &group.setting.gravity, 0.001f);

					// --- 見た目の設定 ---
					ImGui::Spacing();
					ImGui::Text("【 見た目 (Appearance) 】");
					ImGui::ColorEdit4("初期色", &group.setting.startColor.x);
					ImGui::DragFloat("発生サイズ", &group.setting.startScale, 0.01f);
					ImGui::DragFloat("消滅サイズ", &group.setting.endScale, 0.01f);
					ImGui::Checkbox("ビルボード (常にカメラを向く)", &group.setting.isBillboard);

					// --- 寿命の設定 ---
					ImGui::Spacing();
					ImGui::Text("【 寿命 (Life Time) 】");
					ImGui::DragFloat("最短寿命(秒)", &group.setting.minLifeTime, 0.1f, 0.1f, 10.0f);
					ImGui::DragFloat("最長寿命(秒)", &group.setting.maxLifeTime, 0.1f, 0.1f, 10.0f);



					if ( ImGui::Button("設定を保存 (Save)") ) {
						Save();
					}
					ImGui::SameLine();
					if ( ImGui::Button("設定を読み込む (Load)") ) {
						Load();
					}

					ImGui::Separator();

					// 🌟 3. いじった設定ですぐに発生させて確認するボタン
					if ( ImGui::Button("テスト発生 (10個)") ) {
						// 画面の中央(0,0,0)あたりに発生させる
						Emit(selectedGroupName, { 0.0f, 0.0f, 0.0f }, 10);
					}
					ImGui::SameLine();
					if ( ImGui::Button("大量発生 (100個)") ) {
						Emit(selectedGroupName, { 0.0f, 0.0f, 0.0f }, 100);
					}
				}
			}
		}
	}
	ImGui::End();
#endif
}
// パーティクル設定のセッター
void ParticleManager::SetParticleSetting(const std::string& name, const ParticleSetting& setting){
	auto it = particleGroups_.find(name);
	if ( it != particleGroups_.end() ) {
		it->second.setting = setting;
	}
}


// 🌟 設定をJSONファイルに保存する
void ParticleManager::Save(){
	nlohmann::json root;

	for ( auto& [name, group] : particleGroups_ ) {
		nlohmann::json groupJson;

		// Vector3 や Vector4 は配列として保存する
		groupJson["minVelocity"] = { group.setting.minVelocity.x, group.setting.minVelocity.y, group.setting.minVelocity.z };
		groupJson["maxVelocity"] = { group.setting.maxVelocity.x, group.setting.maxVelocity.y, group.setting.maxVelocity.z };
		groupJson["startColor"] = { group.setting.startColor.x, group.setting.startColor.y, group.setting.startColor.z, group.setting.startColor.w };
		groupJson["gravity"] = group.setting.gravity;
		groupJson["minLifeTime"] = group.setting.minLifeTime;
		groupJson["maxLifeTime"] = group.setting.maxLifeTime;
		groupJson["startScale"] = group.setting.startScale;
		groupJson["endScale"] = group.setting.endScale;
		groupJson["isBillboard"] = group.setting.isBillboard;

		root[name] = groupJson;
	}

	// resourcesフォルダの中に保存する
	std::ofstream file("resources/ParticleSettings.json");
	if ( file.is_open() ) {
		// インデント（字下げ）を4マスにして見やすく保存
		file << std::setw(4) << root << std::endl;
	}
}

// 🌟 JSONファイルから設定を読み込む
void ParticleManager::Load(){
	std::ifstream file("resources/ParticleSettings.json");
	if ( !file.is_open() ) {
		return; // ファイルがなければ何もしない
	}

	nlohmann::json root;
	file >> root;

	for ( auto& [name, group] : particleGroups_ ) {
		// JSON内にこのグループの設定が保存されていれば読み込む
		if ( root.contains(name) ) {
			nlohmann::json& groupJson = root[name];

			if ( groupJson.contains("minVelocity") ) {
				group.setting.minVelocity.x = groupJson["minVelocity"][0];
				group.setting.minVelocity.y = groupJson["minVelocity"][1];
				group.setting.minVelocity.z = groupJson["minVelocity"][2];
			}
			if ( groupJson.contains("maxVelocity") ) {
				group.setting.maxVelocity.x = groupJson["maxVelocity"][0];
				group.setting.maxVelocity.y = groupJson["maxVelocity"][1];
				group.setting.maxVelocity.z = groupJson["maxVelocity"][2];
			}
			if ( groupJson.contains("startColor") ) {
				group.setting.startColor.x = groupJson["startColor"][0];
				group.setting.startColor.y = groupJson["startColor"][1];
				group.setting.startColor.z = groupJson["startColor"][2];
				group.setting.startColor.w = groupJson["startColor"][3];
			}
			if ( groupJson.contains("gravity") ) group.setting.gravity = groupJson["gravity"];
			if ( groupJson.contains("minLifeTime") ) group.setting.minLifeTime = groupJson["minLifeTime"];
			if ( groupJson.contains("maxLifeTime") ) group.setting.maxLifeTime = groupJson["maxLifeTime"];
			if ( groupJson.contains("startScale") ) group.setting.startScale = groupJson["startScale"];
			if ( groupJson.contains("endScale") ) group.setting.endScale = groupJson["endScale"];
			if ( groupJson.contains("isBillboard") ) group.setting.isBillboard = groupJson["isBillboard"];
		}
	}
}