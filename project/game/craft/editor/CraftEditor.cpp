#include "game/craft/editor/CraftEditor.h"
#include "game/craft/CraftAssetCatalog.h"
#include "game/craft/CraftStageView.h"

#include "engine/3d/model/ModelManager.h"
#include "engine/3d/obj/Obj3d.h"
#include "engine/camera/Camera.h"
#include "engine/graphics/DebugDraw.h"
#include "engine/math/Matrix4x4.h"
#include "engine/math/VectorMath.h"
#include "engine/rail/SplineRail.h"
#include "engine/utils/EditorManager.h"
#include "engine/utils/Level/LevelEditor.h"
#include "engine/utils/Level/RailEditor.h"
#include "engine/utils/Random.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/ImGuizmo/ImGuizmo.h"
#endif

#include <algorithm>
#include <cmath>
#include <filesystem>

using namespace MatrixMath;

namespace {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kDegToRad = kPi / 180.0f;
    constexpr float kRadToDeg = 180.0f / kPi;
    constexpr float kAutosaveInterval = 180.0f; // 未保存の変更があれば3分ごとに自動保存
    constexpr float kRotationSnap = 15.0f;      // 回転の刻み(度)
    constexpr const char* kWindowTitle = "箱庭エディタ (Craft)"; // EditorManager のパネル定義と同じ名前

    Vector3 ToRadians(const Vector3& degrees){ return { degrees.x * kDegToRad, degrees.y * kDegToRad, degrees.z * kDegToRad }; }

    // 物の大きさ（manifest の範囲。一覧に無い物は赤い箱＝1m角）
    void ObjectBounds(const CraftObject& object, Vector3& outMin, Vector3& outMax){
        if ( const CraftAssetCatalog::Asset* asset = CraftAssetCatalog::GetInstance()->Find(object.asset) ) {
            outMin = asset->boundsMin;
            outMax = asset->boundsMax;
        } else {
            outMin = { -0.5f, -0.5f, -0.5f };
            outMax = { 0.5f, 0.5f, 0.5f };
        }
    }

    Matrix4x4 ObjectMatrix(const CraftObject& object){
        return MakeAffine(object.scale, ToRadians(object.rotationDeg), object.position);
    }

    // レイと物の箱（物の向きに沿った箱）の当たり。当たれば true と距離
    bool RayHitsObject(const CraftObject& object, const Vector3& origin, const Vector3& direction, float& outDistance){
        Vector3 boxMin, boxMax;
        ObjectBounds(object, boxMin, boxMax);
        const Matrix4x4 inverse = Inverse(ObjectMatrix(object));
        const Vector3 localOrigin = Transforms(origin, inverse);
        const Vector3 localAhead  = Transforms(origin + direction, inverse);
        const Vector3 localDirection = localAhead - localOrigin;
        float tMin = 0.0f, tMax = 1e9f;
        const float origins[3] = { localOrigin.x, localOrigin.y, localOrigin.z };
        const float directions[3] = { localDirection.x, localDirection.y, localDirection.z };
        const float mins[3] = { boxMin.x, boxMin.y, boxMin.z };
        const float maxs[3] = { boxMax.x, boxMax.y, boxMax.z };
        for ( int axis = 0; axis < 3; ++axis ) {
            if ( std::abs(directions[axis]) < 1e-8f ) {
                if ( origins[axis] < mins[axis] || origins[axis] > maxs[axis] ) { return false; }
                continue;
            }
            float t1 = ( mins[axis] - origins[axis] ) / directions[axis];
            float t2 = ( maxs[axis] - origins[axis] ) / directions[axis];
            if ( t1 > t2 ) { std::swap(t1, t2); }
            tMin = ( std::max )( tMin, t1 );
            tMax = ( std::min )( tMax, t2 );
            if ( tMin > tMax ) { return false; }
        }
        outDistance = tMin;
        return true;
    }

    float SnapValue(float value, float step){ return ( step > 0.0f ) ? std::round(value / step) * step : value; }

    void DrawCircle(const Vector3& center, float radius, const Vector4& color){
        constexpr int kSegments = 40;
        for ( int i = 0; i < kSegments; ++i ) {
            const float a0 = kPi * 2.0f * ( float ) i / kSegments;
            const float a1 = kPi * 2.0f * ( float ) ( i + 1 ) / kSegments;
            DebugDraw::GetInstance()->Line({ center.x + std::cos(a0) * radius, center.y, center.z + std::sin(a0) * radius },
                                           { center.x + std::cos(a1) * radius, center.y, center.z + std::sin(a1) * radius }, color);
        }
    }

    // 物の向きに沿った箱の線（選んでいる物の目印）
    void DrawObjectBox(const CraftObject& object, const Vector4& color){
        Vector3 boxMin, boxMax;
        ObjectBounds(object, boxMin, boxMax);
        const Matrix4x4 world = ObjectMatrix(object);
        Vector3 corners[8];
        for ( int i = 0; i < 8; ++i ) {
            const Vector3 local { ( i & 1 ) ? boxMax.x : boxMin.x, ( i & 2 ) ? boxMax.y : boxMin.y, ( i & 4 ) ? boxMax.z : boxMin.z };
            corners[i] = Transforms(local, world);
        }
        const int edges[12][2] = { {0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7} };
        for ( const auto& edge : edges ) { DebugDraw::GetInstance()->Line(corners[edge[0]], corners[edge[1]], color); }
    }

    bool TypingInImGui(){
#ifdef USE_IMGUI
        return ImGui::GetIO().WantTextInput;
#else
        return true;
#endif
    }
}

CraftEditor::CraftEditor() = default;
CraftEditor::~CraftEditor() = default;

