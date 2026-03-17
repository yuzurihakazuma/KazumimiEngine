#pragma once

#ifdef _DEBUG
#include <crtdbg.h>

// 呼び出すと CRT デバッグフラグを有効にし、指定した割り当て番号でブレークします。
// メッセージに出ているブロック番号に合わせて _CrtSetBreakAlloc の値を変更してください。
inline void InstallHeapDebugger(unsigned long breakAlloc = 594) {
    // 指定の割り当て番号でブレーク
    if (breakAlloc != 0) {
        _CrtSetBreakAlloc(breakAlloc);
    }
    // デバッグフラグを設定（割り当て情報収集・終了時のリークチェック・常時チェック）
    int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    flags |= _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF;
    _CrtSetDbgFlag(flags);
}
#else
inline void InstallHeapDebugger(unsigned long) {}
#endif