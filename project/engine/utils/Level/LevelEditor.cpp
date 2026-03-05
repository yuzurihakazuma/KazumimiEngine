#include "LevelEditor.h"

/// --- 標準ライブラリ ---
#include <cmath>

// --- エンジン側のファイル ---
#include "LevelManager.h"
#include "engine/3d/obj/Obj3d.h"
#include "engine/utils/ImGuiManager.h"
#include "engine/3d/model/ModelManager.h"


LevelEditor::LevelEditor() = default;
LevelEditor::~LevelEditor() = default;


void LevelEditor::Initialize(){
    // 最初は空の状態でスタートするか、デフォルトのマップを読み込む
    LoadAndCreateMap("resources/map/map01.json");
}
// マップの読み込みと生成
void LevelEditor::LoadAndCreateMap(const std::string& fileName){
    object3ds_.clear();
    levelData_ = LevelManager::GetInstance()->Load(fileName);

    for ( auto& objData : levelData_.objects ) {
        // 実際のモデルデータを検索して持ってくる
        Model* model = ModelManager::GetInstance()->FindModel(objData.type);

        std::unique_ptr<Obj3d> newObj = std::make_unique<Obj3d>();
        // 検索したモデルを渡す！
        newObj->Initialize(model);
        newObj->SetCamera(camera_);
        newObj->SetTranslation(objData.translation);
        newObj->SetRotation(objData.rotation);
        newObj->SetScale(objData.scale);
        newObj->Update();
        object3ds_.push_back(std::move(newObj));
    }
    selectedObjectIndex_ = -1;
}


