#include "game/scene/GamePlayAssets.h"
#include "engine/utils/JsonFile.h"
#include "engine/audio/AudioManager.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/3d/model/Model.h"
#include "engine/particle/ParticleManager.h"
#include <utility>
#include "game/stage/StageBackdrop.h"
#include "externals/nlohmann/json.hpp"

using namespace KazumimiEngine;


namespace {
    void LoadSounds(const std::string& bgmFile){
        // BGM と卵アクションのSE（投げ/命中/割れ。自前生成のプレースホルダ音源）
        AudioManager* audio = AudioManager::GetInstance();
        audio->LoadWave(bgmFile);
        audio->LoadWave("resources/se/eggThrow.wav");
        audio->LoadWave("resources/se/eggHit.wav");
        audio->LoadWave("resources/se/eggBreak.wav");
        audio->LoadWave("resources/se/se_spring.wav"); // ジャンプ台で跳ねた音
        audio->LoadWave("resources/kamishibai/se_pon.wav"); // かくれ花が飛び出した音
        audio->LoadWave("resources/kamishibai/se_snip.wav"); // 背景の物がはじける音
    }

    void LoadCharacterModels(ModelManager* modelManager){
        modelManager->LoadModel("fence", "resources", "fence.obj");
        modelManager->LoadModel("grass", "resources", "terrain.obj");
        modelManager->LoadModel("block", "resources/block", "block.obj");
        modelManager->CreateSphereModel("sphere", 16);
        modelManager->CreatePlaneModel("plane");
        modelManager->LoadModel("egg", "resources/egg", "egg.obj"); // ヨッシーの卵（専用モデル）
        modelManager->LoadModel("player", "resources/player", "player.gltf"); // プレイヤー（リグ付きマスコット。7色パレット焼き込み済み）
        // 敵キャラ3種（リグ+クリップ入りglb。プレイヤーと同じトイ風の公式デザイン）
        modelManager->LoadModel("enemyGround", "resources/enemy", "enemy_ground.glb"); // 地上「ドングリン」(Idle/Walk)
        modelManager->LoadModel("enemyAir",    "resources/enemy", "enemy_air.glb");    // 空中「フワリン」(Fly)
        modelManager->LoadModel("enemyPlant",  "resources/enemy", "enemy_plant.glb");  // 植物「カミバナ」(Idle/Bite)
        modelManager->LoadModel("roadJoint", "resources/road", "road_joint.obj"); // 接続ノードの凸ジョイント（プラレール風）
    }

    // ステージ開始の演技：巻いた布のロールと、頭の上のキラキラ（パーティクル）。背景はクラフトのキット
    void LoadStageIntroAssets(ModelManager* modelManager){
        modelManager->LoadModel("introRoll", "resources/stage_intro/roll", "Roll_Felt.obj", true);
        if ( Model* roll = modelManager->FindModel("introRoll") ) {
            if ( roll->GetMaterial() ) { roll->GetMaterial()->matte = 1.0f; } // フェルトなので照り返しなし
        }
        StageBackdrop::LoadKitModels();
        ParticleManager* particles = ParticleManager::GetInstance();
        particles->CreateParticleGroup("IntroSparkleY", "resources/stage_intro/fx/fx_sparkle_yellow.png");
        particles->CreateParticleGroup("IntroSparkleP", "resources/stage_intro/fx/fx_sparkle_pink.png");
        // 設定は resources/ParticleSettings.json から（このグループの分だけ読む。速さは1フレームあたり）
        ParticleSetting sparkle;
        sparkle.minVelocity = { -0.01f, 0.01f, -0.01f };
        sparkle.maxVelocity = { 0.01f, 0.025f, 0.01f };
        sparkle.gravity = -0.0004f;
        sparkle.minLifeTime = 0.5f;
        sparkle.maxLifeTime = 0.8f;
        sparkle.startScale = 0.25f;
        sparkle.endScale = 0.0f;
        sparkle.isBillboard = true;
        nlohmann::json settings;
        JsonFile::Load("resources/ParticleSettings.json", settings); // 無い・読めない時は空
        for ( const char* name : { "IntroSparkleY", "IntroSparkleP" } ) {
            ParticleSetting setting = sparkle;
            if ( settings.contains(name) ) {
                const nlohmann::json& entry = settings[name];
                auto vector3 = [&](const char* key, Vector3& out){
                    if ( entry.contains(key) ) { out = { entry[key][0], entry[key][1], entry[key][2] }; }
                };
                vector3("minVelocity", setting.minVelocity);
                vector3("maxVelocity", setting.maxVelocity);
                if ( entry.contains("startColor") ) {
                    setting.startColor = { entry["startColor"][0], entry["startColor"][1], entry["startColor"][2], entry["startColor"][3] };
                }
                setting.gravity = entry.value("gravity", setting.gravity);
                setting.minLifeTime = entry.value("minLifeTime", setting.minLifeTime);
                setting.maxLifeTime = entry.value("maxLifeTime", setting.maxLifeTime);
                setting.startScale = entry.value("startScale", setting.startScale);
                setting.endScale = entry.value("endScale", setting.endScale);
                setting.isBillboard = entry.value("isBillboard", setting.isBillboard);
            }
            particles->SetParticleSetting(name, setting);
        }
    }

