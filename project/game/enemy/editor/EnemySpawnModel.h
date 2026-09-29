#pragma once
#include "game/enemy/Enemy.h" // EnemySpawnData
#include <vector>

class SplineRail;

// =====================================================================
//  EnemySpawnModel : 敵の配置データ（正本）と、その上の選択・変更通知・操作
//   追加・削除・複製・移動・レール番号の付け替えはすべてここを通す。
//   操作に合わせて選択番号も付け替える（削除で番号が詰まる等）ので、
//   敵エディタの窓・Game View・配置ビューのどこから触っても選択がずれない
// =====================================================================
class EnemySpawnModel {
public:
    // --- 配置データ ---
    const std::vector<EnemySpawnData>& Datas() const { return spawnDatas_; }
    // レール編集時の「距離の張り直し」や履歴からの書き戻し用。選択や changed_ は触らない
    std::vector<EnemySpawnData>& MutableDatas() { return spawnDatas_; }
    int  Count() const { return ( int ) spawnDatas_.size(); }
    bool IsValidIndex(int index) const { return index >= 0 && index < Count(); }

    void Replace(const std::vector<EnemySpawnData>& datas); // 丸ごと差し替え（選択は外す）
    void Clear();                                           // 全削除（選択は外す）
    void OnRestored(); // 元に戻す/やり直しで中身が入れ替わった後に呼ぶ（はみ出た選択を外して変更通知）

    // --- 選択（下の設定欄の編集対象）---
    int  SelectedEntry() const { return selectedEntry_; }
    void Select(int index) { selectedEntry_ = index; }                                 // 一覧のクリック（その場なので送らない）
    void SelectAndScroll(int index) { selectedEntry_ = index; scrollToSelected_ = true; } // 外から選択：一覧をその行まで送る
    void ClearSelection() { selectedEntry_ = -1; }
    void DropStaleSelection() { if ( selectedEntry_ >= Count() ) { selectedEntry_ = -1; } }
    bool IsScrollToSelectedPending() const { return scrollToSelected_; }
    void ClearScrollToSelected() { scrollToSelected_ = false; }

    // --- 変更通知（シーン側が検知してリスポーンする）---
    void MarkChanged() { changed_ = true; }
    bool ConsumeChanged() { bool c = changed_; changed_ = false; return c; }

    // --- 「カメラをこの敵へ」の要求 ---
    void RequestFocus(int index) { focusRequest_ = index; }
    bool ConsumeFocusRequest(int& outIndex);

    // --- 敵の追加・削除・複製・移動 ---
    int  AddEntry(const EnemySpawnData& spawn); // 追加して選択する。追加した番号を返す
    void RemoveEntry(int index);                // 削除（選択中の番号がずれないよう付け替える）
    int  DuplicateEntry(int index, const std::vector<SplineRail>& splineRails); // 2m先へ複製。新しい番号を返す（失敗=-1）
    // 敵を別の場所へ動かす。行動範囲を指定している敵は、範囲ごと一緒に動かす
    //   （変更通知は立てない：Game View のドラッグ中は手を離した時に呼び出し側が立てる）
    void MoveEntry(int index, int railIndex, float distance, const std::vector<SplineRail>& splineRails);

    // レールの数や並びが変わった：敵のレール番号を付け替える（選択はその敵を追いかける）
    void ApplyRailRemap(const std::vector<int>& oldToNew, const std::vector<EnemySpawnData>& revived);
    // 1つのリストを付け替える（履歴の中身にも同じものを使う）
    static void RemapRails(std::vector<EnemySpawnData>& list,
                           const std::vector<int>& oldToNew, const std::vector<EnemySpawnData>& revived);

    // 行動範囲を指定しているか（始点・終点のどちらかが決まっていれば範囲つき。-1=レール全体）
    static bool HasMoveRange(const EnemySpawnData& spawn) { return spawn.patrolMin >= 0.0f || spawn.patrolMax >= 0.0f; }

private:
    std::vector<EnemySpawnData> spawnDatas_; // 登録されたエネミー配置データの配列

    // 配置データが変更されたことを示すフラグ（シーン側が検知してリスポーンする）
    bool changed_ = false;

    // 一覧で現在選択中のエントリ（下の設定欄の編集対象）
    int selectedEntry_ = -1;
    bool scrollToSelected_ = false; // 外から選択された時に一覧をその行まで送る

    int focusRequest_ = -1; // 「カメラをこの敵へ」の要求（-1=なし）
};
