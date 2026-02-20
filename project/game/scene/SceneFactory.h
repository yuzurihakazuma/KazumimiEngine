#pragma once

#include<engine/scene/AbstractSceneFactory.h>
// シーン生成のための具象クラス
class SceneFactory : public AbstractSceneFactory {
public:
	
	// シーン生成
	IScene* CreateScene(const std::string& sceneName) override; 


};