#pragma once

#include "Engine/Scene/AbstractSceneFactory.h"
#include <string>
#include <memory>
// シーン生成のための具象クラス
class SceneFactory : public AbstractSceneFactory {
public:
	
	// シーン生成
	std::unique_ptr<IScene> CreateScene(const std::string& sceneName) override;


};