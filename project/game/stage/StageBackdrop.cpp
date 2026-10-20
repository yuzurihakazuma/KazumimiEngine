#include "game/stage/StageBackdrop.h"

#include "engine/3d/model/Model.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/camera/Camera.h"
#include "engine/graphics/InstancedGroup.h"
#include "engine/math/Matrix4x4.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

using json = nlohmann::json;

namespace {
    constexpr const char* kKitModelDir = "resources/craft/models";
    constexpr float kPi = 3.14159265358979323846f;
    constexpr const char* kGroundModel = "ground_block_step";
    constexpr const char* kPathModel   = "ground_block_path"; // ground_block_step の上の面を紙の道に
    constexpr const char* kDirtModel   = "ground_block_dirt"; // ground_block_step の上の面を段ボール（土）に
    constexpr float kBlockWidth = 2.02f;   // ground_block_step の幅（make_stage_backdrop.py の BLOCK_W）
    constexpr float kBlockHeight = 0.62f;  // 同じく高さ（上面までの高さ）
    constexpr float kBlockOverlap = 1.005f; // すき間が見えないよう少しだけ大きく
    bool IsGroundModel(const std::string& model){ return model == kGroundModel || model == kPathModel || model == kDirtModel; }
    constexpr float kMatchDistance = 0.3f; // 手直しを自動配置へ当てる時、元の位置がこれだけ近ければ同じ物

    Vector3 ReadVector(const json& value, const Vector3& fallback){
        if ( value.is_number() ) { const float v = value.get<float>(); return { v, v, v }; }
        if ( !value.is_array() || value.size() < 3 ) { return fallback; }
        return { value[0].get<float>(), value[1].get<float>(), value[2].get<float>() };
    }
    json WriteVector(const Vector3& v){
        auto round3 = [](float f){ return std::round(( double ) f * 1000.0) / 1000.0; }; // 保存は mm まで（float のままだと桁が長くなる）
        return json::array({ round3(v.x), round3(v.y), round3(v.z) });
    }
    bool Near(const Vector3& a, const Vector3& b, float eps){
        return std::abs(a.x - b.x) <= eps && std::abs(a.y - b.y) <= eps && std::abs(a.z - b.z) <= eps;
    }
    float Distance(const Vector3& a, const Vector3& b){
        const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }
    bool SameTransform(const BackdropItem& a, const BackdropItem& b){
        return Near(a.pos, b.pos, 0.001f) && Near(a.rot, b.rot, 0.01f) && Near(a.scale, b.scale, 0.001f);
    }
    BackdropItem ReadItem(const json& entry, const Vector3& origin){
        BackdropItem item;
        item.model = entry.value("model", std::string());
        item.pos = ReadVector(entry.value("pos", json()), { 0.0f, 0.0f, 0.0f });
        if ( entry.contains("offset") ) {
            const Vector3 offset = ReadVector(entry["offset"], { 0.0f, 0.0f, 0.0f });
            item.pos = { origin.x + offset.x, origin.y + offset.y, origin.z + offset.z };
        }
        item.scale = ReadVector(entry.value("scale", json(1.0f)), { 1.0f, 1.0f, 1.0f });
        item.rot = { 0.0f, entry.value("rotY", 180.0f), 0.0f };
        if ( entry.contains("rot") ) { item.rot = ReadVector(entry["rot"], item.rot); }
        item.group = entry.value("group", 0);
        return item;
    }
    json WriteItem(const BackdropItem& item){
        json entry { { "model", item.model }, { "pos", WriteVector(item.pos) }, { "rot", WriteVector(item.rot) },
                     { "scale", WriteVector(item.scale) } };
        if ( item.group != 0 ) { entry["group"] = item.group; }
        return entry;
    }
}

bool BackdropItem::operator==(const BackdropItem& other) const{
    return model == other.model && source == other.source && removed == other.removed && group == other.group && SameTransform(*this, other);
}

StageBackdrop::StageBackdrop() = default;
StageBackdrop::~StageBackdrop() = default;

