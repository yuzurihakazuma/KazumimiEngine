#pragma once
#include "game/enemy/Enemy.h" // EnemySpawnData
#include "game/enemy/editor/EnemyEditorAddPanel.h"
#include "game/enemy/editor/EnemyEditorInspector.h"
#include "game/enemy/editor/EnemyEditorList.h"
#include "game/enemy/editor/EnemySpawnHistory.h"
#include "game/enemy/editor/EnemySpawnModel.h"
#include <string>
#include <vector>

class SplineRail;

// =====================================================================
//  EnemyEditor : エネミーのレール配置データを管理し、ImGui UI を提供する
//   ウィンドウは「上＝一覧（絞り込み・レール別まとめ）／下＝選択中の1体の設定」の2段。
//   敵が何十体に増えても一覧の高さは変わらず、編集欄が流れていかない
//
//   中身は役割ごとのクラスに分けてあり、ここは外（シーン・Game View・配置ビュー）への窓口：
//     EnemySpawnModel      … 配置データの正本・選択・変更通知・追加/削除/複製/移動
//     EnemySpawnHistory    … 元に戻す／やり直し（1操作＝1手）
//     EnemyEditorAddPanel  … 上段「新しく置く」
//     EnemyEditorList      … 中段「配置済みの敵」一覧（絞り込み・レール別まとめ）
//     EnemyEditorInspector … 下段「選択中の1体の設定」
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
    int GetHoveredEntry() const { return list_.HoveredEntry(); }
    int GetSelectedEntry() const { return model_.SelectedEntry(); }
    // Game View 直接ドラッグ用：選択の同期と、MutableSpawnDatas 直編集後の確定通知
    void SetSelectedEntry(int i) { model_.SelectAndScroll(i); }
    void MarkChanged() { model_.MarkChanged(); }

    // 敵を別の場所へ動かす（Game View のドラッグ・「選択ノードへ移動」共通の入口）。
    //   行動範囲を指定している敵は、範囲ごと一緒に動かす（置き直した先で範囲外へ瞬間移動しない）
    void MoveEntry(int index, int railIndex, float distance, const std::vector<SplineRail>& splineRails) {
        model_.MoveEntry(index, railIndex, distance, splineRails);
    }

    // --- 敵の追加・削除・複製（敵エディタの窓以外＝Game View や配置ビューからも使う共通の入口）---
    //   どれも選択状態の付け替えと変更通知まで行う
    int  AddEntry(const EnemySpawnData& spawn) { return model_.AddEntry(spawn); } // 追加して選択する。追加した番号を返す
    void RemoveEntry(int index) { model_.RemoveEntry(index); }                    // 削除（選択中の番号がずれないよう付け替える）
    int  DuplicateEntry(int index, const std::vector<SplineRail>& splineRails) {  // 2m先へ複製。新しい番号を返す（失敗=-1）
        return model_.DuplicateEntry(index, splineRails);
    }

    // --- 元に戻す／やり直し（敵の配置・設定だけの履歴）---
    //   TickHistory は敵エディタの窓が閉じていても毎フレーム呼ぶこと
    //   （Game View や配置ビューでの変更も1操作＝1手として履歴に積むため）
    void TickHistory();
    void Undo();
    void Redo();
    // まだ履歴に積んでいない変更（文字を打っている途中など）も「戻せる」に数える
    bool CanUndo() const { return history_.CanUndo(model_.Datas()); }
    bool CanRedo() const { return history_.CanRedo(); }

    // レールの数や並びが変わった時に呼ぶ（レールの削除・その元に戻す/やり直し）。
    //   oldToNew[旧番号] = 新番号（-1 = そのレールは無くなった＝載っていた敵を外す）。
    //   revived はよみがえったレールに載っていた敵（新番号で）。付け加える。
    //   元に戻す/やり直しの履歴の中身も同じように付け替える（しないと、戻した時に別のレールへずれる）。
    //   付け替えそのものは履歴の1手に数えない（レール側の操作に付いてくる変化なので）
    void ApplyRailRemap(const std::vector<int>& oldToNew, const std::vector<EnemySpawnData>& revived);

    // レールの編集に合わせてシーンが距離を張り直した後に呼ぶ：その張り直しを履歴の1手に数えない
    //   （張り直す前に、まだ履歴に積んでいない利用者の変更が無い時だけ呼ぶこと）
    bool IsHistoryClean() const { return history_.IsClean(model_.Datas()); }
    void RebaseHistory() { history_.Rebase(model_.Datas()); }

    // 「カメラをこの敵へ」ボタンの要求を取り出す（あれば true。シーンがカメラを動かす）
    bool ConsumeFocusRequest(int& outIndex) { return model_.ConsumeFocusRequest(outIndex); }

    // 配置情報の取得
    const std::vector<EnemySpawnData>& GetSpawnDatas() const { return model_.Datas(); }

    // レール編集時の「距離の張り直し」用に直接書き換えるアクセサ。
    //   SetSpawnDatas と違い選択状態や changed_ フラグを触らない（UI の選択が飛ばない）。
    std::vector<EnemySpawnData>& MutableSpawnDatas() { return model_.MutableDatas(); }

    // 配置情報の差し替え（マップ読込時に外部データで上書きする）
    void SetSpawnDatas(const std::vector<EnemySpawnData>& datas);

    // 配置データに変更があったかどうか（取得するとフラグはリセットされる）
    bool ConsumeChanged() { return model_.ConsumeChanged(); }

    // 一覧・ピンに出す表示名（名前が空なら種類名）
    static std::string DisplayName(const EnemySpawnData& spawn);
    // 敵タイプ名の取得ヘルパー（短縮版と表示版）
    static const char* GetTypeName(EnemyType type);
    static const char* GetTypeLabel(EnemyType type);

private:
    // ウィンドウ最上段：元に戻す／やり直しと体数
    void DrawHistoryRow();

    EnemySpawnModel      model_;     // 配置データの正本（選択・変更通知・操作つき）
    EnemySpawnHistory    history_;   // 元に戻す／やり直し
    EnemyEditorAddPanel  addPanel_;  // 上段：新しく置く
    EnemyEditorList      list_;      // 中段：一覧
    EnemyEditorInspector inspector_; // 下段：選択中の1体の設定
};
