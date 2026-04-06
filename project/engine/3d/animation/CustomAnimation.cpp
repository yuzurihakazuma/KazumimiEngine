#include "CustomAnimation.h"
#include <algorithm>
#include "engine/math/VectorMath.h"
#include <fstream>
#include <sstream>
#include "externals/nlohmann/json.hpp" // JSONライブラリのインクルード

// --------------------------------------------------
// キーフレームを追加（または更新）して、時間で並び替える
// --------------------------------------------------
void CustomAnimationTrack::AddKeyframe(float time, const Vector3& pos, const Vector3& rot, const Vector3& scale) {
    TransformKeyframe key{ time, pos, rot, scale };
    keyframes.push_back(key);

    // 時間順にソートする（小さい方が前）
    std::sort(keyframes.begin(), keyframes.end(),
        [](const TransformKeyframe& a, const TransformKeyframe& b) {
            return a.time < b.time;
        });

    // duration（全長）を最新にする
    if (!keyframes.empty()) {
        duration = keyframes.back().time;
    }
}

// --------------------------------------------------
// 指定された時間(currentTime)から、補間された座標・回転・スケールを計算する
// --------------------------------------------------
void CustomAnimationTrack::UpdateTransformAtTime(float currentTime, Vector3& outPos, Vector3& outRot, Vector3& outScale) const {
    if (keyframes.empty()) return;

    // キーフレームが1つしかない場合、または現在時間が最初のキーフレームより前の場合
    if (keyframes.size() == 1 || currentTime <= keyframes.front().time) {
        outPos = keyframes.front().position;
        outRot = keyframes.front().rotation;
        outScale = keyframes.front().scale;
        return;
    }

    // 現在時間が最後のキーフレーム以降の場合
    if (currentTime >= keyframes.back().time) {
        outPos = keyframes.back().position;
        outRot = keyframes.back().rotation;
        outScale = keyframes.back().scale;
        return;
    }

    // どのキーフレームの間にあるかを探す
    for (size_t i = 0; i < keyframes.size() - 1; ++i) {
        const auto& keyA = keyframes[i];
        const auto& keyB = keyframes[i + 1];

        // 間の時間だったら補間を行う
        if (currentTime >= keyA.time && currentTime <= keyB.time) {
            // (currentTime - a.time) / (b.time - a.time) をすると、0.0 ～ 1.0の値になる
            float t = (currentTime - keyA.time) / (keyB.time - keyA.time);

            // Lerp（線形補間: a + (b - a) * t）
            auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };

            outPos.x = lerp(keyA.position.x, keyB.position.x, t);
            outPos.y = lerp(keyA.position.y, keyB.position.y, t);
            outPos.z = lerp(keyA.position.z, keyB.position.z, t);

            outRot.x = lerp(keyA.rotation.x, keyB.rotation.x, t);
            outRot.y = lerp(keyA.rotation.y, keyB.rotation.y, t);
            outRot.z = lerp(keyA.rotation.z, keyB.rotation.z, t);

            outScale.x = lerp(keyA.scale.x, keyB.scale.x, t);
            outScale.y = lerp(keyA.scale.y, keyB.scale.y, t);
            outScale.z = lerp(keyA.scale.z, keyB.scale.z, t);
            return;
        }
    }
}

// --------------------------------------------------
// テキストファイルに書き出し
// --------------------------------------------------
void CustomAnimationTrack::SaveToFile(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) return;

    // 1行目に全体時間を書く
    file << duration << "\n";

    // 2行目以降にキーフレームデータを書く
    for (const auto& k : keyframes) {
        file << k.time << " "
             << k.position.x << " " << k.position.y << " " << k.position.z << " "
             << k.rotation.x << " " << k.rotation.y << " " << k.rotation.z << " "
             << k.scale.x << " " << k.scale.y << " " << k.scale.z << "\n";
    }
    file.close();
}

// --------------------------------------------------
// テキストファイルから読み込み
// --------------------------------------------------
void CustomAnimationTrack::LoadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return;

    keyframes.clear();
    
    // 1行目から全体時間を読み込む
    file >> duration;

    // 不要な改行を読み飛ばす
    std::string line;
    std::getline(file, line); 

    // 2行目以降からキーフレームを読み込む
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        TransformKeyframe k;
        ss >> k.time 
           >> k.position.x >> k.position.y >> k.position.z
           >> k.rotation.x >> k.rotation.y >> k.rotation.z
           >> k.scale.x >> k.scale.y >> k.scale.z;

        keyframes.push_back(k);
    }
    file.close();
}

// --------------------------------------------------
// JSON ファイルに書き出し
// --------------------------------------------------
void CustomAnimationTrack::SaveToJson(const std::string& filepath) const {
    nlohmann::json j;
    j["objectName"] = objectName;
    j["duration"] = duration;

    nlohmann::json keyframesJson = nlohmann::json::array();
    for(const auto& k : keyframes) {
        nlohmann::json kfJson;
        kfJson["time"] = k.time;
        kfJson["position"] = {k.position.x, k.position.y, k.position.z};
        kfJson["rotation"] = {k.rotation.x, k.rotation.y, k.rotation.z};
        kfJson["scale"] = {k.scale.x, k.scale.y, k.scale.z};
        keyframesJson.push_back(kfJson);
    }
    j["keyframes"] = keyframesJson;

    std::ofstream file(filepath);
    if(file.is_open()) {
        file << j.dump(4);
        file.close();
    }
}

// --------------------------------------------------
// JSON ファイルから読み込み
// --------------------------------------------------
void CustomAnimationTrack::LoadFromJson(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return;

    nlohmann::json j;
    file >> j;
    file.close();

    keyframes.clear();

    if (j.contains("objectName")) objectName = j["objectName"];
    if (j.contains("duration")) duration = j["duration"];

    if (j.contains("keyframes") && j["keyframes"].is_array()) {
        for (const auto& kfJson : j["keyframes"]) {
            TransformKeyframe k;
            k.time = kfJson["time"];
            k.position.x = kfJson["position"][0];
            k.position.y = kfJson["position"][1];
            k.position.z = kfJson["position"][2];
            k.rotation.x = kfJson["rotation"][0];
            k.rotation.y = kfJson["rotation"][1];
            k.rotation.z = kfJson["rotation"][2];
            k.scale.x = kfJson["scale"][0];
            k.scale.y = kfJson["scale"][1];
            k.scale.z = kfJson["scale"][2];
            keyframes.push_back(k);
        }
    }
}

