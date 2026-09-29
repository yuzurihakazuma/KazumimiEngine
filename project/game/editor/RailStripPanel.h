#pragma once
// =====================================================================
//  RailStripPanel：配置ビュー（レール展開図）
//   ブロック・敵・コインを ImGui のパネルの中だけで配置するための2Dエディタ。
//
//   このゲームのワールドはマップチップ（マス目）ではないが、配置データは
//   「どのレールの・何m地点の・何段目」というレール基準で持っている。
//   そこでレール1本をまっすぐに伸ばして横から見た図（展開図）にすると、
//     横 = レール上の距離（1m刻み） / 縦 = 段（1段=1m）
//   のマス目になり、曲がったレールも坂のレールもマス目エディタとして扱える。
//
//   ・ここで行った編集は既存のデータ（RailEditor のブロック/コイン、EnemyEditor の敵）を
//     直接書き換えるので、Game View・保存・元に戻す はそのまま連動する
//   ・マウスで指しているマスは Game View 側にも枠で表示する（3Dのどこに置かれるかが分かる）
//
//   このクラスは窓（Begin/End・レイアウト）と部品のまとめ役だけを受け持つ。中身は game/editor/strip/ の
//     StripRailSelector（開くレール）/ StripControls（操作部）/ StripCanvas（展開図と道具）/
//     StripRenderer（描画）/ StripBlockTool・StripEnemyTool・StripCoinTool（道具）
//   に分けてあり、部品どうしは StripState（共有の状態）を参照で受け渡す
// =====================================================================
#include "game/editor/strip/StripCanvas.h"
#include "game/editor/strip/StripContext.h"
#include "game/editor/strip/StripControls.h"
#include "game/editor/strip/StripRailSelector.h"
#include "game/editor/strip/StripState.h"

class RailStripPanel {
public:
    // ウィンドウ名。EditorManager のパネル表（kPanelIcons）の名前と必ず一致させること
    static constexpr const char* kWindowTitle = "配置ビュー (レール展開図)";

    // 毎フレーム、シーンから受け取るもの（どれも所有しない。中身は StripContext を参照）
    using Context = StripContext;

    void Draw(const Context& context);

    // 「カメラをここへ」の要求を取り出す（あれば true）。height はレール面からの高さ(m)
    bool ConsumeFocusRequest(int& outRail, float& outDist, float& outHeight);
    // 「敵の設定を開く」の要求を取り出す（あれば true。シーンが敵エディタのパネルを開く）
    bool ConsumeOpenEnemyPanelRequest();

private:
    // 操作部（上段・道具・表示の設定）を描き、そこで起きた出来事（レールの選択・道具の切り替え等）を反映する
    void DrawControls(const Context& context);
    // 展開図を描き、その中で選ばれたレールを描画の後で開く
    void DrawCanvas(const Context& context);
    // レールを開く（切り替えたら途中の操作をやめる）
    void SelectRail(const Context& context, int railIndex, float arrivalDist = -1.0f);
    // 途中の操作（塗り・ドラッグ・表示の移動）を全部やめる
    void CancelInteractions();

    StripState        state_;
    StripRailSelector railSelector_;
    StripControls     controls_;
    StripCanvas       canvas_;
    int lastDrawFrame_ = -1;  // 最後に描いたフレーム（描かれない間があったら途中の操作を捨てる）
};