void CraftEditor::Initialize(Camera* camera){
    camera_ = camera;
    ghost_.reset();
    selectedId_ = 0;
    tool_ = Tool::Select;
    // 散布ブラシの既定：草を多めに、花を少し
    brushAssets_.clear();
    const std::pair<const char*, float> defaults[] = {
        { "grass_tuft_00", 1.0f }, { "grass_tuft_01", 1.0f }, { "grass_tuft_02", 1.0f },
        { "flower_orange", 0.3f }, { "flower_red", 0.3f }, { "flower_white", 0.2f } };
    for ( const auto& entry : defaults ) { brushAssets_.push_back({ entry.first, entry.second }); }
}

void CraftEditor::Finalize(){
    if ( ownsGizmo_ ) { EditorManager::GetInstance()->SetGizmoTarget(nullptr); ownsGizmo_ = false; }
    ghost_.reset();
    brushStroke_.reset();
}

void CraftEditor::OnStageLoaded(const Context& context, bool loadFailed){
    history_.Clear();
    Select(0);
    loadFailed_ = loadFailed;
    // 自動保存の方が本体より新しければ「復元しますか？」と聞く
    namespace fs = std::filesystem;
    std::error_code error;
    const std::string autosave = CraftStage::AutosavePath(context.path);
    restorePrompt_ = false;
    if ( fs::exists(autosave, error) ) {
        restorePrompt_ = !fs::exists(context.path, error)
            || fs::last_write_time(autosave, error) > fs::last_write_time(context.path, error);
    }
}

bool CraftEditor::Save(const Context& context){
    if ( !context.stage ) { return false; }
    const bool saved = context.stage->Save(context.path);
    if ( saved ) {
        history_.MarkSaved();
        std::error_code error;
        std::filesystem::remove(CraftStage::AutosavePath(context.path), error); // 本体が最新になったので自動保存は不要
    }
    message_ = saved ? "保存しました: " + context.path : "保存に失敗しました";
    messageTime_ = 3.0f;
    return saved;
}

// =====================================================================
//  毎フレーム
// =====================================================================
void CraftEditor::BeforeViewUpdate(const Context& context){
    // 選んでいた物が消えた（元に戻した等）：描画側が実体を捨てる前に、ギズモから外す
    if ( ownsGizmo_ && ( selectedId_ == 0 || !context.stage->Find(selectedId_) ) ) {
        EditorManager::GetInstance()->SetGizmoTarget(nullptr);
        ownsGizmo_ = false;
        if ( selectedId_ && !context.stage->Find(selectedId_) ) { selectedId_ = 0; }
    }
    // 未保存の変更があれば、3分ごとに autosave/ へ書く（本体は上書きしない）
    autosaveTimer_ += 1.0f / 60.0f;
    if ( autosaveTimer_ >= kAutosaveInterval ) {
        autosaveTimer_ = 0.0f;
        if ( history_.IsDirty() ) { context.stage->SaveAutosave(context.path); }
    }
    if ( messageTime_ > 0.0f ) { messageTime_ -= 1.0f / 60.0f; }
}

void CraftEditor::AfterViewUpdate(const Context& context){
    EditorManager* editor = EditorManager::GetInstance();
    const bool wantGizmo = editor->IsActive() && editor->IsPanelVisible(EditorManager::Panel_Craft)
        && tool_ == Tool::Select && selectedId_ != 0;
    if ( wantGizmo ) {
        if ( Obj3d* object = context.view->FindObject(selectedId_) ) {
            editor->SetGizmoTarget(object);
            ownsGizmo_ = true;
            return;
        }
    }
    if ( ownsGizmo_ ) { editor->SetGizmoTarget(nullptr); ownsGizmo_ = false; }
}

void CraftEditor::DrawGhost(){
    if ( ghost_ && ghostVisible_ && tool_ == Tool::Place ) { ghost_->Draw(); }
}

