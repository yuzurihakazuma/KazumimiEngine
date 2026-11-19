#pragma once
#define NOMINMAX
#include <Windows.h>   // EXCEPTION_POINTERS, SYSTEMTIME など
#include <DbgHelp.h>   // MINIDUMP_TYPE

namespace KazumimiEngine {

// =====================================================================
//  CrashDumper：ゲームが落ちた時に、原因を調べるためのダンプファイルを書き出す。
//   起動時に Install しておくと、処理されなかった例外でダンプが残る。WriteDumpNow で好きな時にも書ける
// =====================================================================
class CrashDumper{
public:
	// クラッシュダンプを生成する
	static void Install();

	// 任意のタイミングで手動ダンプを吐きたい場合（例外情報なしでも作れる）
	static bool WriteDumpNow();

private:
	// クラッシュダンプを生成する関数
	static LONG WINAPI ExportDump_(EXCEPTION_POINTERS* exception);
	// ダンプを書き出す関数
	static bool WriteDump_(EXCEPTION_POINTERS* exception);


};

} // namespace KazumimiEngine
