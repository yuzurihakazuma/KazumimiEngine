//#pragma once
//
//#ifdef _DEBUG
//#include <wrl.h>
//#include <dxgidebug.h>
//#pragma comment(lib, "dxguid.lib")
//#endif
//
//struct ResourceLeakChecker{
//#ifdef _DEBUG
//    static void Report(){
//        Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
//        if ( SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug))) )
//        {
//           
//            debug->ReportLiveObjects(
//                DXGI_DEBUG_D3D12,
//                DXGI_DEBUG_RLO_DETAIL
//            );
//        }
//    }
//#endif
//};