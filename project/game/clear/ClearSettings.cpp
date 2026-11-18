#include "game/clear/ClearSettings.h"

#include "engine/utils/JsonFile.h"

namespace {
    using json = nlohmann::json;
    constexpr const char* kFolder = "resources/clear/";
    constexpr const char* kSharedFile = "resources/clear/clear.json";

    std::string StageFile(const std::string& stage){ return std::string(kFolder) + stage + ".json"; }

    void ReadTiming(const json& root, const char* key, ClearTiming& out){
        const json node = JsonFile::Child(root, key);
        JsonFile::Read(node, "gate", out.gate);
        JsonFile::Read(node, "goal", out.goal);
        JsonFile::Read(node, "toResult", out.toResult);
        JsonFile::Read(node, "leave", out.leave);
    }

    // { "x": .., "y": .. } を Vector2 へ
    void ReadPoint(const json& parent, const char* key, Vector2& out){
        const json node = JsonFile::Child(parent, key);
        JsonFile::Read(node, "x", out.x);
        JsonFile::Read(node, "y", out.y);
    }

    void ReadResult(const json& root, ClearResultLayout& out){
        const json node = JsonFile::Child(root, "result");
        JsonFile::Read(node, "dim", out.dim);
        ReadPoint(node, "mat", out.mat);
        ReadPoint(node, "title", out.title);
        ReadPoint(node, "name", out.name);
        JsonFile::Read(JsonFile::Child(node, "name"), "width", out.nameWidth);
        ReadPoint(node, "hanamaru", out.hanamaru);
        JsonFile::Read(JsonFile::Child(node, "hanamaru"), "size", out.hanamaruSize);
        JsonFile::Read(node, "rowX", out.rowX);
        JsonFile::Read(node, "rowYs", out.rowYs);
        JsonFile::Read(node, "iconX", out.iconX);
        JsonFile::Read(node, "iconSize", out.iconSize);
        JsonFile::Read(node, "labelX", out.labelX);
        JsonFile::Read(node, "valueRight", out.valueRight);
        JsonFile::Read(node, "valueHeight", out.valueHeight);
        JsonFile::Read(node, "stampX", out.stampX);
        JsonFile::Read(node, "stampSize", out.stampSize);
        JsonFile::Read(node, "flowerX", out.flowerX);
        JsonFile::Read(node, "flowerGap", out.flowerGap);
        JsonFile::Read(node, "flowerSize", out.flowerSize);
        ReadPoint(node, "prompt", out.prompt);
        ReadPoint(node, "owned", out.owned);
        const json owned = JsonFile::Child(node, "owned");
        JsonFile::Read(owned, "numberX", out.ownedNumberX);
        JsonFile::Read(owned, "numberHeight", out.ownedNumberHeight);
        JsonFile::Read(node, "matIn", out.matIn);
        JsonFile::Read(node, "itemIn", out.itemIn);
        JsonFile::Read(node, "flowerInterval", out.flowerInterval);
        JsonFile::Read(node, "countTime", out.countTime);
        JsonFile::Read(node, "stampTime", out.stampTime);
        JsonFile::Read(node, "rowGap", out.rowGap);
        JsonFile::Read(node, "hanamaruTime", out.hanamaruTime);
        JsonFile::Read(node, "addTime", out.addTime);
    }
}

