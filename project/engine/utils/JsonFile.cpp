#include "JsonFile.h"

#include "engine/utils/LogManager.h"

#include <fstream>

namespace {
    // 並びの先頭 count 個が、全部数か
    bool IsNumberList(const nlohmann::json& list, size_t count){
        if ( !list.is_array() || list.size() < count ) { return false; }
        for ( size_t i = 0; i < count; ++i ) {
            if ( !list[i].is_number() ) { return false; }
        }
        return true;
    }
    // object[key] が、数 count 個の並びなら、それを返す（違えば nullptr）
    const nlohmann::json* NumberList(const nlohmann::json& object, const char* key, size_t count){
        if ( !object.is_object() || !object.contains(key) ) { return nullptr; }
        const nlohmann::json& list = object.at(key);
        return IsNumberList(list, count) ? &list : nullptr;
    }
}

namespace JsonFile {

    bool Load(const std::string& path, nlohmann::json& out){
        out = nlohmann::json::object();
        std::ifstream file(path);
        if ( !file.is_open() ) { return false; } // 無い：既定値で動く
        nlohmann::json parsed = nlohmann::json::parse(file, nullptr, false);
        if ( parsed.is_discarded() ) {
            logs::LogManager::GetInstance()->Log("[JsonFile] 読めません（書きかけ・壊れている）: " + path + "\n");
            return false;
        }
        out = std::move(parsed);
        return true;
    }

    bool Save(const std::string& path, const nlohmann::json& value, int indent){
        std::ofstream file(path);
        if ( !file.is_open() ) { return false; }
        file << value.dump(indent);
        return file.good();
    }

    nlohmann::json Child(const nlohmann::json& object, const char* key){
        if ( object.is_object() && object.contains(key) && object.at(key).is_object() ) { return object.at(key); }
        return nlohmann::json::object();
    }

    void Read(const nlohmann::json& object, const char* key, Vector2& out){
        if ( const nlohmann::json* list = NumberList(object, key, 2) ) {
            out = { ( *list )[0].get<float>(), ( *list )[1].get<float>() };
        }
    }
    void Read(const nlohmann::json& object, const char* key, Vector3& out){
        if ( const nlohmann::json* list = NumberList(object, key, 3) ) {
            out = { ( *list )[0].get<float>(), ( *list )[1].get<float>(), ( *list )[2].get<float>() };
        }
    }
    void Read(const nlohmann::json& object, const char* key, Vector4& out){
        if ( const nlohmann::json* list = NumberList(object, key, 4) ) {
            out = { ( *list )[0].get<float>(), ( *list )[1].get<float>(), ( *list )[2].get<float>(), ( *list )[3].get<float>() };
        }
    }

    Vector3 ToVector3(const nlohmann::json& value, const Vector3& fallback){
        if ( !IsNumberList(value, 3) ) { return fallback; }
        return { value[0].get<float>(), value[1].get<float>(), value[2].get<float>() };
    }

    nlohmann::json ToJson(const Vector2& value){ return nlohmann::json::array({ value.x, value.y }); }
    nlohmann::json ToJson(const Vector3& value){ return nlohmann::json::array({ value.x, value.y, value.z }); }
    nlohmann::json ToJson(const Vector4& value){ return nlohmann::json::array({ value.x, value.y, value.z, value.w }); }

} // namespace JsonFile
