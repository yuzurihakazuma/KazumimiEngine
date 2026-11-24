#pragma once
#include "GPUParticleEmitterData.h"

#include <string>

namespace KazumimiEngine {

// GPUパーティクルエミッタクラス
class GPUParticleEmitter{
public:

		// 毎フレーム更新 (GPUParticleManagerにEmitする)
	void Update(float deltaTime);

	// 一気に大量発生させる（エディタの「Burst」ボタンから呼ぶ）
	void Burst();

	// データをファイルに保存
	void SaveToFile(const std::string& filePath);

	// データをファイルに保存
	void LoadFromFile(const std::string& filePath);


	// データのセット
	void SetData(const GPUParticleEmitterData& data);

	// データの取得
	const GPUParticleEmitterData& GetData() const{ return data_; }
	// 調整中の書き戻し（SetData と違い、発生のタイマーは戻さない）
	void UpdateData(const GPUParticleEmitterData& data){ data_ = data; }
	void SetEmitRate(float emitRate){ data_.emitRate = emitRate; }
	void SetGravityY(float gravityY){ data_.gravityY = gravityY; }



private:

	GPUParticleEmitterData data_;

	float emitTimer_ = 0.0f; // 発生タイミング管理用タイマー

	// 乱数生成 (min～maxの範囲で)
	float RandomRange(float min, float max);


};

} // namespace KazumimiEngine
