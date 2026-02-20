#include "SceneFactory.h"
#include <engine/scene/TitleScene.h>
#include <engine/scene/GamePlayScene.h>
// シーン生成
IScene* SceneFactory::CreateScene(const std::string& sceneName) {
	// 生成するシーンのポインタ
    IScene* newScene = nullptr;

    // 文字列に応じて生成するインスタンスを切り替える
    if (sceneName == "TITLE") {
        newScene = new TitleScene(); // TitleSceneクラスのインスタンスを生成
    }
    else if (sceneName == "GAMEPLAY") {
        newScene = new GamePlayScene(); // GamePlaySceneクラスのインスタンスを生成
    }

    return newScene;
}
