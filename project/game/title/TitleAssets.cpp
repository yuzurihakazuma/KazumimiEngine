#include "game/title/TitleAssets.h"

#include "engine/3d/model/Model.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/audio/AudioManager.h"
#include "engine/graphics/TextureManager.h"
#include "engine/math/Matrix4x4.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

namespace {
    const char* kKitModelDir = "resources/craft/models";

    // 紙・フェルト・段ボールはつやが無いので、光のハイライト（白い照り返し）を出さない
    void MakeMatte(Model* model){
        if ( model && model->GetMaterial() ) { model->GetMaterial()->matte = 1.0f; }
    }

    // クラフトキットのモデルを、フォルダにある分だけ全部読む（キットを作り直して物が増えても、ここは直さなくてよい）
    void LoadKitModels(ModelManager* modelManager){
        namespace fs = std::filesystem;
        std::error_code errorCode;
        for ( const auto& entry : fs::directory_iterator(kKitModelDir, errorCode) ) {
            if ( !entry.is_regular_file() ) { continue; }
            std::string extension = entry.path().extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                [](unsigned char character){ return ( char ) std::tolower(character); });
            if ( extension != ".gltf" ) { continue; }
            const std::string modelName = entry.path().stem().string();
            modelManager->LoadModel(modelName, kKitModelDir, entry.path().filename().string(), true);
            MakeMatte(modelManager->FindModel(modelName));
        }
    }

    // 同じモデルを別名で読み、指定した面（マテリアル）の画像だけ差し替える
    void LoadWithFace(ModelManager* modelManager, const char* modelName, const char* file,
                      const char* faceMaterial, const char* faceTexture){
        modelManager->LoadModel(modelName, kKitModelDir, file, true);
        if ( Model* model = modelManager->FindModel(modelName) ) {
            model->SetMaterialTexture(faceMaterial, faceTexture);
            MakeMatte(model);
        }
    }

    // 机の天板：1m角の箱にクラフト紙を繰り返し貼る（マップ側で大きく引き伸ばして置く）
    void CreateDesk(ModelManager* modelManager){
        modelManager->CreateCubeModel(TitleAssets::kDeskModel, 1.0f);
        Model* desk = modelManager->FindModel(TitleAssets::kDeskModel);
        if ( !desk ) { return; }
        desk->SetTexture("resources/craft/textures/kraft.png");
        desk->SetTiledUV(true);
        MakeMatte(desk);
        if ( desk->GetMaterial() ) {
            desk->GetMaterial()->uvTransform = MatrixMath::MakeScale({ 26.0f, 18.0f, 1.0f });
            desk->GetMaterial()->color = { 0.93f, 0.84f, 0.70f, 1.0f }; // 地面の段ボールより少し明るい木の色
        }
    }

    // 紙ふぶきの1枚：1m角の板に色紙を貼る（大きさは使う側が縮めて決める）
    void CreatePaperBits(ModelManager* modelManager){
        const char* kTextures[TitleAssets::kPaperBitModelCount] = {
            "resources/craft/textures/paper_white.png",
            "resources/craft/textures/paper_yellow.png",
            "resources/craft/textures/paper_orange.png",
            "resources/craft/textures/paper_grass2.png",
        };
        for ( int i = 0; i < TitleAssets::kPaperBitModelCount; ++i ) {
            modelManager->CreatePlaneModel(TitleAssets::kPaperBitModels[i], 1.0f, 1.0f);
            if ( Model* model = modelManager->FindModel(TitleAssets::kPaperBitModels[i]) ) {
                model->SetTexture(kTextures[i]);
                // 光の当たり方で暗くならないよう、紙の色をそのまま出す（回っても黒い点に見えない）
                if ( model->GetMaterial() ) { model->GetMaterial()->enableLighting = false; }
            }
        }
    }

    // 本編と同じレール・道の素材（RailField の緑線と RoadMesh が名前で引く）
    void LoadRailAndRoad(ModelManager* modelManager){
        modelManager->LoadModel("roadJoint", "resources/road", "road_joint.obj");
        modelManager->CreateCubeModel("railLineCube", 1.0f);
        modelManager->CreateCubeModel("railLineCubeHole", 1.0f);
        TextureManager* textureManager = TextureManager::GetInstance();
        textureManager->Load("resources/road/road_atlas.png");
        textureManager->Load("resources/block/white1x1.png");
    }
}

void TitleAssets::Load(){
    ModelManager* modelManager = ModelManager::GetInstance();
    LoadKitModels(modelManager);
    CreatePaperBits(modelManager);
    LoadRailAndRoad(modelManager);

    LoadWithFace(modelManager, kLogoBoardModel, "title_board.gltf", "M_logo_face", "resources/title/logo.png");
    LoadWithFace(modelManager, kMenuModels[0], "menu_target.gltf", "M_menu_face", "resources/title/menu_continue.png");
    LoadWithFace(modelManager, kMenuModels[1], "menu_target.gltf", "M_menu_face", "resources/title/menu_start.png");
    LoadWithFace(modelManager, kMenuModels[2], "menu_target.gltf", "M_menu_face", "resources/title/menu_options.png");
    CreateDesk(modelManager);

    // 恐竜（プレイヤーと同じモデル）と、決定時に投げる卵
    modelManager->LoadModel("player", "resources/player", "player.gltf");
    modelManager->LoadModel("egg", "resources/egg", "egg.obj");

    AudioManager* audio = AudioManager::GetInstance();
    audio->LoadWave(kThrowSe);
    audio->LoadWave(kHitSe);
}
