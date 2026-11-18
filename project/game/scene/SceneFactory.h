#pragma once

#include "engine/scene/AbstractSceneFactory.h"
#include <string>
#include <memory>
// シーン生成のための具象クラス
class SceneFactory : public KazumimiEngine::AbstractSceneFactory {
public:
	
	// シーン生成
	std::unique_ptr<KazumimiEngine::IScene> CreateScene(const std::string& sceneName) override;


};