void StageBackdrop::LoadKitModels(){
    ModelManager* modelManager = ModelManager::GetInstance();
    for ( const std::string& modelName : KitModelNames() ) {
        modelManager->LoadModel(modelName, kKitModelDir, modelName + ".gltf", true); // 底面原点のまま
        // 紙・フェルト・段ボールなので、白い照り返しを出さない
        if ( Model* model = modelManager->FindModel(modelName) ) {
            if ( model->GetMaterial() ) { model->GetMaterial()->matte = 1.0f; }
        }
    }
    // 地面のブロックの、上の面だけ違う物（紙の道・段ボールの土）。同じモデルを別名で読んで上の面の画像を差し替える
    const std::pair<const char*, const char*> variants[] = {
        { kPathModel, "resources/craft/textures/paper_path.png" },
        { kDirtModel, "resources/craft/textures/kraft.png" },
    };
    for ( const auto& [name, texture] : variants ) {
        modelManager->LoadModel(name, kKitModelDir, std::string(kGroundModel) + ".gltf", true);
        if ( Model* model = modelManager->FindModel(name) ) {
            model->SetMaterialTexture("M_felt_grass", texture);
            if ( model->GetMaterial() ) { model->GetMaterial()->matte = 1.0f; }
        }
    }
    // 線で引く道の部品：1m の箱（帯）と、直径 1m・高さ 1m の円柱（曲がり角と端を丸くする）。
    //   上に紙の道、下に少し広い段ボールのふち
    auto makePart = [&](const char* name, bool disc, const char* texture, const Vector4& color){
        if ( !modelManager->FindModel(name) ) {
            if ( disc ) { modelManager->CreateCylinderModel(name, 28, 0.5f, 1.0f); }
            else        { modelManager->CreateCubeModel(name, 1.0f); }
        }
        if ( Model* model = modelManager->FindModel(name) ) {
            model->SetTexture(texture);
            if ( model->GetMaterial() ) {
                model->GetMaterial()->matte = 1.0f;
                model->GetMaterial()->color = color;
            }
        }
    };
    makePart(kPathStrip, false, "resources/craft/textures/paper_path.png", { 1.0f, 1.0f, 1.0f, 1.0f });
    makePart(kPathDisc,  true,  "resources/craft/textures/paper_path.png", { 1.0f, 1.0f, 1.0f, 1.0f });
    makePart(kPathEdgeStrip, false, "resources/craft/textures/kraft.png", { 0.85f, 0.75f, 0.62f, 1.0f });
    makePart(kPathEdgeDisc,  true,  "resources/craft/textures/kraft.png", { 0.85f, 0.75f, 0.62f, 1.0f });
}

std::vector<std::string> StageBackdrop::KitModelNames(){
    namespace fs = std::filesystem;
    std::vector<std::string> names;
    std::error_code errorCode;
    for ( const auto& entry : fs::directory_iterator(kKitModelDir, errorCode) ) {
        if ( !entry.is_regular_file() ) { continue; }
        std::string extension = entry.path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](unsigned char character){ return ( char ) std::tolower(character); });
        if ( extension == ".gltf" ) { names.push_back(entry.path().stem().string()); }
    }
    std::sort(names.begin(), names.end());
    return names;
}

void StageBackdrop::Build(const json& list, const Vector3& origin, uint32_t noiseSrv){
    Clear();
    noiseSrv_ = noiseSrv;
    if ( !list.is_array() ) { return; }
    for ( const json& entry : list ) {
        BackdropItem item = ReadItem(entry, origin);
        if ( !ModelManager::GetInstance()->FindModel(item.model) ) { continue; } // 読んでいないモデルは置かない
        item.source = ( int ) generated_.size();
        generated_.push_back(item);
    }
    SetItems(generated_);
}

void StageBackdrop::Clear(){
    generated_.clear();
    items_.clear();
    cache_.clear();
    groups_.clear();
    groupsDirty_ = true;
}

// ===== ファイル =====
std::string StageBackdrop::BasePath(const std::string& stage){ return "resources/map/" + stage + "_backdrop.json"; }
std::string StageBackdrop::EditPath(const std::string& stage){ return "resources/map/" + stage + "_backdrop_edit.json"; }

namespace {
    json ReadJsonFile(const std::string& path){
        std::ifstream file(path);
        json content = json::object(); // 無い・読めない時は空
        if ( file.is_open() ) {
            try { file >> content; } catch ( ... ) { content = json::object(); }
        }
        return content.is_object() ? content : json::object();
    }
}

void StageBackdrop::LoadStage(const std::string& stage, uint32_t noiseSrv, int* outMissed){
    const json base = ReadJsonFile(BasePath(stage));
    Build(base.value("objects", json::array()), { 0.0f, 0.0f, 0.0f }, noiseSrv);
    ReadGrid(base);
    const int missed = ApplyEdits(ReadJsonFile(EditPath(stage)));
    if ( outMissed ) { *outMissed = missed; }
}

