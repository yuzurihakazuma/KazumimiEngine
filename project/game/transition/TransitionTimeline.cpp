#include "game/transition/TransitionTimeline.h"

#include <algorithm>

using json = nlohmann::json;

void TransitionTimeline::Load(const json& parts){
	parts_.clear();
	if ( !parts.is_array() ) { return; }
	for ( const json& entry : parts ) {
		Part part;
		part.name = entry.value("name", "");
		part.type = entry.value("type", "");
		part.event = entry.value("event", "start");
		part.delay = entry.value("delay", 0.0f);
		part.duration = entry.value("duration", 0.0f);
		if ( entry.contains("params") && entry["params"].is_object() ) { part.params = entry["params"]; }
		parts_.push_back(part);
	}
}

json TransitionTimeline::Save() const{
	json parts = json::array();
	for ( const Part& part : parts_ ) {
		json entry = json::object();
		entry["name"] = part.name;
		entry["type"] = part.type;
		entry["event"] = part.event;
		entry["delay"] = part.delay;
		entry["duration"] = part.duration;
		if ( !part.params.empty() ) { entry["params"] = part.params; }
		parts.push_back(entry);
	}
	return parts;
}

void TransitionTimeline::Start(){
	time_ = 0.0f;
	events_.clear();
	for ( Part& part : parts_ ) {
		part.started = part.finished = part.justStarted = part.justFinished = false;
		part.startTime = part.progress = part.elapsed = 0.0f;
	}
	Fire("start");
}

void TransitionTimeline::Fire(const std::string& event){
	events_.emplace(event, time_); // 2回目以降は無視（最初に起きた時刻が基準）
}

void TransitionTimeline::Update(float deltaTime){
	time_ += deltaTime;
	for ( Part& part : parts_ ) {
		part.justStarted = false;
		part.justFinished = false;
		if ( part.finished ) { continue; }
		if ( !part.started ) {
			const auto found = events_.find(part.event);
			if ( found == events_.end() || time_ < found->second + part.delay ) { continue; }
			part.started = true;
			part.justStarted = true;
			part.startTime = found->second + part.delay;
		}
		part.elapsed = time_ - part.startTime;
		part.progress = ( part.duration > 1e-4f ) ? std::clamp(part.elapsed / part.duration, 0.0f, 1.0f) : 1.0f;
		if ( part.progress >= 1.0f ) {
			part.finished = true;
			part.justFinished = true;
		}
	}
}
