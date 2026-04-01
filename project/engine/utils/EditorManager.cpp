#include "EditorManager.h"
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif
#include "engine/base/Input.h"
#include "engine/base/DirectXCommon.h"
#include "engine/utils/ImGuiManager.h"
#include "engine/postEffect/PostEffect.h"
#include "engine/graphics/SrvManager.h"
#include "engine/scene/SceneManager.h"

void EditorManager::Begin(){
#ifdef USE_IMGUI
    ImGuiManager::GetInstance()->Begin();
#endif
}

void EditorManager::Update(){
#ifdef USE_IMGUI
    Input* input = Input::GetInstance();
    if ( input->Triggerkey(DIK_F1) ) {
        isEditorActive_ = !isEditorActive_;
    }

    if ( !isEditorActive_ ) { return; }

    // 1. 全画面の透明なドッキング土台
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::Begin("MasterDockSpace", nullptr, window_flags);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();

    // 2. メインメニューバー
    if ( ImGui::BeginMainMenuBar() ) {
        if ( ImGui::BeginMenu("ファイル (File)") ) {
            if ( ImGui::MenuItem("パーティクルを保存 (Save Particles)") ) {}
            if ( ImGui::MenuItem("パーティクルを読み込む (Load Particles)") ) {}
            ImGui::Separator();
            if ( ImGui::MenuItem("エディタを終了する") ) {}
            ImGui::EndMenu();
        }
        if ( ImGui::BeginMenu("ウィンドウ (Window)") ) {
            if ( ImGui::MenuItem("レイアウトをリセット (※再起動後に反映)") ) {}
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // 3. Game View
    ImGui::Begin("Game View");
    ImVec2 sceneSize = ImGui::GetContentRegionAvail();
    if ( sceneSize.x < 10.0f ) sceneSize.x = 640.0f;
    if ( sceneSize.y < 10.0f ) sceneSize.y = 360.0f;
    uint32_t srvIndex = PostEffect::GetInstance()->GetSrvIndex();
    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle =
        SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex);
    ImGui::Image(( ImTextureID ) ( uintptr_t ) textureHandle.ptr, sceneSize);
    ImGui::End();

    // 4. 全シーン共通のUI
    PostEffect::GetInstance()->DrawDebugUI();

    // 5. 現在のシーン固有のUI
    SceneManager::GetInstance()->DrawCurrentSceneDebugUI();

    // 6. パフォーマンスモニター
    ImGui::Begin("パフォーマンスモニター");
    float fps = ImGui::GetIO().Framerate;
    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("フレーム時間: %.3f ms", 1000.0f / fps);
    ImGui::Separator();
    ImGui::Text("[CPU] 更新処理(Update) : %.3f ms", cpuUpdateTimeMs_);
    ImGui::Text("[CPU] 描画準備(Draw)   : %.3f ms", cpuDrawTimeMs_);
    ImGui::Separator();
    float totalCpuTime = cpuUpdateTimeMs_ + cpuDrawTimeMs_;
    if ( fps < 55.0f ) {
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), " 警告: 処理落ちが発生しています！");
        if ( totalCpuTime > 16.0f ) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f),
                " 原因: CPUの処理が重いです\n（計算やループ処理が多すぎます）");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f),
                " 原因: GPUの処理が重いです\n（描画する量が多すぎるか、シェーダーが重いです）");
        }
    } else {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), " 快適に動作しています！ (60 FPS維持)");
    }
    ImGui::End();
#endif
}

void EditorManager::End(){
#ifdef USE_IMGUI
    DirectXCommon* dxCommon = DirectXCommon::GetInstance();
    ImGuiManager::GetInstance()->End(dxCommon->GetCommandList());
#endif
}
