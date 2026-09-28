#pragma once
#include "game/enemy/Enemy.h" // EnemySpawnData
#include <string>
#include <vector>

class SplineRail;

// =====================================================================
//  EnemyEditor : エネミーのレール配置データを管理し、ImGui UI を提供する
//   ウィンドウは「上＝一覧（絞り込み・レール別まとめ）／下＝選択中の1体の設定」の2段。
//   敵が何十体に増えても一覧の高さは変わらず、編集欄が流れていかない
// =====================================================================
class EnemyEditor {
public:
    EnemyEditor();
    ~EnemyEditor();

    // 初期化：デフォルトの敵データを登録する
    void Initialize();

    // ImGui ウィンドウの描画。
    //   pickRail/pickDist: レールエディタで選択中ノードのレール番号と距離（「選択ノードに配置」用）。
    //   hasPick=false なら未選択（ボタンはグレーアウト）
    void DrawWindow(const std::vector<SplineRail>& splineRails,
                    int pickRail = -1, float pickDist = 0.0f, bool hasPick = false);

    // Game View ハイライト用：一覧でホバー中/選択中のエントリ番号（-1=なし）
    int GetHoveredEntry() const { return hoveredEntry_; }
    int GetSelectedEntry() const { return selectedEntry_; }
    // Game View 直接ドラッグ用：選択の同期と、MutableSpawnDatas 直編集後の確定通知
    void SetSelectedEntry(int i) { selectedEntry_ = i; scrollToSelected_ = true; }
    void MarkChanged() { changed_ = true; }

    // 敵を別の場所へ動かす（Game View のドラッグ・「選択ノードへ移動」共通の入口）。
    //   行動範囲を指定している敵は、範囲ごと一緒に動かす（置き直した先で範囲外へ瞬間移動しない）
    void MoveEntry(int index, int railIndex, float distance, const std::vector<SplineRail>& splineRails);

    // --- 敵の追加・削除・複製（敵エディタの窓以外＝Game View や配置ビューからも使う共通の入口）---
    //   どれも選択状態の付け替えと変更通知まで行う
    int  AddEntry(const EnemySpawnData& spawn); // 追加して選択する。追加した番号を返す
    void RemoveEntry(int index);                // 削除（選択中の番号がずれないよう付け替える）
    int  DuplicateEntry(int index, const std::vector<SplineRail>& splineRails); // 2m先へ複製。新しい番号を返す（失敗=-1）

    // --- 元に戻す／やり直し（敵の配置・設定だけの履歴）---
    //   TickHistory は敵エディタの窓が閉じていても毎フレーム呼ぶこと
    //   （Game View や配置ビューでの変更も1操作＝1手として履歴に積むため）
    void TickHistory();
    void Undo();
    void Redo();
    // まだ履歴に積んでいない変更（文字を打っている途中など）も「戻せる」に数える
    bool CanUndo() const { return !undoStack_.empty() || !( spawnDatas_ == lastCommitted_ ); }
    bool CanRedo() const { return !redoStack_.empty(); }

    // レールの数や並びが変わった時に呼ぶ（レールの削除・その元に戻す/やり直し）。
    //   oldToNew[旧番号] = 新番号（-1 = そのレールは無くなった＝載っていた敵を外す）。
    //   revived はよみがえったレールに載っていた敵（新番号で）。付け加える。
    //   元に戻す/やり直しの履歴の中身も同じように付け替える（しないと、戻した時に別のレールへずれる）。
    //   付け替えそのものは履歴の1手に数えない（レール側の操作に付いてくる変化なので）
    void ApplyRailRemap(const std::vector<int>& oldToNew, const std::vector<EnemySpawnData>& revived);

    // レールの編集に合わせてシーンが距離を張り直した後に呼ぶ：その張り直しを履歴の1手に数えない
    //   （張り直す前に、まだ履歴に積んでいない利用者の変更が無い時だけ呼ぶこと）
    bool IsHistoryClean() const { return spawnDatas_ == lastCommitted_; }
    void RebaseHistory() { lastCommitted_ = spawnDatas_; }

    // 「カメラをこの敵へ」ボタンの要求を取り出す（あれば true。シーンがカメラを動かす）
    bool ConsumeFocusRequest(int& outIndex);

    // 配置情報の取得
    const std::vector<EnemySpawnData>& GetSpawnDatas() const { return spawnDatas_; }

    // レール編集時の「距離の張り直し」用に直接書き換えるアクセサ。
    //   SetSpawnDatas と違い選択状態や changed_ フラグを触らない（UI の選択が飛ばない）。
    std::vector<EnemySpawnData>& MutableSpawnDatas() { return spawnDatas_; }

    // 配置情報の差し替え（マップ読込時に外部データで上書きする）
    void SetSpawnDatas(const std::vector<EnemySpawnData>& datas);

    // 配置データに変更があったかどうか（取得するとフラグはリセットされる）
    bool ConsumeChanged() { bool c = changed_; changed_ = false; return c; }

    // 一覧・ピンに出す表示名（名前が空なら種類名）
    static std::string DisplayName(const EnemySpawnData& spawn);
    // 敵タイプ名の取得ヘルパー（短縮版と表示版）
    static const char* GetTypeName(EnemyType type);
    static const char* GetTypeLabel(EnemyType type);

private:
    // --- ウィンドウの各部 ---
    void DrawToolbar(const std::vector<SplineRail>& splineRails, int pickRail, float pickDist, bool hasPick);
    void DrawList(const std::vector<SplineRail>& splineRails);
    void DrawInspector(const std::vector<SplineRail>& splineRails, int pickRail, float pickDist, bool hasPick);
    bool DrawListRow(int index); // 一覧の1行。クリックされたら true
    bool PassesFilter(const EnemySpawnData& spawn) const;

    // --- 履歴の中身 ---
    void CommitPending(); // 前回から変わっていれば、今の状態を履歴の1手として積む
    std::vector<std::vector<EnemySpawnData>> undoStack_;
    std::vector<std::vector<EnemySpawnData>> redoStack_;
    std::vector<EnemySpawnData> lastCommitted_; // 履歴に積んだ最後の状態
    static constexpr size_t kMaxHistory = 60;

    std::vector<EnemySpawnData> spawnDatas_; // 登録されたエネミー配置データの配列

    // 配置データが変更されたことを示すフラグ（シーン側が検知してリスポーンする）
    bool changed_ = false;

    // 新しく置く敵のひな形（種類・動きの設定をそのまま引き継いで置ける）
    EnemySpawnData newTemplate_;
    int multiCount_ = 3; // 「等間隔で並べる」の体数

    // 一覧で現在選択中のエントリ（下の設定欄の編集対象）
    int selectedEntry_ = -1;
    bool scrollToSelected_ = false; // 外から選択された時に一覧をその行まで送る

    // 一覧でホバー中のエントリ（Game View ハイライト用。毎フレーム取り直す）
    int hoveredEntry_ = -1;

    // 一覧の絞り込み
    int  filterType_ = -1;      // -1=全種類
    int  filterRail_ = -1;      // -1=全レール
    char filterText_[64] = {};  // 名前の部分一致
    bool groupByRail_ = true;   // レール別にまとめて表示

    // 全削除の2段階確認（1回目のクリック時刻）
    double clearArmedTime_ = -100.0;

    int focusRequest_ = -1; // 「カメラをこの敵へ」の要求（-1=なし）
};