void StageBackdrop::ReloadBase(const std::string& stage, const json& edits, int* outMissed){
    const json base = ReadJsonFile(BasePath(stage));
    Build(base.value("objects", json::array()), { 0.0f, 0.0f, 0.0f }, noiseSrv_);
    ReadGrid(base);
    const int missed = ApplyEdits(edits);
    if ( outMissed ) { *outMissed = missed; }
}

bool StageBackdrop::SaveEdits(const std::string& stage) const{
    namespace fs = std::filesystem;
    const fs::path path = EditPath(stage);
    const fs::path temp = path.string() + ".tmp";
    {
        std::ofstream file(temp, std::ios::binary | std::ios::trunc);
        if ( !file.is_open() ) { return false; }
        file << MakeEdits().dump(1);
        if ( !file.good() ) { return false; }
    }
    std::error_code errorCode;
    if ( fs::exists(path, errorCode) ) {
        // 前のファイルを backup/ へ（新しい順に5個まで）
        const fs::path backupDir = path.parent_path() / "backup";
        fs::create_directories(backupDir, errorCode);
        for ( int k = 4; k >= 1; --k ) {
            const fs::path from = backupDir / ( path.filename().string() + "." + std::to_string(k) );
            const fs::path to   = backupDir / ( path.filename().string() + "." + std::to_string(k + 1) );
            if ( fs::exists(from, errorCode) ) { fs::rename(from, to, errorCode); }
        }
        fs::copy_file(path, backupDir / ( path.filename().string() + ".1" ), fs::copy_options::overwrite_existing, errorCode);
    }
    fs::rename(temp, path, errorCode);
    if ( errorCode ) {
        fs::copy_file(temp, path, fs::copy_options::overwrite_existing, errorCode);
        fs::remove(temp, errorCode);
        return !errorCode;
    }
    return true;
}

// ===== 手直し =====
int StageBackdrop::ApplyEdits(const json& edits){
    if ( !edits.is_object() ) { return 0; }
    std::vector<BackdropItem> items = generated_;
    std::vector<bool> used(items.size(), false);
    int missed = 0;
    // 自動配置の中から、同じモデルで元の位置が一番近い物（まだ使っていない物）
    auto find = [&](const std::string& model, const Vector3& from) -> int {
        int best = -1;
        float bestDistance = kMatchDistance;
        for ( int i = 0; i < ( int ) generated_.size(); ++i ) {
            if ( used[i] || generated_[i].model != model ) { continue; }
            const float d = Distance(generated_[i].pos, from);
            if ( d <= bestDistance ) { bestDistance = d; best = i; }
        }
        return best;
    };
    for ( const json& entry : edits.value("removed", json::array()) ) {
        const int index = find(entry.value("model", std::string()), ReadVector(entry.value("pos", json()), {}));
        if ( index < 0 ) { ++missed; continue; }
        used[index] = true;
        items[index].removed = true;
    }
    for ( const json& entry : edits.value("moved", json::array()) ) {
        const int index = find(entry.value("model", std::string()), ReadVector(entry.value("from", json()), {}));
        if ( index < 0 ) { ++missed; continue; }
        used[index] = true;
        BackdropItem moved = ReadItem(entry, {});
        items[index].pos = moved.pos;
        items[index].rot = moved.rot;
        items[index].scale = moved.scale;
    }
    for ( const json& entry : edits.value("added", json::array()) ) {
        BackdropItem item = ReadItem(entry, {});
        if ( !ModelManager::GetInstance()->FindModel(item.model) ) { ++missed; continue; }
        item.source = -1;
        items.push_back(item);
    }
    SetItems(items);
    return missed;
}

json StageBackdrop::MakeEdits() const{
    json added = json::array(), removed = json::array(), moved = json::array();
    for ( const BackdropItem& item : items_ ) {
        if ( item.source < 0 ) {
            if ( !item.removed ) { added.push_back(WriteItem(item)); }
            continue;
        }
        const BackdropItem& original = generated_[item.source];
        if ( item.removed ) {
            removed.push_back(json { { "model", original.model }, { "pos", WriteVector(original.pos) } });
        } else if ( !SameTransform(item, original) ) {
            json entry = WriteItem(item);
            entry["from"] = WriteVector(original.pos);
            moved.push_back(entry);
        }
    }
    return json { { "added", added }, { "removed", removed }, { "moved", moved } };
}

