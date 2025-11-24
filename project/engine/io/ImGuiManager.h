#pragma once
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include<cstdint>
#include <wtypes.h>
#include <d3d12.h>
#include <wrl.h> // Microsoft::WRL::ComPtr
#include "DirectXComon.h"

using Microsoft::WRL::ComPtr;


class ImGuiManager{
public:

    // -------------------- 初期化・終了処理 --------------------

    /// <summary>
    /// ImGui の初期化
    /// </summary>
    void Initialize(
        HWND hwnd,
        ComPtr<ID3D12Device> device,
        DXGI_FORMAT rtvFormat,
        UINT numFramesInFlight,
        ComPtr<ID3D12DescriptorHeap> srvHeap
    );

    /// <summary>
    /// フレーム開始処理
    /// </summary>
    void Begin();

    /// <summary>
    /// フレーム終了処理
    /// </summary>
    void End(ComPtr<ID3D12GraphicsCommandList> commandList);

    /// <summary>
    /// ImGui の描画コマンドを発行
    /// </summary>
    void Render(ComPtr<ID3D12GraphicsCommandList> commandList);

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

    // -------------------- メンバー変数 --------------------

    HWND hwnd_ = nullptr;                      // Win32ウィンドウハンドル
    UINT numFrames_ = 0;                       // フレームインフライト数（マルチバッファ）
    ComPtr<ID3D12Device> device_ = nullptr;    // D3D12デバイス
    ComPtr<ID3D12DescriptorHeap> srvHeap_ = nullptr; // ImGui用SRVヒープ

};

