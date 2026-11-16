#include "game/clear/ClearSettings.h"

#include "engine/utils/JsonFile.h"

namespace {
    using json = nlohmann::json;
    constexpr const char* kFolder = "resources/clear/";
    constexpr const char* kSharedFile = "resources/clear/clear.json";

    std::string StageFile(const std::string& stage){ return std::string(kFolder) + stage + ".json"; }

    void ReadTiming(const json& root, const char* key, ClearTiming& out){
        const json node = JsonFile::Child(root, key);
        JsonFile::Read(node, "ring", out.ring);
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

    const json jumpNode = JsonFile::Child(root, "jump");
    JsonFile::Read(jumpNode, "takeOff", jump.takeOff);
    JsonFile::Read(jumpNode, "land", jump.land);
    JsonFile::Read(jumpNode, "pose", jump.pose);

    const json ringNode = JsonFile::Child(root, "ring");
    JsonFile::Read(ringNode, "hinge", ring.hinge);
    JsonFile::Read(ringNode, "halfWidth", ring.halfWidth);
    JsonFile::Read(ringNode, "height", ring.height);
    JsonFile::Read(ringNode, "innerHalfWidth", ring.innerHalfWidth);
    JsonFile::Read(ringNode, "innerHeight", ring.innerHeight);
    const json openNode = JsonFile::Child(root, "ringOpen");
    JsonFile::Read(openNode, "time", ring.openTime);
    JsonFile::Read(openNode, "angle", ring.openAngle);
    JsonFile::Read(openNode, "hop", ring.openHop);

    const json lettersNode = JsonFile::Child(root, "letters");
    JsonFile::Read(lettersNode, "count", letters.count);
    JsonFile::Read(lettersNode, "textureSize", letters.textureSize);
    JsonFile::Read(lettersNode, "height", letters.height);
    JsonFile::Read(lettersNode, "string", letters.string);
    JsonFile::Read(lettersNode, "spacing", letters.spacing);
    JsonFile::Read(lettersNode, "centerHeight", letters.centerHeight);
    JsonFile::Read(lettersNode, "back", letters.back);
    JsonFile::Read(lettersNode, "start", letters.start);
    JsonFile::Read(lettersNode, "interval", letters.interval);
    JsonFile::Read(lettersNode, "dropTime", letters.dropTime);
    JsonFile::Read(lettersNode, "dropHeight", letters.dropHeight);
    JsonFile::Read(lettersNode, "swing", letters.swing);
    JsonFile::Read(lettersNode, "sway", letters.sway);

    const json starsNode = JsonFile::Child(root, "stars");
    JsonFile::Read(starsNode, "textureSize", stars.textureSize);
    JsonFile::Read(starsNode, "height", stars.height);
    JsonFile::Read(starsNode, "stick", stars.stick);
    JsonFile::Read(starsNode, "perSide", stars.perSide);
    JsonFile::Read(starsNode, "pivotSide", stars.pivotSide);
    JsonFile::Read(starsNode, "pivotHeight", stars.pivotHeight);
    JsonFile::Read(starsNode, "back", stars.back);
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

    ReadResult(root, result);

    const json soundsNode = JsonFile::Child(root, "sounds");
    JsonFile::Read(soundsNode, "ring", sounds.ring);
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
    root["letters"]["centerHeight"] = letters.centerHeight;
    root["letters"]["spacing"] = letters.spacing;
    root["letters"]["back"] = letters.back;
    root["stars"]["pivotSide"] = stars.pivotSide;
    root["stars"]["pivotHeight"] = stars.pivotHeight;
    root["stars"]["back"] = stars.back;
    return JsonFile::Save(kSharedFile, root);
}

bool ClearStageSettings::Load(const std::string& stage){
    json root;
    if ( !JsonFile::Load(StageFile(stage), root) ) { return false; }
    JsonFile::Read(root, "ringOffset", ringOffset);
    JsonFile::Read(root, "landAhead", landAhead);
    const json cameraNode = JsonFile::Child(root, "camera");
    JsonFile::Read(cameraNode, "distance", cameraDistance);
    JsonFile::Read(cameraNode, "height", cameraHeight);
    JsonFile::Read(cameraNode, "side", cameraSide);
    JsonFile::Read(cameraNode, "lookHeight", lookHeight);
    JsonFile::Read(cameraNode, "fovY", fovY);
    JsonFile::Read(root, "comboTarget", comboTarget);
    JsonFile::Read(root, "band", band);
    JsonFile::Read(root, "healthRule", healthRule);
    return true;
}

bool ClearStageSettings::Save(const std::string& stage) const{
    json root;
    root["ringOffset"] = ringOffset;
    root["landAhead"] = landAhead;
    root["camera"] = { { "distance", cameraDistance }, { "height", cameraHeight }, { "side", cameraSide },
                       { "lookHeight", lookHeight }, { "fovY", fovY } };
    root["comboTarget"] = comboTarget;
    root["band"] = band;
    root["healthRule"] = healthRule;
    return JsonFile::Save(StageFile(stage), root);
}