bool StageBackdrop::HasEdits() const{
    const json edits = MakeEdits();
    return !edits["added"].empty() || !edits["removed"].empty() || !edits["moved"].empty();
}

void StageBackdrop::SetItems(const std::vector<BackdropItem>& items){
    items_ = items;
    cache_.assign(items_.size(), Cache {});
    for ( int i = 0; i < ( int ) items_.size(); ++i ) { UpdateCache(i); }
    groupsDirty_ = true;
}

void StageBackdrop::SetItem(int index, const BackdropItem& item){
    if ( index < 0 || index >= ( int ) items_.size() ) { return; }
    const bool membership = items_[index].model != item.model || items_[index].removed != item.removed;
    items_[index] = item;
    UpdateCache(index);
    if ( membership ) { groupsDirty_ = true; }
}

int StageBackdrop::AddItem(const BackdropItem& item){
    items_.push_back(item);
    items_.back().source = -1;
    items_.back().removed = false;
    cache_.push_back(Cache {});
    UpdateCache(( int ) items_.size() - 1);
    groupsDirty_ = true;
    return ( int ) items_.size() - 1;
}

void StageBackdrop::RemoveItem(int index){
    if ( index < 0 || index >= ( int ) items_.size() ) { return; }
    if ( items_[index].source >= 0 ) {
        items_[index].removed = true; // 自動配置の物は「消した」と覚えておく（作り直しても消えたまま）
    } else {
        items_.erase(items_.begin() + index);
        cache_.erase(cache_.begin() + index);
    }
    groupsDirty_ = true;
}

bool StageBackdrop::IsGround(int index) const{
    return index >= 0 && index < ( int ) items_.size() && IsGroundModel(items_[index].model)
        && std::abs(items_[index].rot.x) < 1.0f;
}

bool StageBackdrop::IsFloor(int index) const{
    // 段ボールの底＝裏返した地面のブロック（穴の底に見せる物。さわらない）
    return index >= 0 && index < ( int ) items_.size() && IsGroundModel(items_[index].model)
        && std::abs(items_[index].rot.x) >= 1.0f;
}

// ===== 地面（升目）=====
const char* StageBackdrop::GroundModel(GroundKind kind){
    switch ( kind ) {
    case GroundKind::Path: return kPathModel;
    case GroundKind::Dirt: return kDirtModel;
    default:               return kGroundModel;
    }
}

void StageBackdrop::ReadGrid(const json& content){
    gridCell_ = 1.2f;
    gridOriginX_ = gridOriginZ_ = 0.0f;
    if ( content.contains("grid") ) {
        const json& grid = content["grid"];
        gridCell_ = grid.value("cell", 1.2f);
        if ( grid.contains("origin") && grid["origin"].size() >= 2 ) {
            gridOriginX_ = grid["origin"][0].get<float>();
            gridOriginZ_ = grid["origin"][1].get<float>();
        }
        return;
    }
    // 古い自動配置（grid が無い）：地面のブロックの位置から升目の位置を推測する
    for ( const BackdropItem& item : generated_ ) {
        if ( !IsGroundModel(item.model) || std::abs(item.rot.x) >= 1.0f ) { continue; }
        int width, depth;
        BlockSize(item, width, depth);
        gridOriginX_ = std::fmod(item.pos.x - width * gridCell_ * 0.5f, gridCell_);
        gridOriginZ_ = std::fmod(item.pos.z - depth * gridCell_ * 0.5f, gridCell_);
        return;
    }
}

void StageBackdrop::BlockSize(const BackdropItem& item, int& outWidth, int& outDepth) const{
    outWidth = std::clamp(( int ) std::lround(item.scale.x * kBlockWidth / kBlockOverlap / gridCell_), 1, 8);
    outDepth = std::clamp(( int ) std::lround(item.scale.z * kBlockWidth / kBlockOverlap / gridCell_), 1, 8);
}

int StageBackdrop::NextGroup() const{
    int next = 1;
    for ( const BackdropItem& item : items_ ) { next = ( std::max )( next, item.group + 1 ); }
    return next;
}

