#include "SceneFactory.h"
#include "TitleScene.h"
#include "GamePlayScene.h"
// シーン生成
std::unique_ptr<IScene> SceneFactory::CreateScene(const std::string& sceneName) {

    // 文字列に応じて生成するインスタンスを切り替える
    if (sceneName == "TITLE") {
        return std::make_unique<TitleScene>(); // TitleSceneクラスのインスタンスを生成
    }
    else if (sceneName == "GAMEPLAY") {
        return std::make_unique<GamePlayScene>(); // GamePlaySceneクラスのインスタンスを生成
    }

    return nullptr;
}