    void LoadEffectModels(ModelManager* modelManager){
        modelManager->CreateEggShellModel("eggShell", 0.3f); // 卵の殻の欠片（割れ演出用）
        // 緑の殻（卵の水玉の色）。白と混ぜて出すと、割れたのがあの卵だと分かる
        modelManager->CreateEggShellModel("eggShellGreen", 0.3f);
        if ( auto* shell = modelManager->FindModel("eggShellGreen") ) {
            if ( shell->GetMaterial() ) { shell->GetMaterial()->color = { 0.08f, 0.43f, 0.05f, 1.0f } /* 卵のテクスチャの緑（sRGB 80,175,60）をリニアにした値 */; }
        }
        // 汎用パーティクル用の粒（"sphere" は敵と共有＋モンスターボール柄がデフォルトなので、
        //   色を付けるだけの粒には専用の白い球を使う）
        modelManager->CreateSphereModel("fxSphere", 8);
        if ( auto* fxModel = modelManager->FindModel("fxSphere") ) {
            fxModel->SetTexture("resources/block/white1x1.png");
        }
        // 収集物（コイン）用の金色の球
        modelManager->CreateSphereModel("coin", 12);
        if ( auto* coinModel = modelManager->FindModel("coin") ) {
            coinModel->SetTexture("resources/block/white1x1.png");
            if ( coinModel->GetMaterial() ) { coinModel->GetMaterial()->color = { 1.0f, 0.85f, 0.2f, 1.0f }; }
        }
        // レール可視化用モデル（通常=緑 / 穴=赤 の2モデル。マテリアルはモデル単位で共有のため別モデルが必要）
        modelManager->CreateCubeModel("railLineCube", 1.0f);
        modelManager->CreateCubeModel("railLineCubeHole", 1.0f);
    }

    // クリア演出：卵の殻のゴールの輪（上のふた・下のうつわ。tools/clear/make_clear_assets.py）。
    //   切り文字・星の板は GoalProps が、リザルト画面の絵は ResultScreen が自分で用意する
    // クリア演出：ゴールの門・立体の切り文字・棒付きの星（tools/clear/make_goal_set.py が Blender で作る。テクスチャはクラフトの共有）
    void LoadClearModels(ModelManager* modelManager){
        const std::pair<const char*, const char*> models[] = {
            { "goalGate", "goal_gate.gltf" }, { "goalEggBottom", "goal_egg_bottom.gltf" }, { "goalEggTop", "goal_egg_top.gltf" },
            { "goalTapeLeft", "goal_tape_left.gltf" }, { "goalTapeRight", "goal_tape_right.gltf" }, { "goalString", "goal_string.gltf" },
            { "goalLetter0", "goal_letter_0.gltf" }, { "goalLetter1", "goal_letter_1.gltf" },
            { "goalLetter2", "goal_letter_2.gltf" }, { "goalLetter3", "goal_letter_3.gltf" },
            { "goalStar0", "goal_star_0.gltf" }, { "goalStar1", "goal_star_1.gltf" }, { "goalStar2", "goal_star_2.gltf" },
        };
        for ( const auto& [name, file] : models ) {
            modelManager->LoadModel(name, "resources/craft/models", file, true);
            if ( Model* model = modelManager->FindModel(name) ) {
                if ( model->GetMaterial() ) { model->GetMaterial()->matte = 1.0f; } // 紙と段ボールなので照り返しなし
            }
        }
    }