void StageBackdrop::MergeCells(int column0, int row0, int column1, int row1){
    // 範囲の中の、1升ブロック（消していない地面）を升目ごとに
    struct Cell { int index; std::string model; float top; };
    std::map<std::pair<int, int>, Cell> cells;
    for ( int i = 0; i < ( int ) items_.size(); ++i ) {
        if ( items_[i].removed || !IsGround(i) ) { continue; }
        int width, depth;
        BlockSize(items_[i], width, depth);
        if ( width != 1 || depth != 1 ) { continue; }
        int column, row;
        CellOf(items_[i].pos.x, items_[i].pos.z, column, row);
        if ( column < column0 || column > column1 || row < row0 || row > row1 ) { continue; }
        const float top = std::round(( items_[i].pos.y + kBlockHeight * items_[i].scale.y ) * 100.0f) / 100.0f;
        cells[{ column, row }] = { i, items_[i].model, top };
    }
    // 大きい長方形から順に、同じ種類・同じ高さの升目だけでまとめる
    static const int kShapes[][2] = { { 3, 3 }, { 3, 2 }, { 2, 3 }, { 2, 2 }, { 3, 1 }, { 1, 3 }, { 2, 1 }, { 1, 2 } };
    std::vector<int> toRemove;
    std::vector<BackdropItem> toAdd;
    std::set<std::pair<int, int>> used;
    for ( auto& [key, cell] : cells ) {
        if ( used.count(key) ) { continue; }
        for ( const auto& shape : kShapes ) {
            bool fits = true;
            for ( int a = 0; a < shape[0] && fits; ++a ) {
                for ( int b = 0; b < shape[1] && fits; ++b ) {
                    auto found = cells.find({ key.first + a, key.second + b });
                    fits = found != cells.end() && !used.count(found->first)
                        && found->second.model == cell.model && found->second.top == cell.top;
                }
            }
            if ( !fits ) { continue; }
            BackdropItem merged = items_[cell.index];
            const Vector3 first = CellCenter(key.first, key.second);
            merged.pos.x = first.x + ( shape[0] - 1 ) * gridCell_ * 0.5f;
            merged.pos.z = first.z + ( shape[1] - 1 ) * gridCell_ * 0.5f;
            merged.scale.x = shape[0] * gridCell_ / kBlockWidth * kBlockOverlap;
            merged.scale.z = shape[1] * gridCell_ / kBlockWidth * kBlockOverlap;
            merged.source = -1;
            for ( int a = 0; a < shape[0]; ++a ) {
                for ( int b = 0; b < shape[1]; ++b ) {
                    used.insert({ key.first + a, key.second + b });
                    toRemove.push_back(cells[{ key.first + a, key.second + b }].index);
                }
            }
            toAdd.push_back(merged);
            break;
        }
    }
    if ( toRemove.empty() ) { return; }
    std::sort(toRemove.rbegin(), toRemove.rend()); // 後ろから（手で足した物は取り除かれて番号が詰まる）
    for ( int index : toRemove ) { RemoveItem(index); }
    for ( const BackdropItem& item : toAdd ) { AddItem(item); }
}

void StageBackdrop::CellOf(float x, float z, int& outColumn, int& outRow) const{
    outColumn = ( int ) std::floor(( x - gridOriginX_ ) / gridCell_);
    outRow    = ( int ) std::floor(( z - gridOriginZ_ ) / gridCell_);
}

Vector3 StageBackdrop::CellCenter(int column, int row) const{
    return { gridOriginX_ + ( column + 0.5f ) * gridCell_, 0.0f, gridOriginZ_ + ( row + 0.5f ) * gridCell_ };
}

int StageBackdrop::GroundAtCell(int column, int row) const{
    const Vector3 center = CellCenter(column, row);
    int best = -1;
    float bestTop = -1e9f;
    for ( int i = 0; i < ( int ) items_.size(); ++i ) {
        if ( items_[i].removed || !IsGround(i) || !cache_[i].hasBounds ) { continue; }
        const Cache& cache = cache_[i];
        if ( center.x < cache.boundsMin.x || center.x > cache.boundsMax.x || center.z < cache.boundsMin.z || center.z > cache.boundsMax.z ) { continue; }
        const float top = items_[i].pos.y + kBlockHeight * items_[i].scale.y;
        if ( top > bestTop ) { bestTop = top; best = i; }
    }
    return best;
}

bool StageBackdrop::CellTop(int column, int row, float& outTop, GroundKind* outKind) const{
    const int index = GroundAtCell(column, row);
    if ( index < 0 ) { return false; }
    outTop = items_[index].pos.y + kBlockHeight * items_[index].scale.y;
    if ( outKind ) {
        const std::string& model = items_[index].model;
        *outKind = model == kPathModel ? GroundKind::Path : model == kDirtModel ? GroundKind::Dirt : GroundKind::Grass;
    }
    return true;
}

