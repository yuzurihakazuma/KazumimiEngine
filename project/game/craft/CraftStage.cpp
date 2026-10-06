#include "game/craft/CraftStage.h"

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <random>
#include <set>

using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;
namespace fs = std::filesystem;

namespace {
    constexpr int kBackupKeep = 10; // バックアップを残す数

    const char* kLayerNames[CraftLayer_Count] = { "backdrop", "ground", "decor", "gameplay" };
    const char* kLayerDisplayNames[CraftLayer_Count] = { "背景", "地面", "飾り", "ゲーム" };

    Vector3 ReadVector(const json& value, const Vector3& fallback){
        if ( !value.is_array() || value.size() < 3 ) { return fallback; }
        return { value[0].get<float>(), value[1].get<float>(), value[2].get<float>() };
    }
    ordered_json WriteVector(const Vector3& value){
        // 小数の細かい揺れで差分が出ないよう、4桁に丸めて書く
        auto round4 = [](float x){ return std::round(x * 10000.0f) / 10000.0f; };
        return ordered_json::array({ round4(value.x), round4(value.y), round4(value.z) });
    }
    uint64_t ParseId(const json& value){
        if ( value.is_string() ) {
            try { return std::stoull(value.get<std::string>(), nullptr, 16); } catch ( ... ) { return 0; }
        }
        if ( value.is_number_unsigned() ) { return value.get<uint64_t>(); }
        return 0;
    }
    int ParseLayer(const std::string& name){
        for ( int i = 0; i < CraftLayer_Count; ++i ) {
            if ( name == kLayerNames[i] ) { return i; }
        }
        return CraftLayer_Decor;
    }
    std::wstring Widen(const std::string& text){
        return fs::path(text).wstring();
    }
}

const char* CraftLayerName(int layer){
    return ( layer >= 0 && layer < CraftLayer_Count ) ? kLayerNames[layer] : "decor";
}
const char* CraftLayerDisplayName(int layer){
    return ( layer >= 0 && layer < CraftLayer_Count ) ? kLayerDisplayNames[layer] : "?";
}

uint64_t CraftStage::NewId(){
    static std::mt19937_64 engine { std::random_device{}() ^ ( uint64_t ) std::chrono::steady_clock::now().time_since_epoch().count() };
    uint64_t id = 0;
    while ( id == 0 ) { id = engine(); }
    return id;
}

std::string CraftStage::IdToString(uint64_t id){
    char text[17] = {};
    std::snprintf(text, sizeof(text), "%016llx", ( unsigned long long ) id);
    return text;
}

const CraftObject* CraftStage::Find(uint64_t id) const{
    for ( const CraftObject& object : objects_ ) { if ( object.id == id ) { return &object; } }
    return nullptr;
}

CraftObject* CraftStage::FindMutable(uint64_t id){
    for ( CraftObject& object : objects_ ) { if ( object.id == id ) { return &object; } }
    return nullptr;
}

void CraftStage::Add(const CraftObject& object){
    objects_.push_back(object);
    Touch();
}

bool CraftStage::Remove(uint64_t id, CraftObject* removed){
    for ( size_t i = 0; i < objects_.size(); ++i ) {
        if ( objects_[i].id != id ) { continue; }
        if ( removed ) { *removed = objects_[i]; }
        objects_.erase(objects_.begin() + i);
        Touch();
        return true;
    }
    return false;
}

void CraftStage::Clear(){
    objects_.clear();
    markers_.clear();
    environment_ = json::object();
    Touch();
}

// =====================================================================
//  読み込み
// =====================================================================
bool CraftStage::Load(const std::string& path){
    warnings_.clear();
    std::ifstream file(path);
    if ( !file.is_open() ) {
        warnings_.push_back("ファイルがありません（空のステージとして始めます）: " + path);
        Clear();
        name_ = fs::path(path).stem().stem().string();
        return true;
    }
    json root;
    try { file >> root; } catch ( const std::exception& error ) {
        warnings_.push_back(std::string("JSON として壊れています。読み込みを中止しました: ") + error.what());
        return false;
    }

    const int fileVersion = root.value("version", 0);
    if ( fileVersion < kVersion ) {
        warnings_.push_back("古い形式（version " + std::to_string(fileVersion) + "）を新しい形式に変換して読みました。保存すると新しい形式になります");
    }

    std::vector<CraftObject> objects;
    std::set<uint64_t> usedIds;
    for ( const json& entry : root.value("objects", json::array()) ) {
        CraftObject object;
        object.id = ParseId(entry.value("id", json()));
        object.asset = entry.value("asset", "");
        object.layer = ParseLayer(entry.value("layer", "decor"));
        object.position = ReadVector(entry.value("pos", json()), {});
        object.rotationDeg = ReadVector(entry.value("rot", json()), {});
        object.scale = ReadVector(entry.value("scale", json()), { 1.0f, 1.0f, 1.0f });
        if ( entry.contains("params") && entry["params"].is_object() ) { object.params = entry["params"]; }
        // id が無い・重複している物は、後から来た方に新しい id を振る（参照が壊れないよう警告を出す）
        if ( object.id == 0 || usedIds.count(object.id) ) {
            const uint64_t oldId = object.id;
            object.id = NewId();
            if ( oldId != 0 ) { warnings_.push_back("id の重複 " + IdToString(oldId) + " → " + IdToString(object.id) + " を振り直しました"); }
        }
        usedIds.insert(object.id);
        objects.push_back(object);
    }
    std::vector<CraftMarker> markers;
    for ( const json& entry : root.value("markers", json::array()) ) {
        CraftMarker marker;
        marker.id = ParseId(entry.value("id", json()));
        if ( marker.id == 0 ) { marker.id = NewId(); }
        marker.name = entry.value("name", "");
        marker.position = ReadVector(entry.value("pos", json()), {});
        marker.rotationDeg = ReadVector(entry.value("rot", json()), {});
        markers.push_back(marker);
    }

    name_ = root.value("name", fs::path(path).stem().stem().string());
    environment_ = root.value("environment", json::object());
    objects_ = std::move(objects);
    markers_ = std::move(markers);
    Touch();
    return true;
}

