
#include "GPUParticleEmitter.h"
#include "GPUParticleManager.h"

// nlohmann/json でJSON読み書き
#include "externals/nlohmann/json.hpp"
#include <fstream>
#include <random>

using json = nlohmann::json;


// 毎フレーム更新
void GPUParticleEmitter::Update(float deltaTime){
	// 発生タイミング管理
    if ( !data_.active ){
        return;
    }

	// 1秒に emitRate 個出す → 1個あたりの発生間隔は 1/emitRate 秒
	emitTimer_ += deltaTime;

	// 発生間隔を計算
	float emitInterval = 1.0f / data_.emitRate;

	// 重力の強さをGPUParticleManagerにセット
    GPUParticleManager::GetInstance()->SetGravity(data_.gravityY);

    while ( emitTimer_ >= emitInterval ){
		emitTimer_ -= emitInterval; // タイマーを減らす（複数回発生する可能性があるのでwhile）

        Vector3 vel = {
            data_.velocity.x + RandomRange(-data_.velocitySpread, data_.velocitySpread),
            data_.velocity.y + RandomRange(-data_.velocitySpread, data_.velocitySpread),
            data_.velocity.z + RandomRange(-data_.velocitySpread, data_.velocitySpread),
        };
    
		// スケールもランダムにする
		float lifeTime = RandomRange(data_.lifeTimeMin, data_.lifeTimeMax);

		// 色は開始色と終了色の間でランダムにする
		float scale = RandomRange(data_.scaleMin, data_.scaleMax);

        
        // Emitする
		GPUParticleManager::GetInstance()->Emit(data_.position, vel, lifeTime, scale, data_.startColor); 

    }


}

void GPUParticleEmitter::Burst(){
    for ( int i = 0; i < data_.burstCount; i++ ) {
        Vector3 vel = {
            data_.velocity.x + RandomRange(-data_.velocitySpread, data_.velocitySpread),
            data_.velocity.y + RandomRange(-data_.velocitySpread, data_.velocitySpread),
            data_.velocity.z + RandomRange(-data_.velocitySpread, data_.velocitySpread),
        };
        float lifeTime = RandomRange(data_.lifeTimeMin, data_.lifeTimeMax);
        float scale = RandomRange(data_.scaleMin, data_.scaleMax);

        GPUParticleManager::GetInstance()->Emit(
            data_.position, vel, lifeTime, scale, data_.startColor);
    }
}

// ここで発生タイミングを管理して、GPUParticleManagerにEmitする
// 乱数生成 (min～maxの範囲で)
float GPUParticleEmitter::RandomRange(float min, float max){ 
    static std::mt19937 rng(std::random_device {}( ));
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}

// 毎フレーム更新
void GPUParticleEmitter::SetData(const GPUParticleEmitterData& data){
    data_ = data;
    emitTimer_ = 0.0f; // タイマーもリセット
}


// -------------------------------------------------------
// JSONに保存
// -------------------------------------------------------
void GPUParticleEmitter::SaveToFile(const std::string& filePath){
    json j;
    j["name"] = data_.name;
    j["position"] = { data_.position.x,  data_.position.y,  data_.position.z };
    j["emitRate"] = data_.emitRate;
    j["loop"] = data_.loop;
    j["active"] = data_.active;
    j["velocity"] = { data_.velocity.x,  data_.velocity.y,  data_.velocity.z };
    j["velocitySpeed"] = data_.velocitySpread;
    j["lifeTimeMin"] = data_.lifeTimeMin;
    j["lifeTimeMax"] = data_.lifeTimeMax;
    j["scaleMin"] = data_.scaleMin;
    j["scaleMax"] = data_.scaleMax;
    j["startColor"] = { data_.startColor.x, data_.startColor.y, data_.startColor.z, data_.startColor.w };
    j["endColor"] = { data_.endColor.x,   data_.endColor.y,   data_.endColor.z,   data_.endColor.w };
    j["gravityY"] = data_.gravityY;
    j["burstCount"] = data_.burstCount;

    std::ofstream file(filePath);
    if ( file.is_open() ) {
        file << j.dump(4); // 4スペースでインデントして見やすく保存
    }
}

// -------------------------------------------------------
// JSONから読み込み
// -------------------------------------------------------
void GPUParticleEmitter::LoadFromFile(const std::string& filePath){
    std::ifstream file(filePath);
    if ( !file.is_open() ) { return; } // ファイルがなければ何もしない

    json j;
    file >> j;

    data_.name = j.value("name", "emitter");
    data_.emitRate = j.value("emitRate", 10.0f);
    data_.loop = j.value("loop", true);
    data_.active = j.value("active", true);
    data_.velocitySpread = j.value("velocitySpeed", 0.0f);

    data_.lifeTimeMin = j.value("lifeTimeMin", 1.0f);
    data_.lifeTimeMax = j.value("lifeTimeMax", 2.0f);
    data_.scaleMin = j.value("scaleMin", 0.1f);
    data_.scaleMax = j.value("scaleMax", 0.3f);
    data_.gravityY = j.value("gravityY", -0.098f);
    data_.burstCount = j.value("burstCount", 10);

    if ( j.contains("position") ) {
        data_.position = { j["position"][0], j["position"][1], j["position"][2] };
    }
    if ( j.contains("velocity") ) {
        data_.velocity = { j["velocity"][0], j["velocity"][1], j["velocity"][2] };
    }
    if ( j.contains("startColor") ) {
        data_.startColor = { j["startColor"][0], j["startColor"][1], j["startColor"][2], j["startColor"][3] };
    }
    if ( j.contains("endColor") ) {
        data_.endColor = { j["endColor"][0], j["endColor"][1], j["endColor"][2], j["endColor"][3] };
    }
}