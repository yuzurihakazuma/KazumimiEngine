#pragma once
// =====================================================================
//  TransitionTimeline：遷移の演出を「部品の並び」として、時刻と出来事で進める。
//   部品はそれぞれ「きっかけの出来事（event）」「そこから何秒後（delay）」「長さ（duration）」を持つ。
//   出来事は "start"（再生を始めた時）のほか、演出の側が Fire で知らせる
//   （卵が扉に当たった "doorHit"、次のシーンを読み終えた "loaded" など）。
//   時間だけで並べると、卵の速さを変えた時にずれるので、きっかけは出来事で待てるようにしている
// =====================================================================
#include "externals/nlohmann/json.hpp"

#include <map>
#include <string>
#include <vector>

class TransitionTimeline {
public:
	struct Part {
		std::string name;              // 表示名（エディタ用）
		std::string type;              // 部品の種類（KamishibaiTransition が見る）
		std::string event = "start";   // 始まるきっかけの出来事
		float delay = 0.0f;            // 出来事から何秒後に始まるか
		float duration = 0.0f;         // 長さ（0 なら始まった瞬間に終わる）
		nlohmann::json params = nlohmann::json::object(); // 部品ごとの設定

		// --- 再生中の状態 ---
		bool  started = false;
		bool  finished = false;
		bool  justStarted = false;     // このフレームで始まった
		bool  justFinished = false;    // このフレームで終わった
		float startTime = 0.0f;
		float progress = 0.0f;         // 0〜1
		float elapsed = 0.0f;          // 始まってからの秒数
	};

	void Load(const nlohmann::json& parts);
	nlohmann::json Save() const;

	void Start();                            // 最初から再生（"start" を知らせる）
	void Fire(const std::string& event);     // 出来事を知らせる（最初の1回の時刻だけ覚える）
	bool HasFired(const std::string& event) const{ return events_.count(event) != 0; }
	void Update(float deltaTime);

	float GetTime() const{ return time_; }
	std::vector<Part>& Parts(){ return parts_; }
	const std::vector<Part>& Parts() const{ return parts_; }

private:
	std::vector<Part> parts_;
	std::map<std::string, float> events_; // 出来事の名前 → 起きた時刻
	float time_ = 0.0f;
};