// =====================================================================
//  書き出し
// =====================================================================
ordered_json CraftStage::ToJson() const{
    ordered_json root;
    root["version"] = kVersion;
    root["name"] = name_;
    root["environment"] = environment_;
    // id 順に並べる（並べ替えや削除をしても、変えた物だけが差分に出る）
    std::vector<const CraftObject*> sorted;
    for ( const CraftObject& object : objects_ ) { sorted.push_back(&object); }
    std::sort(sorted.begin(), sorted.end(), [](const CraftObject* a, const CraftObject* b){ return a->id < b->id; });
    ordered_json objects = ordered_json::array();
    for ( const CraftObject* object : sorted ) {
        ordered_json entry;
        entry["id"] = IdToString(object->id);
        entry["asset"] = object->asset;
        entry["layer"] = CraftLayerName(object->layer);
        entry["pos"] = WriteVector(object->position);
        entry["rot"] = WriteVector(object->rotationDeg);
        entry["scale"] = WriteVector(object->scale);
        if ( !object->params.empty() ) { entry["params"] = object->params; }
        objects.push_back(entry);
    }
    root["objects"] = objects;
    ordered_json markers = ordered_json::array();
    for ( const CraftMarker& marker : markers_ ) {
        ordered_json entry;
        entry["id"] = IdToString(marker.id);
        entry["name"] = marker.name;
        entry["pos"] = WriteVector(marker.position);
        entry["rot"] = WriteVector(marker.rotationDeg);
        markers.push_back(entry);
    }
    root["markers"] = markers;
    return root;
}

bool CraftStage::Save(const std::string& path) const{
    std::error_code error;
    const fs::path target(path);
    fs::create_directories(target.parent_path(), error);

    // 1. 一時ファイルに全部書く
    const std::string temporary = path + ".tmp";
    {
        std::ofstream file(temporary, std::ios::trunc);
        if ( !file.is_open() ) { return false; }
        file << ToJson().dump(2);
        file.flush();
        if ( !file.good() ) { return false; }
    }

    // 2. 今の本体を backup/ へ日時付きで移す（新しい10個だけ残す）
    if ( fs::exists(target, error) ) {
        const fs::path backupDirectory = target.parent_path() / "backup";
        fs::create_directories(backupDirectory, error);
        const std::time_t now = std::time(nullptr);
        std::tm localTime {};
        localtime_s(&localTime, &now);
        char stamp[32] = {};
        std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &localTime);
        const fs::path backup = backupDirectory / ( target.stem().string() + "_" + stamp + target.extension().string() );
        fs::copy_file(target, backup, fs::copy_options::overwrite_existing, error);

        std::vector<fs::path> backups;
        const std::string prefix = target.stem().string() + "_";
        for ( const auto& entry : fs::directory_iterator(backupDirectory, error) ) {
            if ( entry.path().filename().string().rfind(prefix, 0) == 0 ) { backups.push_back(entry.path()); }
        }
        std::sort(backups.begin(), backups.end()); // 名前＝日時順
        while ( ( int ) backups.size() > kBackupKeep ) {
            fs::remove(backups.front(), error);
            backups.erase(backups.begin());
        }
    }

    // 3. 一時ファイルを本体へ改名（上書き）。ここで初めて本体が新しくなる
    return MoveFileExW(Widen(temporary).c_str(), Widen(path).c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

std::string CraftStage::AutosavePath(const std::string& path){
    const fs::path target(path);
    return ( target.parent_path() / "autosave" / target.filename() ).string();
}

bool CraftStage::SaveAutosave(const std::string& path) const{
    const std::string autosave = AutosavePath(path);
    std::error_code error;
    fs::create_directories(fs::path(autosave).parent_path(), error);
    std::ofstream file(autosave, std::ios::trunc);
    if ( !file.is_open() ) { return false; }
    file << ToJson().dump(2);
    return file.good();
}

std::string CraftStage::LatestBackupPath(const std::string& path){
    const fs::path target(path);
    const fs::path backupDirectory = target.parent_path() / "backup";
    std::error_code error;
    std::vector<fs::path> backups;
    const std::string prefix = target.stem().string() + "_";
    for ( const auto& entry : fs::directory_iterator(backupDirectory, error) ) {
        if ( entry.path().filename().string().rfind(prefix, 0) == 0 ) { backups.push_back(entry.path()); }
    }
    if ( backups.empty() ) { return {}; }
    std::sort(backups.begin(), backups.end());
    return backups.back().string();
}