int StageBackdrop::SplitToCell(int index, int column, int row){
    const BackdropItem block = items_[index];
    int width, depth;
    BlockSize(block, width, depth);
    if ( width <= 1 && depth <= 1 ) { return index; }
    // まとめたブロックを1升ずつに割る（元は消して、同じ高さ・同じ種類の1升ブロックを並べる）
    int firstColumn, firstRow;
    CellOf(block.pos.x - ( width - 1 ) * gridCell_ * 0.5f, block.pos.z - ( depth - 1 ) * gridCell_ * 0.5f, firstColumn, firstRow);
    RemoveItem(index);
    int result = -1;
    const float scaleXZ = gridCell_ / kBlockWidth * kBlockOverlap;
    for ( int a = 0; a < width; ++a ) {
        for ( int b = 0; b < depth; ++b ) {
            BackdropItem piece = block;
            const Vector3 center = CellCenter(firstColumn + a, firstRow + b);
            piece.pos = { center.x, block.pos.y, center.z };
            piece.scale = { scaleXZ, block.scale.y, scaleXZ };
            const int added = AddItem(piece);
            if ( firstColumn + a == column && firstRow + b == row ) { result = added; }
        }
    }
    return result;
}

void StageBackdrop::PaintCell(int column, int row, GroundKind kind, float top){
    int index = GroundAtCell(column, row);
    if ( index >= 0 ) { index = SplitToCell(index, column, row); }
    const float scaleY = 1.8f; // 自動配置の BLOCK_SY と同じ厚み
    BackdropItem block;
    if ( index >= 0 ) {
        block = items_[index];
    } else {
        const Vector3 center = CellCenter(column, row);
        const float scaleXZ = gridCell_ / kBlockWidth * kBlockOverlap;
        block.pos = { center.x, 0.0f, center.z };
        block.rot = { 0.0f, 180.0f, 0.0f };
        block.scale = { scaleXZ, scaleY, scaleXZ };
    }
    block.model = GroundModel(kind);
    block.pos.y = top - kBlockHeight * block.scale.y;
    if ( index >= 0 ) {
        if ( !( items_[index] == block ) ) { SetItem(index, block); }
    } else {
        AddItem(block);
    }
}

void StageBackdrop::EraseCell(int column, int row){
    // 重なっている地面があれば全部（段を上げた所の下に残った物も）
    for ( int guard = 0; guard < 8; ++guard ) {
        int index = GroundAtCell(column, row);
        if ( index < 0 ) { return; }
        index = SplitToCell(index, column, row);
        if ( index < 0 ) { return; }
        RemoveItem(index);
    }
}

// ===== 行列 =====
Matrix4x4 StageBackdrop::MakeWorld(const BackdropItem& item){
    const float k = kPi / 180.0f;
    return MatrixMath::MakeAffine(item.scale, { item.rot.x * k, item.rot.y * k, item.rot.z * k }, item.pos); // Obj3d::Update と同じ作り方
}

void StageBackdrop::UpdateCache(int index){
    Cache& cache = cache_[index];
    cache.world = MakeWorld(items_[index]);
    cache.inverse = MatrixMath::Inverse(cache.world);
    cache.inverseTranspose = MatrixMath::Transpose(cache.inverse);
    // 世界での箱（地面の高さ探し・範囲選択で毎フレーム使うので、変わった時だけ計算）
    Vector3 lo, hi;
    cache.hasBounds = LocalBounds(items_[index].model, lo, hi);
    if ( cache.hasBounds ) {
        for ( int k = 0; k < 8; ++k ) {
            const Vector3 corner = MatrixMath::Transforms({ ( k & 1 ) ? hi.x : lo.x, ( k & 2 ) ? hi.y : lo.y, ( k & 4 ) ? hi.z : lo.z }, cache.world);
            if ( k == 0 ) { cache.boundsMin = cache.boundsMax = corner; continue; }
            cache.boundsMin = { ( std::min )( cache.boundsMin.x, corner.x ), ( std::min )( cache.boundsMin.y, corner.y ), ( std::min )( cache.boundsMin.z, corner.z ) };
            cache.boundsMax = { ( std::max )( cache.boundsMax.x, corner.x ), ( std::max )( cache.boundsMax.y, corner.y ), ( std::max )( cache.boundsMax.z, corner.z ) };
        }
    }
}

