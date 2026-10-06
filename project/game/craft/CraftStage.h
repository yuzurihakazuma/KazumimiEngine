#pragma once
// =====================================================================
//  CraftStage：クラフトのステージ（箱庭の配置）のデータ。1ステージ1ファイルの JSON。
//   ゲームはこのデータを読んで描くだけ、エディタはコマンド（CraftCommands）を通してしか書き換えない。
//
//   ファイル（resources/stage/<名前>.stage.json）：
//     version / name / environment（光・背景の名前）/ objects / markers
//     objects … id（生成時に1回だけ作る64bitの乱数。16進）/ asset（manifest の名前）/ layer /
//               pos / rot（度）/ scale / params（ゲームオブジェクトだけが持つ自由な設定）
//   書き出しは id 順・キーの順番も固定にする（Git の差分が変えた所だけになる）。
//
//   保存は「一時ファイルへ書く → 今のファイルを backup/ へ日時付きで移す（10個まで）→ 一時ファイルを本体へ改名」。
//   書き出しの途中で落ちても、壊れるのは一時ファイルだけで本体は無事
// =====================================================================
#include "engine/math/struct.h"
#include "externals/nlohmann/json.hpp"

#include <cstdint>
#include <string>
#include <vector>

// 層（エディタで層ごとに表示・ロックできる）
enum CraftLayer {
    CraftLayer_Backdrop, // 背景（空・山・雲・机）
    CraftLayer_Ground,   // 地面（タイル・段差）
    CraftLayer_Decor,    // 飾り（木・草・花・小物）
    CraftLayer_Gameplay, // 敵・卵・ギミック
    CraftLayer_Count
};
const char* CraftLayerName(int layer);       // ファイルに書く名前（backdrop など）
const char* CraftLayerDisplayName(int layer); // 画面に出す名前（背景 など）

struct CraftObject {
    uint64_t    id = 0;
    std::string asset;
    int         layer = CraftLayer_Decor;
    Vector3     position {};
    Vector3     rotationDeg {};   // 度数法のオイラー角（人が読んで分かるように）
    Vector3     scale { 1.0f, 1.0f, 1.0f };
    nlohmann::json params = nlohmann::json::object();
};

struct CraftMarker {
    uint64_t    id = 0;
    std::string name;
    Vector3     position {};
    Vector3     rotationDeg {};
};

class CraftStage {
public:
    static constexpr int kVersion = 1;

    // --- 読み書き ---
    // 読み込み。壊れていたら false（今のデータはそのまま）。警告は GetLoadWarnings で取れる
    bool Load(const std::string& path);
    // 保存（一時ファイル → バックアップ → 改名）。成功で true
    bool Save(const std::string& path) const;
    // 自動保存（autosave/ へ書く。本体は上書きしない）
    bool SaveAutosave(const std::string& path) const;
    static std::string AutosavePath(const std::string& path);
    static std::string LatestBackupPath(const std::string& path); // 一番新しいバックアップ（無ければ空）
    const std::vector<std::string>& GetLoadWarnings() const{ return warnings_; }

    // --- 中身 ---
    const std::string& GetName() const{ return name_; }
    void SetName(const std::string& name){ name_ = name; }
    const std::vector<CraftObject>& GetObjects() const{ return objects_; }
    const CraftObject* Find(uint64_t id) const;
    CraftObject* FindMutable(uint64_t id);
    const std::vector<CraftMarker>& GetMarkers() const{ return markers_; }
    nlohmann::json& RefEnvironment(){ return environment_; }
    const nlohmann::json& GetEnvironment() const{ return environment_; }

    // --- 書き換え（エディタのコマンドから呼ぶ）---
    void Add(const CraftObject& object);
    bool Remove(uint64_t id, CraftObject* removed = nullptr);
    void Clear();

    static uint64_t NewId();
    static std::string IdToString(uint64_t id);

    // 中身が変わるたびに増える番号（描画側が作り直す合図）
    int GetVersion() const{ return version_; }
    void Touch(){ ++version_; }

private:
    nlohmann::ordered_json ToJson() const;

    std::string name_ = "stage";
    nlohmann::json environment_ = nlohmann::json::object();
    std::vector<CraftObject> objects_;
    std::vector<CraftMarker> markers_;
    std::vector<std::string> warnings_;
    int version_ = 0;
};
