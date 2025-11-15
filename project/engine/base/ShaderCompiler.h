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
	// 初期化
	bool Initialize();
	
	Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
		// CompilerするShaderファイルへのパス
		const std::wstring& filePath,
		// Compilerに仕様するProfile
		const wchar_t* profile);



private:
	Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;


};

