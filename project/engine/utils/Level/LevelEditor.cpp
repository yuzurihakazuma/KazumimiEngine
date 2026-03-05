#include "LevelEditor.h"
#include "engine/utils/ImGuiManager.h"
#include "engine/3d/model/ModelManager.h"

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


void LevelEditor::Update(){
#ifdef USE_IMGUI
    ImGui::Begin("Level Editor");

    if ( ImGui::Button("Save map") ){
        LevelManager::GetInstance()->Save("resources/map/map01.json", levelData_);
    }
    ImGui::SameLine();
    if ( ImGui::Button("Load map") ){
        LoadAndCreateMap("resources/map/map01.json");
    }

    ImGui::Separator();

	//  "block" に固定してみる！(今後、UIで選べるようにする予定)
    if ( ImGui::Button("Add Block") ) {
        LevelObjectData newObj;
        newObj.type = "block"; // ★ JSONに保存する名前を "block" にする
        newObj.translation = { 0.0f, 0.0f, 0.0f };
        newObj.rotation = { 0.0f, 0.0f, 0.0f };
        newObj.scale = { 1.0f, 1.0f, 1.0f };
        levelData_.objects.push_back(newObj);

		// 追加したオブジェクトのモデルを検索して持ってくる
        Model* model = ModelManager::GetInstance()->FindModel("block");
        std::unique_ptr<Obj3d> obj = std::make_unique<Obj3d>();
        obj->Initialize(model); 

        object3ds_.push_back(std::move(obj));
        selectedObjectIndex_ = ( int ) levelData_.objects.size() - 1;
    }

    ImGui::Separator();

    ImGui::Text("Object List:");
    for ( int i = 0; i < levelData_.objects.size(); ++i ) {
        std::string label = std::to_string(i) + ": " + levelData_.objects[i].type;
        if ( ImGui::Selectable(label.c_str(), selectedObjectIndex_ == i) ) {
            selectedObjectIndex_ = i;
        }
    }

    ImGui::Separator();

    if ( selectedObjectIndex_ >= 0 && selectedObjectIndex_ < levelData_.objects.size() ) {
        auto& objData = levelData_.objects[selectedObjectIndex_];
        bool isChanged = false;
        isChanged |= ImGui::DragFloat3("Position", &objData.translation.x, 0.1f);
        isChanged |= ImGui::DragFloat3("Rotation", &objData.rotation.x, 0.05f);
        isChanged |= ImGui::DragFloat3("Scale", &objData.scale.x, 0.1f);

        if ( isChanged ) {
            object3ds_[selectedObjectIndex_]->SetTranslation(objData.translation);
            object3ds_[selectedObjectIndex_]->SetRotation(objData.rotation);
            object3ds_[selectedObjectIndex_]->SetScale(objData.scale);
        }
    }

    ImGui::End();
#endif

    // すべてのオブジェクトの行列を更新
    for ( auto& obj : object3ds_ ) {
        obj->Update();
    }
}

void LevelEditor::Draw(){
    for ( auto& obj : object3ds_ ) {
        obj->Draw();
    }
}