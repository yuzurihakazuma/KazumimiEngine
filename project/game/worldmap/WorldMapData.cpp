#include "game/worldmap/WorldMapData.h"
#include "engine/utils/JsonFile.h"

#include "externals/nlohmann/json.hpp"

using namespace KazumimiEngine;


using json = nlohmann::json;

namespace {
    // 無い・読めない・オブジェクトでない時は、空のオブジェクト
    json ReadFile(const std::string& path){
        json content;
        JsonFile::Load(path, content);
        return content.is_object() ? content : json::object();
    }
}

bool WorldMapData::Load(const std::string& path){
    const json content = ReadFile(path);
    if ( !content.contains("stages") ) { return false; }
    road.clear();
    for ( const json& point : content.value("road", json::array()) ) { road.push_back(JsonFile::ToVector3(point, {})); }
    const json skin = content.value("roadSkin", json::object());
    roadWidth = skin.value("width", roadWidth);
    roadColor = JsonFile::ToVector3(skin.value("color", json()), roadColor);
    const json pedestal = content.value("pedestal", json::object());
    pedestalBack = pedestal.value("back", pedestalBack);
    pedestalSize = JsonFile::ToVector3(pedestal.value("size", json()), pedestalSize);
    const json camera = content.value("camera", json::object());
    cameraOffset = JsonFile::ToVector3(camera.value("offset", json()), cameraOffset);
    cameraLook = JsonFile::ToVector3(camera.value("lookAhead", json()), cameraLook);
    fovY = camera.value("fovY", fovY);
    stages.clear();
    for ( const json& entry : content["stages"] ) {
        WorldStage stage;
        stage.id = entry.value("id", std::string());
        stage.stage = entry.value("stage", std::string());
        stage.label = entry.value("label", std::string());
        stage.unlocked = entry.value("unlocked", false) && !stage.stage.empty();
        stage.node = JsonFile::ToVector3(entry.value("node", json()), {});
        stage.dioramaMargin = entry.value("dioramaMargin", stage.dioramaMargin);
        stage.feature = entry.value("feature", std::string());
        const json window = entry.value("window", json::object());
        stage.windowDepth = window.value("depth", stage.windowDepth);
        if ( window.contains("center") && window["center"].is_array() && window["center"].size() >= 2 ) {
            stage.hasWindowCenter = true;
            stage.windowCenter = { window["center"][0].get<float>(), 0.0f, window["center"][1].get<float>() };
        }
        const json preview = entry.value("preview", json::object());
        stage.previewDistance = preview.value("distance", stage.previewDistance);
        stage.previewHeight = preview.value("height", stage.previewHeight);
        stage.orbitSeconds = preview.value("orbitSeconds", stage.orbitSeconds);
        stages.push_back(stage);
    }
    return !stages.empty() && road.size() >= 2;
}

bool DiveTiming::Load(const std::string& path){
    const json content = ReadFile(path);
    if ( content.empty() ) { return false; }
    crouch = content.value("crouch", crouch);
    jump = content.value("jump", jump);
    apex = content.value("apex", apex);
    dive = content.value("dive", dive);
    absorb = content.value("absorb", absorb);
    apexHeight = content.value("apexHeight", apexHeight);
    endScale = content.value("endScale", endScale);
    irisTime = content.value("irisTime", irisTime);
    irisHold = content.value("irisHold", irisHold);
    const json camera = content.value("camera", json::object());
    cameraApex = JsonFile::ToVector3(camera.value("apex", json()), cameraApex);
    cameraDive = JsonFile::ToVector3(camera.value("dive", json()), cameraDive);
    return true;
}