void StageBackdrop::RebuildGroups(){
    // モデルごとに、消していない物の番号を集める。GPU の入れ物は、足りない時だけ余裕を持たせて作り直す
    std::map<std::string, std::vector<int>> byModel;
    for ( int i = 0; i < ( int ) items_.size(); ++i ) {
        if ( !items_[i].removed ) { byModel[items_[i].model].push_back(i); }
    }
    std::vector<Group> next;
    for ( auto& [model, indices] : byModel ) {
        Group group;
        group.model = model;
        group.items = std::move(indices);
        for ( Group& old : groups_ ) {
            if ( old.model == model && old.batch && old.capacity >= group.items.size() ) {
                group.batch = std::move(old.batch);
                group.capacity = old.capacity;
                break;
            }
        }
        if ( !group.batch ) {
            group.capacity = ( uint32_t ) group.items.size() + ( uint32_t ) group.items.size() / 2 + 32;
            group.batch = std::make_unique<InstancedGroup>();
            group.batch->Initialize(model, group.capacity);
            group.batch->SetNoiseTexture(noiseSrv_);
        }
        next.push_back(std::move(group));
    }
    groups_ = std::move(next);
    groupsDirty_ = false;
}

void StageBackdrop::Update(const Camera* camera){
    if ( !camera ) { return; }
    if ( groupsDirty_ ) { RebuildGroups(); }
    const Matrix4x4& viewProjection = camera->GetViewProjectionMatrix();
    for ( Group& group : groups_ ) {
        group.matrices.resize(group.items.size());
        for ( size_t k = 0; k < group.items.size(); ++k ) {
            const Cache& cache = cache_[group.items[k]];
            Obj3d::TransformationMatrix& matrix = group.matrices[k];
            matrix.World = cache.world;
            matrix.WorldInverseTranspose = cache.inverseTranspose;
            matrix.WVP = MatrixMath::Multiply(cache.world, viewProjection);
        }
        group.batch->Update(group.matrices);
    }
}

void StageBackdrop::Draw(const Camera* camera){
    for ( Group& group : groups_ ) { group.batch->Draw(camera); }
}

size_t StageBackdrop::GetObjectCount() const{
    size_t count = 0;
    for ( const BackdropItem& item : items_ ) { count += item.removed ? 0 : 1; }
    return count;
}

// ===== 選ぶ（光線と箱） =====
bool StageBackdrop::LocalBounds(const std::string& model, Vector3& outMin, Vector3& outMax) const{
    auto found = boundsCache_.find(model);
    if ( found == boundsCache_.end() ) {
        Model* data = ModelManager::GetInstance()->FindModel(model);
        if ( !data || data->GetModelData().vertices.empty() ) { return false; }
        Vector3 lo { 1e9f, 1e9f, 1e9f }, hi { -1e9f, -1e9f, -1e9f };
        for ( const auto& vertex : data->GetModelData().vertices ) {
            lo = { ( std::min )( lo.x, vertex.position.x ), ( std::min )( lo.y, vertex.position.y ), ( std::min )( lo.z, vertex.position.z ) };
            hi = { ( std::max )( hi.x, vertex.position.x ), ( std::max )( hi.y, vertex.position.y ), ( std::max )( hi.z, vertex.position.z ) };
        }
        // 紙の切り絵は厚みがほぼ無いので、少しだけ厚くして当てやすくする
        const float pad = 0.04f;
        lo = { lo.x - pad, lo.y - pad, lo.z - pad };
        hi = { hi.x + pad, hi.y + pad, hi.z + pad };
        found = boundsCache_.emplace(model, std::make_pair(lo, hi)).first;
    }
    outMin = found->second.first;
    outMax = found->second.second;
    return true;
}