bool ClearSettings::Load(){
    json root;
    if ( !JsonFile::Load(kSharedFile, root) ) { return false; }

    ReadTiming(root, "full", full);
    ReadTiming(root, "short", shortVersion);

    const json sessionNode = JsonFile::Child(root, "playSession");
    JsonFile::Read(sessionNode, "allStars", playSession.allStars);
    JsonFile::Read(sessionNode, "retryNearGoal", playSession.retryNearGoal);
    JsonFile::Read(sessionNode, "retryBack", playSession.retryBack);

    const json jumpNode = JsonFile::Child(root, "jump");
    JsonFile::Read(jumpNode, "takeOff", jump.takeOff);
    JsonFile::Read(jumpNode, "land", jump.land);
    JsonFile::Read(jumpNode, "pose", jump.pose);

    const json gateNode = JsonFile::Child(root, "gate");
    JsonFile::Read(gateNode, "innerHalfWidth", gate.innerHalfWidth);
    JsonFile::Read(gateNode, "innerHeight", gate.innerHeight);
    JsonFile::Read(gateNode, "archCenterHeight", gate.archCenterHeight);
    JsonFile::Read(gateNode, "archOuterRadius", gate.archOuterRadius);
    JsonFile::Read(gateNode, "archFront", gate.archFront);
    JsonFile::Read(gateNode, "eggHinge", gate.eggHinge);
    JsonFile::Read(gateNode, "tapePivot", gate.tapePivot);
    JsonFile::Read(gateNode, "height", gate.height);
    const json openNode = JsonFile::Child(root, "gateOpen");
    JsonFile::Read(openNode, "time", gate.openTime);
    JsonFile::Read(openNode, "eggAngle", gate.eggAngle);
    JsonFile::Read(openNode, "hop", gate.hop);
    JsonFile::Read(openNode, "tapeAngle", gate.tapeAngle);

    const json lettersNode = JsonFile::Child(root, "letters");
    JsonFile::Read(lettersNode, "count", letters.count);
    JsonFile::Read(lettersNode, "height", letters.height);
    JsonFile::Read(lettersNode, "scale", letters.scale);
    JsonFile::Read(lettersNode, "arcRadius", letters.arcRadius);
    JsonFile::Read(lettersNode, "arcAngles", letters.arcAngles);
    JsonFile::Read(lettersNode, "stringLength", letters.stringLength);
    JsonFile::Read(lettersNode, "dropHeight", letters.dropHeight);
    JsonFile::Read(lettersNode, "front", letters.front);
    JsonFile::Read(lettersNode, "start", letters.start);
    JsonFile::Read(lettersNode, "interval", letters.interval);
    JsonFile::Read(lettersNode, "dropTime", letters.dropTime);
    JsonFile::Read(lettersNode, "swing", letters.swing);
    JsonFile::Read(lettersNode, "sway", letters.sway);

    const json starsNode = JsonFile::Child(root, "stars");
    JsonFile::Read(starsNode, "perSide", stars.perSide);
    JsonFile::Read(starsNode, "pivotSide", stars.pivotSide);
    JsonFile::Read(starsNode, "pivotHeight", stars.pivotHeight);
    JsonFile::Read(starsNode, "front", stars.front);
    JsonFile::Read(starsNode, "angles", stars.angles);
    JsonFile::Read(starsNode, "start", stars.start);
    JsonFile::Read(starsNode, "interval", stars.interval);
    JsonFile::Read(starsNode, "popTime", stars.popTime);

    const json confettiNode = JsonFile::Child(root, "confetti");
    JsonFile::Read(confettiNode, "burst", confetti.burst);
    JsonFile::Read(confettiNode, "rain", confetti.rain);
    JsonFile::Read(confettiNode, "rainInterval", confetti.rainInterval);
    JsonFile::Read(confettiNode, "height", confetti.height);
    JsonFile::Read(confettiNode, "spread", confetti.spread);

    // クリアの書き割り（"backdrop"：{ model, back, side, up, scale[3], turn } の並び）
    if ( root.contains("backdrop") && root["backdrop"].is_array() ) {
        backdrop.clear();
        for ( const json& entry : root["backdrop"] ) {
            ClearBackdropItem item;
            JsonFile::Read(entry, "model", item.model);
            JsonFile::Read(entry, "back", item.back);
            JsonFile::Read(entry, "side", item.side);
            JsonFile::Read(entry, "up", item.up);
            JsonFile::Read(entry, "scale", item.scale);
            JsonFile::Read(entry, "turn", item.turn);
            if ( !item.model.empty() ) { backdrop.push_back(item); }
        }
    }

    ReadResult(root, result);

    const json soundsNode = JsonFile::Child(root, "sounds");
    JsonFile::Read(soundsNode, "gate", sounds.gate);
    JsonFile::Read(soundsNode, "letter", sounds.letter);
    JsonFile::Read(soundsNode, "star", sounds.star);
    JsonFile::Read(soundsNode, "count", sounds.count);
    JsonFile::Read(soundsNode, "stamp", sounds.stamp);
    JsonFile::Read(soundsNode, "hanamaru", sounds.hanamaru);
    JsonFile::Read(soundsNode, "add", sounds.add);
    JsonFile::Read(soundsNode, "road", sounds.road);
    return true;
}

bool ClearSettings::SaveProps() const{
    json root;
    JsonFile::Load(kSharedFile, root); // ほかの項目を残すため、今のファイルに書き足す
    root["letters"]["scale"] = letters.scale;
    root["letters"]["arcRadius"] = letters.arcRadius;
    root["letters"]["arcAngles"] = letters.arcAngles;
    root["letters"]["front"] = letters.front;
    root["stars"]["pivotSide"] = stars.pivotSide;
    root["stars"]["pivotHeight"] = stars.pivotHeight;
    root["stars"]["front"] = stars.front;
    return JsonFile::Save(kSharedFile, root);
}

bool ClearStageSettings::Load(const std::string& stage){
    json root;
    if ( !JsonFile::Load(StageFile(stage), root) ) { return false; }
    JsonFile::Read(root, "gateOffset", gateOffset);
    JsonFile::Read(root, "gateTurn", gateTurn);
    JsonFile::Read(root, "landAhead", landAhead);
    const json cameraNode = JsonFile::Child(root, "camera");
    JsonFile::Read(cameraNode, "yaw", cameraYaw);
    JsonFile::Read(cameraNode, "distance", cameraDistance);
    JsonFile::Read(cameraNode, "height", cameraHeight);
    JsonFile::Read(cameraNode, "lookHeight", lookHeight);
    JsonFile::Read(cameraNode, "lookBack", lookBack);
    JsonFile::Read(cameraNode, "fovY", fovY);
    JsonFile::Read(root, "comboTarget", comboTarget);
    JsonFile::Read(root, "band", band);
    JsonFile::Read(root, "healthRule", healthRule);
    return true;
}

bool ClearStageSettings::Save(const std::string& stage) const{
    json root;
    root["gateOffset"] = gateOffset;
    root["gateTurn"] = gateTurn;
    root["landAhead"] = landAhead;
    root["camera"] = { { "yaw", cameraYaw }, { "distance", cameraDistance }, { "height", cameraHeight },
                       { "lookHeight", lookHeight }, { "lookBack", lookBack }, { "fovY", fovY } };
    root["comboTarget"] = comboTarget;
    root["band"] = band;
    root["healthRule"] = healthRule;
    return JsonFile::Save(StageFile(stage), root);
}
