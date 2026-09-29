#include "game/scene/GamePlayAssets.h"
#include "engine/audio/AudioManager.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/3d/model/Model.h"
#include "engine/particle/ParticleManager.h"

namespace {
    void LoadSounds(const std::string& bgmFile){
        // BGM と卵アクションのSE（投げ/命中/割れ。自前生成のプレースホルダ音源）
        AudioManager* audio = AudioManager::GetInstance();
        audio->LoadWave(bgmFile);
        audio->LoadWave("resources/se/eggThrow.wav");
        audio->LoadWave("resources/se/eggHit.wav");
        audio->LoadWave("resources/se/eggBreak.wav");
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

    void LoadEffectModels(ModelManager* modelManager){
        modelManager->CreateEggShellModel("eggShell", 0.3f); // 卵の殻の欠片（割れ演出用）
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
        loadTintedBlock("craftSpring",     "block_1x1x1_sponge.obj", { 0.5f, 1.0f, 0.55f, 1.0f }); // ジャンプ台（緑）
        loadTintedBlock("craftHatena",     "block_1x1x1_layer.obj",  { 1.0f, 0.8f, 0.15f, 1.0f }); // ？ブロック（金）
        loadTintedBlock("craftHatenaUsed", "block_1x1x1_layer.obj",  { 0.45f, 0.42f, 0.4f, 1.0f }); // 使用済み（灰）
        loadTintedBlock("craftCloud",      "block_1x1x1_layer.obj",  { 0.9f, 0.97f, 1.0f, 1.0f }); // すり抜け床（白）
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
        textures["skybox"]        = textureManager->LoadCube("resources/StandardCubeMap.dds");
    }
}

void GamePlayAssets::Load(const std::string& bgmFile, std::unordered_map<std::string, TextureData>& textures){
    LoadSounds(bgmFile);
    ModelManager* modelManager = ModelManager::GetInstance();
    LoadCharacterModels(modelManager);
    LoadEffectModels(modelManager);
    LoadBlockModels(modelManager);
    // パーティクルグループ
    //   ※ 卵の煙／殻の飛び散りは加算パーティクルだと明るい背景で見えないため、
    //      EggSystem 側で実体(Obj3d)の小球として描画する
    ParticleManager::GetInstance()->CreateParticleGroup("Circle", "resources/uvChecker.png");
    LoadTextures(textures);
}