void LevelEditor::Update() {
#ifdef USE_IMGUI
    ImGui::Begin("Level Editor");

    // =========================================================
    // 📁 1. ファイル操作（折りたたみメニュー）
    // =========================================================
    if (ImGui::CollapsingHeader("File Operations", ImGuiTreeNodeFlags_DefaultOpen)) {
        char buffer[256];
        strcpy_s(buffer, saveFileName_.c_str());
        if (ImGui::InputText("File Name (.json)", buffer, sizeof(buffer))) {
            saveFileName_ = buffer;
        }

        std::string fullPath = "resources/map/" + saveFileName_;

        if (ImGui::Button("Save map")) {
            LevelManager::GetInstance()->Save(fullPath, levelData_);
        }
        ImGui::SameLine();
        if (ImGui::Button("Load map")) {
            LoadAndCreateMap(fullPath);
        }
        ImGui::SameLine();
        // ★新規追加：画面を一旦まっさらにするリセットボタン
        if (ImGui::Button("Clear Map")) {
            object3ds_.clear();
            levelData_.objects.clear();
            selectedObjectIndex_ = -1;
        }
    }

    // =========================================================
    // ➕ 2. オブジェクトの追加（折りたたみメニュー）
    // =========================================================
    if (ImGui::CollapsingHeader("Add Object", ImGuiTreeNodeFlags_DefaultOpen)) {
        // ★新規追加：追加できるモデルのリスト
        const char* modelNames[] = { "block", "fence", "plane", "sphere", "terrain", "axis" };
        static int currentModelIndex = 0; // 現在選択されているモデルの番号

        // コンボボックス（ドロップダウン）でモデルを選ぶ
        ImGui::Combo("Model Type", &currentModelIndex, modelNames, IM_ARRAYSIZE(modelNames));

        // 選んだモデルを追加するボタン
        if (ImGui::Button("Add Selected Object")) {
            LevelObjectData newObj;
            newObj.type = modelNames[currentModelIndex]; // コンボボックスで選んだ名前を入れる
            newObj.translation = { 0.0f, 0.0f, 0.0f };
            newObj.rotation = { 0.0f, 0.0f, 0.0f };
            newObj.scale = { 1.0f, 1.0f, 1.0f };
            levelData_.objects.push_back(newObj);

            Model* model = ModelManager::GetInstance()->FindModel(newObj.type);
            if (model != nullptr) {
                std::unique_ptr<Obj3d> obj = std::make_unique<Obj3d>();
                obj->Initialize(model);
                obj->SetCamera(camera_);

                object3ds_.push_back(std::move(obj));
                selectedObjectIndex_ = (int)levelData_.objects.size() - 1;
            }
        }
    }

    // =========================================================
    // 📋 3. リスト表示と編集（折りたたみメニュー）
    // =========================================================
    if (ImGui::CollapsingHeader("Object List & Edit", ImGuiTreeNodeFlags_DefaultOpen)) {

        // --- リスト表示 ---
        ImGui::Text("Object List:");
        // リストボックスにして、縦に長くなりすぎるのを防ぐ（高さ約100ピクセル）
        if (ImGui::BeginListBox("##ObjectList", ImVec2(-FLT_MIN, 100))) {
            for (int i = 0; i < levelData_.objects.size(); ++i) {
                std::string label = std::to_string(i) + ": " + levelData_.objects[i].type;
                if (ImGui::Selectable(label.c_str(), selectedObjectIndex_ == i)) {
                    selectedObjectIndex_ = i;
                }
            }
            ImGui::EndListBox();
        }

        // --- 選択中のオブジェクト編集 ---
        if (selectedObjectIndex_ >= 0 && selectedObjectIndex_ < levelData_.objects.size()) {
            ImGui::Separator();
            ImGui::Text("Editing Object [%d] : %s", selectedObjectIndex_, levelData_.objects[selectedObjectIndex_].type.c_str());

            if (ImGui::Button("Delete Object")) {
                levelData_.objects.erase(levelData_.objects.begin() + selectedObjectIndex_);
                object3ds_.erase(object3ds_.begin() + selectedObjectIndex_);
                selectedObjectIndex_ = -1;
            }

            if (selectedObjectIndex_ != -1) {
                auto& objData = levelData_.objects[selectedObjectIndex_];

                ImGui::SameLine();
                if (ImGui::Button("Duplicate")) {
                    LevelObjectData dupObj = objData;
                    levelData_.objects.push_back(dupObj);

                    Model* model = ModelManager::GetInstance()->FindModel(dupObj.type);
                    if (model != nullptr) {
                        std::unique_ptr<Obj3d> obj = std::make_unique<Obj3d>();
                        obj->Initialize(model);
                        obj->SetCamera(camera_);
                        obj->SetTranslation(dupObj.translation);
                        obj->SetRotation(dupObj.rotation);
                        obj->SetScale(dupObj.scale);
                        object3ds_.push_back(std::move(obj));
                        selectedObjectIndex_ = (int)levelData_.objects.size() - 1;
                    }
                }

                ImGui::Checkbox("Snap to Grid (1.0 step)", &snapToGrid_);

                bool isChanged = false;
                float moveStep = snapToGrid_ ? 1.0f : 0.1f;
                isChanged |= ImGui::DragFloat3("Position", &objData.translation.x, moveStep);
                isChanged |= ImGui::DragFloat3("Rotation", &objData.rotation.x, 0.05f);
                isChanged |= ImGui::DragFloat3("Scale", &objData.scale.x, 0.1f);

                if (isChanged) {
                    if (snapToGrid_) {
                        objData.translation.x = std::round(objData.translation.x);
                        objData.translation.y = std::round(objData.translation.y);
                        objData.translation.z = std::round(objData.translation.z);
                    }
                    object3ds_[selectedObjectIndex_]->SetTranslation(objData.translation);
                    object3ds_[selectedObjectIndex_]->SetRotation(objData.rotation);
                    object3ds_[selectedObjectIndex_]->SetScale(objData.scale);
                }
            }
        }
    }

    ImGui::End();
#endif

    // すべてのオブジェクトの行列を更新
    for (auto& obj : object3ds_) {
        obj->Update();
    }
}
void LevelEditor::Draw(){
    for ( auto& obj : object3ds_ ) {
        obj->Draw();
    }
}