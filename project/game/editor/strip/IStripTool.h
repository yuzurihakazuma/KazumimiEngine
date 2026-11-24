#pragma once

struct StripContext;
struct StripState;
struct StripLayout;

// =====================================================================
//  IStripTool：配置ビュー（レール展開図）の道具の共通の形（ブロック・敵・コイン）。
//   StripCanvas は、今の道具を基底のポインタで持って Update / Cancel / IsBusy を呼ぶ（道具の種類で分岐しない）。
//   道具を足す時は、これを継承したクラスを 1 つ足して StripCanvas::Tools に並べる
// =====================================================================
class IStripTool {
public:
    virtual ~IStripTool() = default;
    // マウスの操作。戻り値 true＝途中の操作を全部やめる必要がある（ドラッグ中の物が消えていた、など）
    virtual bool Update(const StripContext& context, StripState& state, const StripLayout& layout, bool hovered) = 0;
    // 途中の操作をやめる（何も適用しない）
    virtual void Cancel() = 0;
    // 何かを塗っている・動かしている途中か
    virtual bool IsBusy() const = 0;
};
