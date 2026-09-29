#pragma once
// D3D12オブジェクトに「作った場所（ファイル名:行）」の名前を付ける。
//   終了時のリークレポート（LIVE_ROOTSIGNATURE 等）に Name として表示され、どこで作った物が残ったか分かる。
#include <d3d12.h>
#include <source_location>
#include <string>

inline void SetD3DDebugName(ID3D12Object* object, const wchar_t* kind, const std::source_location& where){
#ifdef _DEBUG
	if ( !object ) return;
	std::string file = where.file_name();
	size_t slash = file.find_last_of("\\/");
	if ( slash != std::string::npos ) { file = file.substr(slash + 1); }
	std::wstring name = kind;
	name += L" ";
	name += std::wstring(file.begin(), file.end()); // ソースのファイル名はASCIIのみ
	name += L":" + std::to_wstring(where.line());
	object->SetName(name.c_str());
#else
	( void )object; ( void )kind; ( void )where;
#endif
}
