#pragma once

#ifdef _DEBUG
#include <wrl.h>
#include <dxgidebug.h>
#include <windows.h>   
#include <dxgi1_3.h>
#pragma comment(lib, "dxguid.lib")
#endif
#include <d3d12sdklayers.h>

// リソースリーク（メモリ解放忘れ）をチェックする便利構造体
struct ResourceLeakChecker {

    // コンストラクタ（生成時）は何もしない
    ResourceLeakChecker() = default;

    // デストラクタ（プログラム終了時＝この変数の寿命が尽きた時に自動で呼ばれる！）
    ~ResourceLeakChecker() {
#ifdef _DEBUG
        Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug))))
        {
            // D3D12のリソースリークを出力
            debug->ReportLiveObjects(
                DXGI_DEBUG_D3D12,
                DXGI_DEBUG_RLO_DETAIL
            );

            // アプリケーション全体（スワップチェーン等）のリークも出力
            debug->ReportLiveObjects(
                DXGI_DEBUG_APP,
                DXGI_DEBUG_RLO_DETAIL
            );
        }
#endif
    }
};