// =====================================================================
//  パネル
// =====================================================================
void CraftEditor::DrawUI(const Context& context){
#ifdef USE_IMGUI
    EditorManager* editor = EditorManager::GetInstance();
    const EditorManager::GameViewMouse& gameView = editor->GetGameViewMouse();
    ghostVisible_ = false;

    bool windowFocused = false;
    if ( ImGui::Begin(kWindowTitle) ) {
        windowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        DrawRestorePrompt(context);
        DrawToolbar(context);
        if ( ImGui::BeginTabBar("##craftTabs") ) {
            if ( ImGui::BeginTabItem("一覧") )       { DrawOutliner(context); ImGui::EndTabItem(); }
            if ( ImGui::BeginTabItem("インスペクタ") ) { DrawInspector(context); ImGui::EndTabItem(); }
            if ( ImGui::BeginTabItem("アセット") )   { DrawAssets(context); ImGui::EndTabItem(); }
            if ( ImGui::BeginTabItem("ブラシ") )     { DrawBrushSettings(); ImGui::EndTabItem(); }
            if ( ImGui::BeginTabItem("レール沿い") ) { DrawRailSettings(context); ImGui::EndTabItem(); }
            if ( ImGui::BeginTabItem("履歴") )       { DrawHistory(); ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();

    // Game View 上の操作
    const bool mouseUsable = gameView.hovered && !gameView.gizmoActive;
    HandleHotkeys(context, gameView.hovered, windowFocused);
    UpdateGizmoDrag(context);
    switch ( tool_ ) {
    case Tool::Select: if ( mouseUsable ) { UpdateSelectClick(context); } break;
    case Tool::Place:  UpdatePlace(context); break;
    case Tool::Brush:  UpdateBrush(context); break;
    default: break;
    }
    DrawSelection(context);
#else
    ( void ) context;
#endif
}

void CraftEditor::DrawToolbar(const Context& context){
#ifdef USE_IMGUI
    // ステージ名（未保存なら「*」）
    ImGui::Text("%s%s", context.stage->GetName().c_str(), history_.IsDirty() ? " *" : "");
    ImGui::SameLine();
    if ( ImGui::Button("保存 (Ctrl+S)") ) { Save(context); }
    ImGui::SameLine();
    if ( ImGui::Button("読み直す") ) {
        const bool loaded = context.stage->Load(context.path);
        OnStageLoaded(context, !loaded);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!history_.CanUndo());
    if ( ImGui::Button("元に戻す") ) { Undo(context); }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!history_.CanRedo());
    if ( ImGui::Button("やり直し") ) { Redo(context); }
    ImGui::EndDisabled();

    // 道具
    int tool = ( int ) tool_;
    ImGui::RadioButton("選ぶ", &tool, ( int ) Tool::Select); ImGui::SameLine();
    ImGui::RadioButton("1個置く", &tool, ( int ) Tool::Place); ImGui::SameLine();
    ImGui::RadioButton("ブラシ", &tool, ( int ) Tool::Brush); ImGui::SameLine();
    ImGui::RadioButton("レール沿い", &tool, ( int ) Tool::Rail);
    tool_ = ( Tool ) tool;

    const char* snapNames[] = { "スナップ 大（地面タイル）", "スナップ 細かい", "スナップなし" };
    ImGui::SetNextItemWidth(220.0f);
    ImGui::Combo("##snap", &snapMode_, snapNames, 3);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::DragFloat("基準の大きさ", &baseScale_, 0.01f, 0.1f, 5.0f);

    if ( messageTime_ > 0.0f ) { ImGui::TextColored({ 0.5f, 0.9f, 1.0f, 1.0f }, "%s", message_.c_str()); }
    for ( const std::string& warning : context.stage->GetLoadWarnings() ) {
        ImGui::TextColored({ 1.0f, 0.75f, 0.3f, 1.0f }, "%s", warning.c_str());
    }
    if ( loadFailed_ ) {
        const std::string backup = CraftStage::LatestBackupPath(context.path);
        if ( !backup.empty() && ImGui::Button("バックアップの最新を開く") ) {
            if ( context.stage->Load(backup) ) {
                history_.Clear();
                history_.PushDone(std::make_unique<CraftGroupCommand>("バックアップから復元")); // 未保存扱いにする
                loadFailed_ = false;
            }
        }
    }
    ImGui::Separator();
#endif
}

void CraftEditor::DrawRestorePrompt(const Context& context){
#ifdef USE_IMGUI
    if ( !restorePrompt_ ) { return; }
    ImGui::TextColored({ 1.0f, 0.85f, 0.3f, 1.0f }, "自動保存の方が本体より新しいです。復元しますか？");
    if ( ImGui::Button("復元する") ) {
        if ( context.stage->Load(CraftStage::AutosavePath(context.path)) ) {
            history_.Clear();
            history_.PushDone(std::make_unique<CraftGroupCommand>("自動保存から復元")); // 本体へはまだ保存していない
        }
        restorePrompt_ = false;
    }
    ImGui::SameLine();
    if ( ImGui::Button("復元しない") ) { restorePrompt_ = false; }
    ImGui::Separator();
#else
    ( void ) context;
#endif
}

void CraftEditor::DrawOutliner(const Context& context){
#ifdef USE_IMGUI
    // 層ごとの表示・ロック
    for ( int layer = 0; layer < CraftLayer_Count; ++layer ) {
        ImGui::PushID(layer);
        bool visible = context.view->IsLayerVisible(layer);
        if ( ImGui::Checkbox("##visible", &visible) ) { context.view->SetLayerVisible(layer, visible); }
        if ( ImGui::IsItemHovered() ) { ImGui::SetTooltip("表示"); }
        ImGui::SameLine();
        ImGui::Checkbox("##lock", &layerLocked_[layer]);
        if ( ImGui::IsItemHovered() ) { ImGui::SetTooltip("ロック（クリックで選べなくする）"); }
        ImGui::SameLine();
        int count = 0;
        for ( const CraftObject& object : context.stage->GetObjects() ) { if ( object.layer == layer ) { ++count; } }
        ImGui::Text("%s（%d）", CraftLayerDisplayName(layer), count);
        ImGui::PopID();
    }
    ImGui::TextDisabled("左：表示 / 右：ロック");
    ImGui::InputTextWithHint("##filter", "名前で絞り込み", filter_, sizeof(filter_));

    // 一覧（多くても軽いよう、見えている行だけ描く）
    std::vector<const CraftObject*> rows;
    for ( const CraftObject& object : context.stage->GetObjects() ) {
        if ( filter_[0] && object.asset.find(filter_) == std::string::npos ) { continue; }
        rows.push_back(&object);
    }
    if ( ImGui::BeginChild("##outlinerList", ImVec2(0, 0), true) ) {
        ImGuiListClipper clipper;
        clipper.Begin(( int ) rows.size());
        while ( clipper.Step() ) {
            for ( int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i ) {
                const CraftObject* object = rows[i];
                ImGui::PushID(( int ) i);
                const std::string label = std::string("[") + CraftLayerDisplayName(object->layer) + "] " + object->asset;
                if ( ImGui::Selectable(label.c_str(), object->id == selectedId_) ) {
                    Select(object->id);
                    tool_ = Tool::Select;
                }
                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();
#else
    ( void ) context;
#endif
}

void CraftEditor::DrawInspector(const Context& context){
#ifdef USE_IMGUI
    const CraftObject* selected = selectedId_ ? context.stage->Find(selectedId_) : nullptr;
    if ( !selected ) {
        ImGui::TextDisabled("選んでいる物はありません（Game View でクリック、または一覧から選ぶ）");
        return;
    }
    ImGui::Text("%s", selected->asset.c_str());
    ImGui::TextDisabled("id %s", CraftStage::IdToString(selected->id).c_str());
    if ( !CraftAssetCatalog::GetInstance()->Find(selected->asset) ) {
        ImGui::TextColored({ 1.0f, 0.3f, 0.3f, 1.0f }, "manifest に無いアセットです（赤い箱で表示。データは残ります）");
    }

    // 層の切り替え（1回の操作として履歴へ）
    int layer = selected->layer;
    const char* layerNames[CraftLayer_Count];
    for ( int i = 0; i < CraftLayer_Count; ++i ) { layerNames[i] = CraftLayerDisplayName(i); }
    if ( ImGui::Combo("層", &layer, layerNames, CraftLayer_Count) ) {
        CraftObject after = *selected;
        after.layer = layer;
        history_.Execute(*context.stage, std::make_unique<CraftTransformCommand>(*selected, after));
        selected = context.stage->Find(selectedId_);
    }

    // 位置・回転・拡縮：触り始めに「変更前」を覚え、離した時に1回分として履歴へ積む
    CraftObject edit = *selected;
    bool changed = false;
    bool activated = false, deactivated = false;
    changed |= ImGui::DragFloat3("位置", &edit.position.x, 0.02f);
    activated |= ImGui::IsItemActivated(); deactivated |= ImGui::IsItemDeactivatedAfterEdit();
    changed |= ImGui::DragFloat3("回転（度）", &edit.rotationDeg.x, 0.5f);
    activated |= ImGui::IsItemActivated(); deactivated |= ImGui::IsItemDeactivatedAfterEdit();
    changed |= ImGui::DragFloat3("拡縮", &edit.scale.x, 0.01f, 0.01f, 100.0f);
    activated |= ImGui::IsItemActivated(); deactivated |= ImGui::IsItemDeactivatedAfterEdit();
    if ( activated ) { fieldBefore_ = *selected; fieldEditing_ = true; }
    if ( changed ) {
        if ( CraftObject* target = context.stage->FindMutable(selectedId_) ) {
            target->position = edit.position;
            target->rotationDeg = edit.rotationDeg;
            target->scale = edit.scale;
            context.stage->Touch();
        }
    }
    if ( deactivated && fieldEditing_ ) {
        fieldEditing_ = false;
        if ( const CraftObject* after = context.stage->Find(selectedId_) ) {
            history_.PushDone(std::make_unique<CraftTransformCommand>(fieldBefore_, *after));
        }
    }
    // 背景の追従（カメラが動いた分のこの割合だけ一緒に動く。0=動かない / 1=空のように付いてくる）
    {
        float follow = selected->params.value("parallax", 0.0f);
        ImGui::SliderFloat("背景の追従", &follow, 0.0f, 1.0f);
        const bool startEdit = ImGui::IsItemActivated();
        if ( startEdit ) { paramsBefore_ = selected->params; }
        if ( ImGui::IsItemEdited() ) {
            if ( CraftObject* target = context.stage->FindMutable(selectedId_) ) {
                if ( follow > 0.0f ) { target->params["parallax"] = follow; } else { target->params.erase("parallax"); }
                context.stage->Touch();
            }
        }
        if ( ImGui::IsItemDeactivatedAfterEdit() ) {
            if ( const CraftObject* after = context.stage->Find(selectedId_) ) {
                history_.PushDone(std::make_unique<CraftParamsCommand>(selectedId_, paramsBefore_, after->params, after->asset));
            }
        }
        selected = context.stage->Find(selectedId_);
        if ( !selected ) { return; }
    }
    if ( ImGui::Button("地面に下ろす") ) {
        CraftObject after = *selected;
        float groundY = after.position.y;
        if ( GroundBelow(context, after.position.x, after.position.z, after.position.y, groundY) ) { after.position.y = groundY; }
        history_.Execute(*context.stage, std::make_unique<CraftTransformCommand>(*selected, after));
    }
    ImGui::SameLine();
    if ( ImGui::Button("複製 (Ctrl+D)") ) { DuplicateSelected(context); }
    ImGui::SameLine();
    if ( ImGui::Button("削除 (Delete)") ) { DeleteSelected(context); }
#else
    ( void ) context;
#endif
}

void CraftEditor::DrawAssets(const Context& context){
#ifdef USE_IMGUI
    ( void ) context;
    CraftAssetCatalog* catalog = CraftAssetCatalog::GetInstance();
    ImGui::TextDisabled("クリックで「1個置く」を始める（Game View で左クリック＝置く / Esc＝終わる）");
    if ( ImGui::BeginTabBar("##assetCategories") ) {
        for ( const std::string& category : catalog->GetCategories() ) {
            if ( !ImGui::BeginTabItem(category.c_str()) ) { continue; }
            for ( const CraftAssetCatalog::Asset& asset : catalog->GetAssets() ) {
                if ( asset.category != category ) { continue; }
                if ( ImGui::Selectable(asset.name.c_str(), tool_ == Tool::Place && placeAsset_ == asset.name) ) {
                    placeAsset_ = asset.name;
                    tool_ = Tool::Place;
                }
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    if ( tool_ == Tool::Place ) {
        ImGui::SliderFloat("置く向き（度）", &placeYawDeg_, -180.0f, 180.0f);
    }
#else
    ( void ) context;
#endif
}

void CraftEditor::DrawBrushSettings(){
#ifdef USE_IMGUI
    ImGui::TextDisabled("Game View の地面をドラッグ＝撒く / Shift＋ドラッグ＝消す（飾りの層だけ）");
    if ( tool_ != Tool::Brush && ImGui::Button("ブラシを使う") ) { tool_ = Tool::Brush; }
    ImGui::DragFloat("半径 (m)", &brushRadius_, 0.05f, 0.3f, 20.0f);
    ImGui::DragFloat("密度（1m²あたり）", &brushDensity_, 0.01f, 0.02f, 5.0f);
    ImGui::DragFloat("最小間隔 (m)", &brushSpacing_, 0.01f, 0.05f, 5.0f);
    ImGui::DragFloatRange2("大きさの幅", &brushScaleMin_, &brushScaleMax_, 0.01f, 0.1f, 4.0f);
    ImGui::SeparatorText("撒く物と割合（0 で撒かない）");
    for ( size_t i = 0; i < brushAssets_.size(); ++i ) {
        ImGui::PushID(( int ) i);
        ImGui::SetNextItemWidth(140.0f);
        ImGui::SliderFloat(brushAssets_[i].asset.c_str(), &brushAssets_[i].weight, 0.0f, 3.0f);
        ImGui::PopID();
    }
    // 飾りの分類のアセットを足せるように
    if ( ImGui::BeginCombo("##addBrushAsset", "撒く物を足す") ) {
        for ( const CraftAssetCatalog::Asset& asset : CraftAssetCatalog::GetInstance()->GetAssets() ) {
            if ( CraftAssetCatalog::DefaultLayerFor(asset.category) != CraftLayer_Decor ) { continue; }
            if ( ImGui::Selectable(asset.name.c_str()) ) { brushAssets_.push_back({ asset.name, 1.0f }); }
        }
        ImGui::EndCombo();
    }
#endif
}

void CraftEditor::DrawRailSettings(const Context& context){
#ifdef USE_IMGUI
    if ( !context.rails || context.rails->empty() ) {
        ImGui::TextDisabled("このシーンにはレールがありません");
        return;
    }
    const int railCount = ( int ) context.rails->size();
    railIndex_ = std::clamp(railIndex_, 0, railCount - 1);
    ImGui::SliderInt("レール番号", &railIndex_, 0, railCount - 1);
    if ( ImGui::BeginCombo("並べる物", railAsset_.c_str()) ) {
        for ( const CraftAssetCatalog::Asset& asset : CraftAssetCatalog::GetInstance()->GetAssets() ) {
            if ( ImGui::Selectable(asset.name.c_str(), asset.name == railAsset_) ) { railAsset_ = asset.name; }
        }
        ImGui::EndCombo();
    }
    ImGui::DragFloat("間隔 (m)", &railInterval_, 0.05f, 0.3f, 50.0f);
    ImGui::DragFloat("レールからの横の距離 (m)", &railOffset_, 0.05f, 0.0f, 30.0f);
    const char* sides[] = { "左", "右", "両側" };
    ImGui::Combo("どちら側", &railSide_, sides, 3);
    const char* facings[] = { "レールに沿う（横長の物を沿わせる）", "レールの方を向く" };
    ImGui::Combo("向き", &railFacing_, facings, 2);
    if ( ImGui::Button("並べる") ) { PlaceAlongRail(context); }
    ImGui::SameLine();
    ImGui::TextDisabled("並べた分は1回の「元に戻す」で消せます");
    // 並べる先のレールを Game View に示す
    const SplineRail& rail = ( *context.rails )[railIndex_];
    for ( float d = 0.0f; d + 0.5f <= rail.GetLength(); d += 0.5f ) {
        DebugDraw::GetInstance()->Line(rail.GetPositionByDistance(d), rail.GetPositionByDistance(d + 0.5f), { 1.0f, 0.8f, 0.2f, 1.0f });
    }
#else
    ( void ) context;
#endif
}

void CraftEditor::DrawHistory(){
#ifdef USE_IMGUI
    ImGui::TextDisabled("▶ ＝ 今の位置 / ● ＝ 保存した位置（%d / %d 個）", history_.GetCursor(), ( int ) history_.GetEntries().size());
    if ( ImGui::BeginChild("##historyList", ImVec2(0, 0), true) ) {
        ImGui::TextDisabled("%s%s（読み込んだ時）", history_.GetCursor() == 0 ? "▶ " : "   ", history_.GetSavedCursor() == 0 ? "●" : " ");
        const auto& entries = history_.GetEntries();
        for ( int i = 0; i < ( int ) entries.size(); ++i ) {
            const bool done = i < history_.GetCursor();
            const std::string line = std::string(history_.GetCursor() == i + 1 ? "▶ " : "   ")
                + ( history_.GetSavedCursor() == i + 1 ? "● " : "  " ) + entries[i]->Label();
            if ( done ) { ImGui::TextUnformatted(line.c_str()); }
            else        { ImGui::TextDisabled("%s", line.c_str()); } // 元に戻した（やり直せる）操作
        }
    }
    ImGui::EndChild();
#endif
}

// =====================================================================
//  キー操作（Game View の上か、このパネルを触っている時だけ。文字入力中は効かない）
// =====================================================================
void CraftEditor::HandleHotkeys(const Context& context, bool gameViewHovered, bool windowFocused){
#ifdef USE_IMGUI
    if ( TypingInImGui() || !( gameViewHovered || windowFocused ) ) { return; }
    const bool ctrl = ImGui::GetIO().KeyCtrl;
    EditorManager* editor = EditorManager::GetInstance();
    // レールエディタも Ctrl+Z / Ctrl+Y を見ているので、このフレームは譲ってもらう（両方が戻る事故を防ぐ）
    if ( ctrl && ( ImGui::IsKeyDown(ImGuiKey_Z) || ImGui::IsKeyDown(ImGuiKey_Y) ) ) {
        if ( LevelEditor* levelEditor = editor->GetLevelEditor() ) { levelEditor->GetRailEditor()->SkipUndoHotkeyThisFrame(); }
    }
    if ( ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, false) ) { Undo(context); }
    if ( ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false) ) { Redo(context); }
    if ( ctrl && ImGui::IsKeyPressed(ImGuiKey_S, false) ) { Save(context); }
    if ( ctrl && ImGui::IsKeyPressed(ImGuiKey_D, false) ) { DuplicateSelected(context); }
    if ( !ctrl ) {
        if ( ImGui::IsKeyPressed(ImGuiKey_W, false) ) { editor->SetGizmoOperation(7); }   // 移動
        if ( ImGui::IsKeyPressed(ImGuiKey_E, false) ) { editor->SetGizmoOperation(120); } // 回転
        if ( ImGui::IsKeyPressed(ImGuiKey_R, false) ) { editor->SetGizmoOperation(896); } // 拡縮
        if ( ImGui::IsKeyPressed(ImGuiKey_F, false) ) { FocusSelected(context); }
    }
    if ( ImGui::IsKeyPressed(ImGuiKey_Delete, false) ) { DeleteSelected(context); }
    if ( ImGui::IsKeyPressed(ImGuiKey_Escape, false) ) { tool_ = Tool::Select; }
#else
    ( void ) context; ( void ) gameViewHovered; ( void ) windowFocused;
#endif
}

// =====================================================================
//  ギズモ：つかんでいる間はデータへ写し続け、離した時に「変更前→変更後」を1回分として積む
// =====================================================================
void CraftEditor::UpdateGizmoDrag(const Context& context){
#ifdef USE_IMGUI
    EditorManager* editor = EditorManager::GetInstance();
    Obj3d* object = selectedId_ ? context.view->FindObject(selectedId_) : nullptr;
    const bool usingGizmo = ownsGizmo_ && object && editor->GetGizmoTarget() == object && ImGuizmo::IsUsing();
    if ( usingGizmo ) {
        if ( !dragging_ ) {
            if ( const CraftObject* before = context.stage->Find(selectedId_) ) { dragBefore_ = *before; }
            dragging_ = true;
            context.view->SetHeldObject(selectedId_); // つかんでいる間は、データから位置を戻さない
        }
        if ( CraftObject* target = context.stage->FindMutable(selectedId_) ) {
            const Vector3 rotation = object->GetRotation();
            target->position = object->GetTranslation();
            target->rotationDeg = { rotation.x * kRadToDeg, rotation.y * kRadToDeg, rotation.z * kRadToDeg };
            target->scale = object->GetScale();
        }
        return;
    }
    if ( !dragging_ ) { return; }
    dragging_ = false;
    context.view->SetHeldObject(0);
    CraftObject* target = context.stage->FindMutable(selectedId_);
    if ( !target ) { return; }
    // 離した時に刻みへ合わせる（Ctrl を押していれば合わせない）
    if ( !ImGui::GetIO().KeyCtrl ) {
        target->position = SnapPosition(*target, target->position);
        target->rotationDeg = { SnapValue(target->rotationDeg.x, kRotationSnap), SnapValue(target->rotationDeg.y, kRotationSnap),
                                SnapValue(target->rotationDeg.z, kRotationSnap) };
    }
    context.stage->Touch();
    history_.PushDone(std::make_unique<CraftTransformCommand>(dragBefore_, *target));
#else
    ( void ) context;
#endif
}

// クリックで選ぶ（ロックした層・隠した層の物は選べない）
void CraftEditor::UpdateSelectClick(const Context& context){
#ifdef USE_IMGUI
    if ( !ImGui::IsMouseClicked(ImGuiMouseButton_Left) ) { return; }
    Vector3 origin, direction;
    if ( !MouseRay(origin, direction) ) { return; }
    Select(PickObject(context, origin, direction));
#else
    ( void ) context;
#endif
}

// 1個ずつ置く：マウスの下の地面に仮置きを出し、左クリックで置く
void CraftEditor::UpdatePlace(const Context& context){
#ifdef USE_IMGUI
    const EditorManager::GameViewMouse& gameView = EditorManager::GetInstance()->GetGameViewMouse();
    if ( placeAsset_.empty() || !gameView.hovered ) { return; }
    Vector3 origin, direction, hit;
    if ( !MouseRay(origin, direction) || !RaycastGround(context, origin, direction, hit) ) { return; }

    CraftObject candidate = MakeObject(placeAsset_, hit, placeYawDeg_, 1.0f);
    if ( !ImGui::GetIO().KeyCtrl ) { candidate.position = SnapPosition(candidate, candidate.position); }

    // 仮置き
    Model* model = ModelManager::GetInstance()->FindModel(placeAsset_);
    if ( !ghost_ ) {
        ghost_ = std::make_unique<Obj3d>();
        ghost_->Initialize(model);
        ghost_->SetCamera(camera_);
    }
    ghost_->SetModel(model);
    ghost_->SetTranslation(candidate.position);
    ghost_->SetRotation(ToRadians(candidate.rotationDeg));
    ghost_->SetScale(candidate.scale);
    ghost_->Update();
    ghostVisible_ = true;
    DrawObjectBox(candidate, { 0.4f, 1.0f, 0.5f, 1.0f });

    if ( ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !gameView.gizmoActive ) {
        history_.Execute(*context.stage, std::make_unique<CraftCreateCommand>(candidate));
    }
#else
    ( void ) context;
#endif
}

// 散布ブラシ：押してから離すまでに置いた（消した）物を、まとめて1回分にする
void CraftEditor::UpdateBrush(const Context& context){
#ifdef USE_IMGUI
    const EditorManager::GameViewMouse& gameView = EditorManager::GetInstance()->GetGameViewMouse();
    const bool pressed = ImGui::IsMouseDown(ImGuiMouseButton_Left) && gameView.hovered && !gameView.gizmoActive;
    Vector3 origin, direction, center;
    const bool hasCenter = gameView.hovered && MouseRay(origin, direction) && RaycastGround(context, origin, direction, center);
    if ( hasCenter ) {
        const bool erase = ImGui::GetIO().KeyShift;
        DrawCircle({ center.x, center.y + 0.05f, center.z }, brushRadius_, erase ? Vector4 { 1.0f, 0.3f, 0.3f, 1.0f } : Vector4 { 0.4f, 1.0f, 0.5f, 1.0f });
    }

    if ( pressed && hasCenter ) {
        if ( !brushStroke_ ) {
            brushErasing_ = ImGui::GetIO().KeyShift;
            brushStroke_ = std::make_unique<CraftGroupCommand>(brushErasing_ ? "ブラシで消す" : "ブラシで撒く");
        }
        const float radiusSq = brushRadius_ * brushRadius_;
        if ( brushErasing_ ) {
            // 円の中の飾りを消す（飾りの層だけ。地面や背景は消さない）
            std::vector<CraftObject> targets;
            for ( const CraftObject& object : context.stage->GetObjects() ) {
                const float dx = object.position.x - center.x, dz = object.position.z - center.z;
                if ( object.layer == CraftLayer_Decor && dx * dx + dz * dz <= radiusSq ) { targets.push_back(object); }
            }
            for ( const CraftObject& object : targets ) {
                auto command = std::make_unique<CraftDeleteCommand>(object);
                command->Execute(*context.stage);
                brushStroke_->Add(std::move(command));
            }
        } else {
            // 円の中が密度に足りなければ、少しずつ足す（1フレームに数個まで）
            float totalWeight = 0.0f;
            for ( const BrushEntry& entry : brushAssets_ ) { totalWeight += ( std::max )( entry.weight, 0.0f ); }
            int existing = 0;
            for ( const CraftObject& object : context.stage->GetObjects() ) {
                const float dx = object.position.x - center.x, dz = object.position.z - center.z;
                if ( object.layer == CraftLayer_Decor && dx * dx + dz * dz <= radiusSq ) { ++existing; }
            }
            const int wanted = ( int ) ( brushDensity_ * kPi * radiusSq );
            int toPlace = ( std::min )( wanted - existing, 3 );
            for ( int attempt = 0; attempt < 12 && toPlace > 0 && totalWeight > 0.0f; ++attempt ) {
                const float angle = Random::Float(0.0f, kPi * 2.0f);
                const float distance = brushRadius_ * std::sqrt(Random::Float(0.0f, 1.0f));
                const float x = center.x + std::cos(angle) * distance;
                const float z = center.z + std::sin(angle) * distance;
                bool tooClose = false;
                for ( const CraftObject& object : context.stage->GetObjects() ) {
                    if ( object.layer != CraftLayer_Decor ) { continue; }
                    const float dx = object.position.x - x, dz = object.position.z - z;
                    if ( dx * dx + dz * dz < brushSpacing_ * brushSpacing_ ) { tooClose = true; break; }
                }
                if ( tooClose ) { continue; }
                // 割合で選ぶ
                float pick = Random::Float(0.0f, totalWeight);
                std::string asset = brushAssets_.front().asset;
                for ( const BrushEntry& entry : brushAssets_ ) {
                    pick -= ( std::max )( entry.weight, 0.0f );
                    if ( pick <= 0.0f && entry.weight > 0.0f ) { asset = entry.asset; break; }
                }
                float y = center.y;
                GroundBelow(context, x, z, center.y, y);
                CraftObject object = MakeObject(asset, { x, y, z }, Random::Float(-180.0f, 180.0f),
                                                Random::Float(brushScaleMin_, brushScaleMax_));
                object.layer = CraftLayer_Decor; // ブラシは飾りの層にしか書かない
                auto command = std::make_unique<CraftCreateCommand>(object);
                command->Execute(*context.stage);
                brushStroke_->Add(std::move(command));
                --toPlace;
            }
        }
    }
    if ( !ImGui::IsMouseDown(ImGuiMouseButton_Left) && brushStroke_ ) {
        if ( !brushStroke_->Empty() ) { history_.PushDone(std::move(brushStroke_)); }
        brushStroke_.reset();
    }
#else
    ( void ) context;
#endif
}

void CraftEditor::DrawSelection(const Context& context) const{
    if ( const CraftObject* selected = selectedId_ ? context.stage->Find(selectedId_) : nullptr ) {
        DrawObjectBox(*selected, { 1.0f, 0.85f, 0.2f, 1.0f });
    }
}

// =====================================================================
//  操作
// =====================================================================
void CraftEditor::Select(uint64_t id){
    selectedId_ = id;
}

void CraftEditor::DeleteSelected(const Context& context){
    const CraftObject* selected = selectedId_ ? context.stage->Find(selectedId_) : nullptr;
    if ( !selected ) { return; }
    const CraftObject copy = *selected;
    Select(0);
    history_.Execute(*context.stage, std::make_unique<CraftDeleteCommand>(copy));
}

// その場に複製して、複製した方を選ぶ（そのままギズモで動かせる）
void CraftEditor::DuplicateSelected(const Context& context){
    const CraftObject* selected = selectedId_ ? context.stage->Find(selectedId_) : nullptr;
    if ( !selected ) { return; }
    CraftObject copy = *selected;
    copy.id = CraftStage::NewId();
    copy.position.x += 0.5f * baseScale_;
    history_.Execute(*context.stage, std::make_unique<CraftCreateCommand>(copy));
    Select(copy.id);
    tool_ = Tool::Select;
}

// 選んだ物へカメラを寄せる（向きはそのまま、物が画面の中央に来る位置へ）
void CraftEditor::FocusSelected(const Context& context){
    const CraftObject* selected = selectedId_ ? context.stage->Find(selectedId_) : nullptr;
    if ( !selected || !context.camera ) { return; }
    const Vector3 rotation = context.camera->GetRotation();
    const Vector3 forward { std::sin(rotation.y) * std::cos(rotation.x), -std::sin(rotation.x), std::cos(rotation.y) * std::cos(rotation.x) };
    Vector3 boxMin, boxMax;
    ObjectBounds(*selected, boxMin, boxMax);
    const float size = VectorMath::Length(boxMax - boxMin) * ( std::max )( selected->scale.x, selected->scale.y );
    const float distance = ( std::max )( size * 2.5f, 4.0f );
    context.camera->SetTranslation(selected->position - forward * distance);
}

// レールに沿って並べる（RMF フレームの「右」を使うので、カーブでも自然に並ぶ）
void CraftEditor::PlaceAlongRail(const Context& context){
    if ( !context.rails || railIndex_ < 0 || railIndex_ >= ( int ) context.rails->size() ) { return; }
    const SplineRail& rail = ( *context.rails )[railIndex_];
    if ( rail.GetLength() <= 0.0f || railInterval_ <= 0.0f ) { return; }
    auto group = std::make_unique<CraftGroupCommand>("レール沿いに並べる");
    for ( float distance = railInterval_ * 0.5f; distance < rail.GetLength(); distance += railInterval_ ) {
        const SplineRail::RailFrame frame = rail.GetFrameAtDistance(distance);
        for ( int side = -1; side <= 1; side += 2 ) {
            if ( ( railSide_ == 0 && side > 0 ) || ( railSide_ == 1 && side < 0 ) ) { continue; }
            Vector3 position = frame.position + frame.right * ( railOffset_ * ( float ) side );
            GroundBelow(context, position.x, position.z, position.y, position.y);
            float yawRad = 0.0f;
            if ( railFacing_ == 0 ) {
                yawRad = std::atan2(-frame.tangent.z, frame.tangent.x);                        // 横長の向き（モデルの X）をレールに沿わせる
            } else {
                yawRad = std::atan2(-frame.right.x * ( float ) side, -frame.right.z * ( float ) side); // 正面（モデルの +Z）をレールへ向ける
            }
            auto command = std::make_unique<CraftCreateCommand>(MakeObject(railAsset_, position, yawRad * kRadToDeg, 1.0f));
            command->Execute(*context.stage);
            group->Add(std::move(command));
        }
    }
    if ( !group->Empty() ) { history_.PushDone(std::move(group)); }
}

void CraftEditor::Undo(const Context& context){
    if ( dragging_ || brushStroke_ ) { return; } // 操作の途中では戻さない
    history_.Undo(*context.stage);
}

void CraftEditor::Redo(const Context& context){
    if ( dragging_ || brushStroke_ ) { return; }
    history_.Redo(*context.stage);
}

CraftObject CraftEditor::MakeObject(const std::string& asset, const Vector3& position, float yawDeg, float scale) const{
    CraftObject object;
    object.id = CraftStage::NewId();
    object.asset = asset;
    const CraftAssetCatalog::Asset* entry = CraftAssetCatalog::GetInstance()->Find(asset);
    object.layer = entry ? CraftAssetCatalog::DefaultLayerFor(entry->category) : CraftLayer_Decor;
    object.position = position;
    object.rotationDeg = { 0.0f, yawDeg, 0.0f };
    const float size = baseScale_ * scale;
    object.scale = { size, size, size };
    return object;
}

// =====================================================================
//  マウス・当たり
// =====================================================================
bool CraftEditor::MouseRay(Vector3& origin, Vector3& direction) const{
    const EditorManager::GameViewMouse& gameView = EditorManager::GetInstance()->GetGameViewMouse();
    if ( gameView.imgSize.x < 1.0f || gameView.imgSize.y < 1.0f ) { return false; }
    const float u = ( gameView.mousePos.x - gameView.imgMin.x ) / gameView.imgSize.x;
    const float v = ( gameView.mousePos.y - gameView.imgMin.y ) / gameView.imgSize.y;
    const float ndcX = u * 2.0f - 1.0f;
    const float ndcY = 1.0f - v * 2.0f;
    const Matrix4x4 inverse = Inverse(gameView.viewProj);
    const Vector3 nearPoint = Transforms({ ndcX, ndcY, 0.0f }, inverse);
    const Vector3 farPoint  = Transforms({ ndcX, ndcY, 1.0f }, inverse);
    origin = nearPoint;
    direction = VectorMath::Normalize(farPoint - nearPoint);
    return true;
}

// 地面の層の物（タイル・段差）の上面へ。何も無ければ高さ0の平面へ
bool CraftEditor::RaycastGround(const Context& context, const Vector3& origin, const Vector3& direction, Vector3& hit) const{
    float best = 1e9f;
    for ( const CraftObject& object : context.stage->GetObjects() ) {
        if ( object.layer != CraftLayer_Ground || !context.view->IsLayerVisible(CraftLayer_Ground) ) { continue; }
        float distance = 0.0f;
        if ( RayHitsObject(object, origin, direction, distance) && distance < best ) { best = distance; }
    }
    if ( best >= 1e9f && direction.y < -0.01f ) {
        // 地面の物が無い所（本編ステージは道が宙に浮いている）：マウスのレイに一番近い道の高さの面に置く。
        //   道からも遠ければ高さ0の面
        float planeY = 0.0f;
        if ( context.rails ) {
            float nearest = 6.0f; // これより遠い道は見ない(m)
            for ( const SplineRail& rail : *context.rails ) {
                for ( float d = 0.0f; d <= rail.GetLength(); d += 1.0f ) {
                    const Vector3 point = rail.GetPositionByDistance(d);
                    const float along = VectorMath::Dot(point - origin, direction);
                    if ( along <= 0.0f ) { continue; }
                    const float gap = VectorMath::Length(point - ( origin + direction * along ));
                    if ( gap < nearest ) { nearest = gap; planeY = point.y; }
                }
            }
        }
        best = ( planeY - origin.y ) / direction.y;
    }
    if ( best >= 1e9f || best < 0.0f ) { return false; }
    hit = origin + direction * best;
    return true;
}

bool CraftEditor::GroundBelow(const Context& context, float x, float z, float fallbackY, float& outY) const{
    Vector3 hit;
    if ( RaycastGround(context, { x, fallbackY + 30.0f, z }, { 0.0f, -1.0f, 0.0f }, hit) ) { outY = hit.y; return true; }
    outY = fallbackY;
    return false;
}

uint64_t CraftEditor::PickObject(const Context& context, const Vector3& origin, const Vector3& direction) const{
    uint64_t picked = 0;
    float best = 1e9f;
    for ( const CraftObject& object : context.stage->GetObjects() ) {
        if ( layerLocked_[object.layer] || !context.view->IsLayerVisible(object.layer) ) { continue; }
        float distance = 0.0f;
        if ( RayHitsObject(object, origin, direction, distance) && distance < best ) { best = distance; picked = object.id; }
    }
    return picked;
}

// 位置の刻み：地面のタイルは大きなマス、それ以外は細かいマス（スナップなしなら合わせない）
Vector3 CraftEditor::SnapPosition(const CraftObject& object, const Vector3& position) const{
    if ( snapMode_ == 2 ) { return position; }
    const float tile = 2.0f * baseScale_;
    const float step = ( object.layer == CraftLayer_Ground || snapMode_ == 0 ) ? tile : 0.5f * baseScale_;
    return { SnapValue(position.x, step), position.y, SnapValue(position.z, step) };
}
