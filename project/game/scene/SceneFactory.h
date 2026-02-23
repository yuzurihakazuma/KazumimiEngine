#pragma once

#include "Engine/Scene/AbstractSceneFactory.h"
#include <string>

// シーン生成のための具象クラス
class SceneFactory : public AbstractSceneFactory {
public:
	
	// シーン生成
	IScene* CreateScene(const std::string& sceneName) override; 


};