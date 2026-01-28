#pragma once
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include<cstdint>
#include <wtypes.h>
#include <d3d12.h>
#include <wrl.h> // Microsoft::WRL::ComPtr
class DirectXCommon;
class WindowProc;

using Microsoft::WRL::ComPtr;


class ImGuiManager{
public:

    // シングルトンインスタンスの取得
    static ImGuiManager* GetInstance();

    // -------------------- 初期化・終了処理 --------------------

    /// <summary>
    /// ImGui の初期化
    /// </summary>
    void Initialize(WindowProc* windowProc, DirectXCommon* dxCommon);

    /// <summary>
    /// フレーム開始処理
    /// </summary>
    void Begin();

    /// <summary>
    /// フレーム終了処理
    /// </summary>
    void End(ID3D12GraphicsCommandList* commandList); 

    /// <summary>
    /// ImGui の描画コマンドを発行
    /// </summary>
    void Render(ID3D12GraphicsCommandList* commandList);
    /// <summary>
    /// ImGui の破棄処理
    /// </summary>
    void Shutdown();


    // -------------------- メッセージ処理 --------------------

    /// <summary>
    /// Win32 メッセージの ImGui 用処理
    /// </summary>
    bool WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
private:
    // コンストラクタを隠蔽
    ImGuiManager() = default;
    ~ImGuiManager() = default;
    ImGuiManager(const ImGuiManager&) = delete;
    ImGuiManager& operator=(const ImGuiManager&) = delete;

private:

    // -------------------- メンバー変数 --------------------

    HWND hwnd_ = nullptr;                      // Win32ウィンドウハンドル
    UINT numFrames_ = 0;                       // フレームインフライト数（マルチバッファ）
    ComPtr<ID3D12Device> device_ = nullptr;    // D3D12デバイス
    ComPtr<ID3D12DescriptorHeap> srvHeap_ = nullptr; // ImGui用SRVヒープ

};