    void LoadBlockModels(ModelManager* modelManager){
        // クラフトブロック一式（全モデルが block_atlas.png 1枚を共有。原点=底面中心・実寸1m角）。
        //   keepOrigin=true：ローダーの重心センタリングを止めてファイルの底面原点を維持する
        //   （センタリングされると描画だけ半分沈む＝長年の「ブロックが浮く/埋まる」の根本原因だった）
        modelManager->LoadModel("craftSponge",  "resources/block/craft", "block_1x1x1_sponge.obj", true);
        modelManager->LoadModel("craftLayer",   "resources/block/craft", "block_1x1x1_layer.obj", true);
        modelManager->LoadModel("craftSlope45", "resources/block/craft", "slope_1x1_rolls.obj", true);
        modelManager->LoadModel("craftSlope26", "resources/block/craft", "slope_2x1_rolls.obj", true);
        modelManager->LoadModel("flowerOrange", "resources/block/craft", "flower_orange.obj", true);
        modelManager->LoadModel("flowerWhite",  "resources/block/craft", "flower_white_face.obj", true);
        // 性質つきブロック：既存モデルを別名で読み、着色して見分ける
        auto loadTintedBlock = [&](const char* name, const char* file, const Vector4& color){
            modelManager->LoadModel(name, "resources/block/craft", file, true);
            if ( auto* model = modelManager->FindModel(name) ) {
                if ( model->GetMaterial() ) { model->GetMaterial()->color = color; }
            }
        };
        // ジャンプ台：工作のばね（段ボールの土台・白い紙のコイル・緑のフェルトの板。tools/craftkit/make_spring_block.py）
        modelManager->LoadModel("craftSpring", "resources/block/craft", "spring_1x1x1.obj", true);
        loadTintedBlock("craftHatena",     "block_1x1x1_layer.obj",  { 1.0f, 0.8f, 0.15f, 1.0f }); // ？ブロック（金）
        loadTintedBlock("craftHatenaUsed", "block_1x1x1_layer.obj",  { 0.45f, 0.42f, 0.4f, 1.0f }); // 使用済み（灰）
        // すり抜け床：あみあみの床（黄色い枠に白い紙の帯を編んだ網。マスの上の端にだけある。tools/craftkit/make_net_block.py）
        modelManager->LoadModel("craftCloud", "resources/block/craft", "net_1x1x1.obj", true);
        // 大型ブロック（road_system_4）：横長2m（進行方向）と2×2m台座
        modelManager->LoadModel("craftWide",     "resources/block/craft", "block_2x1x1_sponge.obj", true);
        modelManager->LoadModel("craftPedestal", "resources/block/craft", "block_2x2x1_layer.obj", true);
    }

    void LoadTextures(std::unordered_map<std::string, TextureData>& textures){
        TextureManager* textureManager = TextureManager::GetInstance();
        textures["uvChecker"]     = textureManager->Load("resources/uvChecker.png");
        textures["monsterBall"]   = textureManager->Load("resources/monsterBall.png");
        textures["fence"]         = textureManager->Load("resources/fence.png");
        textures["circle"]        = textureManager->Load("resources/circle.png");
        textures["circle2"]       = textureManager->Load("resources/circle2.png");
        textures["noise0"]        = textureManager->Load("Resources/noise0.png");
        textures["noise1"]        = textureManager->Load("Resources/noise1.png");
        textures["gradationLine"] = textureManager->Load("Resources/gradationLine.png");
        textures["white"]         = textureManager->Load("resources/block/white1x1.png");
        // 道アトラスの先読み：RoadMesh はレール編集のたび（=フレーム途中）に参照するので、
        // ここでキャッシュに載せておく
        textures["roadAtlas"]     = textureManager->Load("resources/road/road_atlas.png");
        // 布ロードの布（同じく道の作り直しの時に参照する）
        textures["clothFelt"]      = textureManager->Load("resources/road/cloth/cloth_felt.png");
        textures["clothFeltPlain"] = textureManager->Load("resources/road/cloth/cloth_felt_plain.png");
        textures["skybox"]        = textureManager->LoadCube("resources/StandardCubeMap.dds");
    }
}

void GamePlayAssets::LoadForDiorama(){
    ModelManager* modelManager = ModelManager::GetInstance();
    LoadCharacterModels(modelManager); // 敵・道のジョイント
    LoadBlockModels(modelManager);
    StageBackdrop::LoadKitModels();
    TextureManager* textureManager = TextureManager::GetInstance();
    textureManager->Load("resources/road/road_atlas.png");
    textureManager->Load("resources/road/cloth/cloth_felt.png");
    textureManager->Load("resources/road/cloth/cloth_felt_plain.png");
}

void GamePlayAssets::Load(const std::string& bgmFile, std::unordered_map<std::string, TextureData>& textures){
    LoadSounds(bgmFile);
    ModelManager* modelManager = ModelManager::GetInstance();
    LoadCharacterModels(modelManager);
    LoadEffectModels(modelManager);
    LoadStageIntroAssets(modelManager);
    LoadClearModels(modelManager);
    LoadBlockModels(modelManager);
    // パーティクルグループ
    //   ※ 卵の煙／殻の飛び散りは加算パーティクルだと明るい背景で見えないため、
    //      EggSystem 側で実体(Obj3d)の小球として描画する
    ParticleManager::GetInstance()->CreateParticleGroup("Circle", "resources/uvChecker.png");
    LoadTextures(textures);
}
