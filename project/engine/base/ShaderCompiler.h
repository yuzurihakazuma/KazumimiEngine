#pragma once
#define NOMINMAX
#include <Windows.h>
#include <wrl.h>
#include<dxcapi.h>
#include <string>

#pragma comment(lib,"dxcompiler.lib")

// シェーダーコンパイラー
class ShaderCompiler{
public:
    // -------------------- 初期化 --------------------

   /// <summary>
   /// 初期化
   /// </summary>
    bool Initialize();


    // -------------------- シェーダーコンパイル --------------------

    /// <summary>
    /// シェーダーファイルをコンパイルして DXC Blob を返す
    /// </summary>
    Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
        const std::wstring& filePath,
        const wchar_t* profile);


private:

    // -------------------- DXC関連オブジェクト --------------------

    Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;             // DXCユーティリティ
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;      // DXCコンパイラ本体
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_; // インクルードハンドラ

};

