#include "SceneFactory.h"
#include "TitleScene.h"
#include "GamePlayScene.h"
#include "GameOverScene.h"
#include "GameClearScene.h"

#include "AnimationEditorScene.h"
// シーン生成
std::unique_ptr<IScene> SceneFactory::CreateScene(const std::string& sceneName) {

    // 文字列に応じて生成するインスタンスを切り替える
    if (sceneName == "TITLE") {
        return std::make_unique<TitleScene>(); // TitleSceneクラスのインスタンスを生成
    }
    else if (sceneName == "GAMEPLAY") {
        return std::make_unique<GamePlayScene>(); // GamePlaySceneクラスのインスタンスを生成
    } else if (sceneName == "GAMEOVER") {
        return std::make_unique<GameOverScene>();
    } else if (sceneName == "GAMECLEAR") {
        return std::make_unique<GameClearScene>();
    }
	else if (sceneName == "ANIMATION_EDITOR") { // AnimationEditorSceneクラスのインスタンスを生成
        return std::make_unique<AnimationEditorScene>();
    }
    return nullptr;
}
