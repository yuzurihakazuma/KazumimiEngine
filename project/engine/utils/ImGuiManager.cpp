#include "ImGuiManager.h"
// --- 外部ライブラリ ---
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

// --- エンジン側のファイル ---
#include "engine/base/DirectXCommon.h"
#include "engine/base/WindowProc.h"
#include "engine/graphics/SrvManager.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


ImGuiManager* ImGuiManager::GetInstance() {
    static ImGuiManager instance;
    return &instance;
}


void ImGuiManager::Initialize(WindowProc* windowProc, DirectXCommon* dxCommon){

    // 1. コンテキストの生成とスタイル設定
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // 2. Win32用初期化
    // WindowProcからハンドルを取得して渡す
    ImGui_ImplWin32_Init(windowProc->GetHwnd());

    // 3. DirectX12用初期化
    // スライド通り: SrvManagerからSRVを確保してインデックスを得る
    SrvManager* srvManager = dxCommon->GetSrvManager();
    uint32_t srvIndex = srvManager->Allocate();

#pragma region SRV記述子ヒープのハンドルを取得
    // 確保したインデックスに対応するCPUハンドルとGPUハンドルを取得
    D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle = srvManager->GetCPUDescriptorHandle(srvIndex);
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle = srvManager->GetGPUDescriptorHandle(srvIndex);

    // SRVヒープそのものも必要
    ID3D12DescriptorHeap* srvHeap = srvManager->GetDescriptorHeap();
#pragma endregion

    // 初期化関数を呼び出す
    ImGui_ImplDX12_Init(
        dxCommon->GetDevice(),
        static_cast< int >( dxCommon->GetBackBufferCount() ), // バックバッファ数(通常2)
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,                  // RTVのフォーマット(DirectXCommonと合わせる)
        srvHeap,                                          // SRVヒープ
        srvCpuHandle,                                     // ImGuiが使うSRVのCPUハンドル
        srvGpuHandle                                      // ImGuiが使うSRVのGPUハンドル
    );
}

void ImGuiManager::Begin(){
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}
void ImGuiManager::End(ID3D12GraphicsCommandList* commandList){
    ImGui::Render(); // ImGui描画データ生成
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}
void ImGuiManager::Render(ID3D12GraphicsCommandList* commandList){
    // Endの中でRenderDrawDataを呼んでいるなら、ここはEndを呼ぶか、あるいは空でもよい
    End(commandList);
}
void ImGuiManager::Shutdown(){
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

bool ImGuiManager::WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam){
	return ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);
}