bool StageBackdrop::RayHit(int index, const Vector3& origin, const Vector3& dir, float& outT) const{
    Vector3 lo, hi;
    if ( !LocalBounds(items_[index].model, lo, hi) ) { return false; }
    // 光線を物の中の座標へ（行列は拡縮も含むので、長さ t は世界と同じ尺度のまま）
    const Matrix4x4& inverse = cache_[index].inverse;
    const Vector3 o = MatrixMath::Transforms(origin, inverse);
    const Vector3 e = MatrixMath::Transforms({ origin.x + dir.x, origin.y + dir.y, origin.z + dir.z }, inverse);
    const float d[3] = { e.x - o.x, e.y - o.y, e.z - o.z };
    const float p[3] = { o.x, o.y, o.z };
    const float mn[3] = { lo.x, lo.y, lo.z };
    const float mx[3] = { hi.x, hi.y, hi.z };
    float t0 = 0.0f, t1 = 1e9f;
    for ( int axis = 0; axis < 3; ++axis ) {
        if ( std::abs(d[axis]) < 1e-8f ) {
            if ( p[axis] < mn[axis] || p[axis] > mx[axis] ) { return false; }
            continue;
        }
        float a = ( mn[axis] - p[axis] ) / d[axis];
        float b = ( mx[axis] - p[axis] ) / d[axis];
        if ( a > b ) { std::swap(a, b); }
        t0 = ( std::max )( t0, a );
        t1 = ( std::min )( t1, b );
        if ( t0 > t1 ) { return false; }
    }
    outT = t0;
    return true;
}

int StageBackdrop::Pick(const Vector3& origin, const Vector3& dir, bool includeGround, float* outDistance) const{
    int best = -1;
    float bestT = 1e9f;
    for ( int i = 0; i < ( int ) items_.size(); ++i ) {
        if ( items_[i].removed || IsFloor(i) ) { continue; }
        if ( !includeGround && IsGround(i) ) { continue; }
        // 空・丘・雲は大きすぎて手前の物を隠すので、地面と同じく「地面も選ぶ」の時だけ
        const std::string& model = items_[i].model;
        if ( !includeGround && ( model == "sky_backdrop" || model.rfind("hill_", 0) == 0 ) ) { continue; }
        float t;
        if ( RayHit(i, origin, dir, t) && t < bestT ) { bestT = t; best = i; }
    }
    if ( outDistance ) { *outDistance = bestT; }
    return best;
}

bool StageBackdrop::HitGround(const Vector3& origin, const Vector3& dir, Vector3& outPoint) const{
    float bestT = 1e9f;
    for ( int i = 0; i < ( int ) items_.size(); ++i ) {
        if ( items_[i].removed || !IsGround(i) ) { continue; }
        float t;
        if ( RayHit(i, origin, dir, t) && t < bestT ) { bestT = t; }
    }
    if ( bestT >= 1e9f ) { return false; }
    outPoint = { origin.x + dir.x * bestT, origin.y + dir.y * bestT, origin.z + dir.z * bestT };
    // 上面に当たった時は、草の厚み分のずれを無くす（地面の高さちょうどに置く）
    float y;
    if ( GroundHeightAt(outPoint.x, outPoint.z, y) && std::abs(y - outPoint.y) < 0.3f ) { outPoint.y = y; }
    return true;
}

bool StageBackdrop::GroundHeightAt(float x, float z, float& outY) const{
    bool found = false;
    float best = -1e9f;
    for ( int i = 0; i < ( int ) items_.size(); ++i ) {
        if ( items_[i].removed || !IsGround(i) ) { continue; }
        Vector3 lo, hi;
        if ( !WorldBounds(i, lo, hi) ) { continue; }
        if ( x < lo.x || x > hi.x || z < lo.z || z > hi.z ) { continue; }
        // 上面の高さ＝モデルの一番上（草の面）。少しの厚み pad を除く
        const float top = hi.y - 0.04f * items_[i].scale.y;
        if ( top > best ) { best = top; found = true; }
    }
    if ( found ) { outY = best; }
    return found;
}

bool StageBackdrop::WorldCorners(int index, Vector3 outCorners[8]) const{
    if ( index < 0 || index >= ( int ) items_.size() ) { return false; }
    Vector3 lo, hi;
    if ( !LocalBounds(items_[index].model, lo, hi) ) { return false; }
    for ( int k = 0; k < 8; ++k ) {
        const Vector3 local { ( k & 1 ) ? hi.x : lo.x, ( k & 2 ) ? hi.y : lo.y, ( k & 4 ) ? hi.z : lo.z };
        outCorners[k] = MatrixMath::Transforms(local, cache_[index].world);
    }
    return true;
}

bool StageBackdrop::WorldBounds(int index, Vector3& outMin, Vector3& outMax) const{
    if ( index < 0 || index >= ( int ) cache_.size() || !cache_[index].hasBounds ) { return false; }
    outMin = cache_[index].boundsMin;
    outMax = cache_[index].boundsMax;
    return true;
}
