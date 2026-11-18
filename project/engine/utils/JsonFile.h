#pragma once
// =====================================================================
//  JsonFile：調整データ（JSON）の読み書きの共通の入口。
//   ・Load / Save … ファイルを開く・読む・失敗を握る、をどこでも同じ書き方で
//       （前は「ifstream で開く → 開けたか調べる → try で読む → catch で空にする」を、ファイルごとに書いていた）
//   ・Read（関数テンプレート）… 「キーがあればその値、無ければ今の値のまま」で 1 項目読む。
//       数・真偽・文字・それらの並びは、型をテンプレートが決める。Vector2 / Vector3 / Vector4 は [x, y, z] の並びで読む
//   ・ToJson … Vector を [x, y, z] の並びにする
//
//     nlohmann::json root;
//     if ( JsonFile::Load("resources/clear/clear.json", root) ) {
//         JsonFile::Read(root, "duration", duration_);     // float
//         JsonFile::Read(root, "cameraOffset", offset_);   // Vector3
//     }
//
//  ファイルが無いのは、ふつうの事（まだ保存していない＝コードの既定値で動く）なので、何も言わずに false。
//  ファイルはあるのに読めない（書きかけ・壊れている）時は、ログに 1 行残して false
// =====================================================================
#include "engine/math/struct.h"
#include "externals/nlohmann/json.hpp"

#include <string>

namespace KazumimiEngine {

namespace JsonFile {

    // ファイルを読む。無い・壊れている時は false（out は空のオブジェクトになる）
    bool Load(const std::string& path, nlohmann::json& out);

    // ファイルへ書く（indent＝字下げの数）。書けなければ false
    bool Save(const std::string& path, const nlohmann::json& value, int indent = 2);

    // object の中の、名前 key の物（無ければ空のオブジェクト）。入れ子を読む時に使う
    nlohmann::json Child(const nlohmann::json& object, const char* key);

    // 1 項目読む：key があり、型が合えば out へ。無い・型が違う時は out はそのまま（＝コードの既定値が残る）
    template <class T>
    void Read(const nlohmann::json& object, const char* key, T& out){
        if ( !object.is_object() || !object.contains(key) ) { return; }
        try {
            out = object.at(key).template get<T>();
        } catch ( const nlohmann::json::exception& ) {
            // 型が違う：既定値のまま
        }
    }

    // Vector は [x, y] / [x, y, z] / [x, y, z, w] の並び（テンプレートより、こちらの専用の物が選ばれる）
    void Read(const nlohmann::json& object, const char* key, Vector2& out);
    void Read(const nlohmann::json& object, const char* key, Vector3& out);
    void Read(const nlohmann::json& object, const char* key, Vector4& out);

    // 値そのもの（[x, y, z] の並び）を Vector3 にする。数 3 つの並びでなければ fallback
    Vector3 ToVector3(const nlohmann::json& value, const Vector3& fallback);

    nlohmann::json ToJson(const Vector2& value);
    nlohmann::json ToJson(const Vector3& value);
    nlohmann::json ToJson(const Vector4& value);

} // namespace JsonFile

} // namespace KazumimiEngine
