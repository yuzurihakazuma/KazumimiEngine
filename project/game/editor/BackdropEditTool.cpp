#include "game/editor/BackdropEditTool.h"
#include "game/editor/SceneEditContext.h"

#include "engine/3d/model/Model.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/3d/obj/Obj3d.h"
#include "engine/graphics/DebugDraw.h"
#include "engine/graphics/SrvManager.h"
#include "engine/graphics/TextureManager.h"
#include "engine/math/Matrix4x4.h"
#include "engine/utils/Level/LevelEditor.h"
#include "engine/utils/Level/RailEditor.h"
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kDeg = 180.0f / kPi;
    constexpr size_t kUndoLimit = 80;
    constexpr int   kOpTranslate = 7, kOpRotate = 120, kOpScale = 896; // ImGuizmo::OPERATION

    // 置いた物の分類（パレットの並び・消しゴムの対象）
    bool IsDecor(const std::string& model){ return model.rfind("grass_", 0) == 0 || model.rfind("flower_", 0) == 0; }
    bool IsScenery(const std::string& model){
        return model == "sky_backdrop" || model.rfind("hill_", 0) == 0 || model.rfind("cloud_", 0) == 0;
    }
    // パレットに出さない物（タイトル専用の看板・的・コップ）
    bool IsPaletteHidden(const std::string& model){
        return model == "title_board" || model == "menu_target" || model == "paper_cup"
            || model.rfind("ground_", 0) == 0; // 地面は「地面」ブラシで塗る（物として置くと地面の上に重なって浮く）
    }
    bool IsGroundModel(const std::string& model){ return model.rfind("ground_block_", 0) == 0; }
    // パレットの並び（よく使う順）
    int PaletteOrder(const std::string& model){
        if ( model.rfind("grass_", 0) == 0 )  return 0;
        if ( model.rfind("flower_", 0) == 0 ) return 1;
        if ( model.rfind("tree_", 0) == 0 )   return 2;
        if ( model.rfind("paper_rock", 0) == 0 || model == "fence_sticks" || model == "sign_arrow" ) return 3;
        if ( model.rfind("ground_", 0) == 0 ) return 5;
        return 4;
    }
    float Dist2D(const Vector3& a, const Vector3& b){
        const float dx = a.x - b.x, dz = a.z - b.z;
        return std::sqrt(dx * dx + dz * dz);
    }
    bool NearVec(const Vector3& a, const Vector3& b, float eps){
        return std::abs(a.x - b.x) <= eps && std::abs(a.y - b.y) <= eps && std::abs(a.z - b.z) <= eps;
    }
    // 向き（度）から、前（+Z）の向きの水平の角度（度）
    float YawOf(const Vector3& rotRadians){
        const Matrix4x4 m = MatrixMath::MakeAffine({ 1.0f, 1.0f, 1.0f }, rotRadians, { 0.0f, 0.0f, 0.0f });
        const Vector3 f = MatrixMath::Transforms({ 0.0f, 0.0f, 1.0f }, m);
        return std::atan2(f.x, f.z) * kDeg;
    }
    // 光線と水平な面 y=h
    bool RayPlane(const Vector3& origin, const Vector3& dir, float h, Vector3& out){
        if ( std::abs(dir.y) < 1e-4f ) return false;
        const float t = ( h - origin.y ) / dir.y;
        if ( t <= 0.0f || t > 400.0f ) return false;
        out = { origin.x + dir.x * t, h, origin.z + dir.z * t };
        return true;
    }
}

BackdropEditTool::BackdropEditTool(){
    for ( const char* model : { "grass_tuft_00", "grass_tuft_01", "grass_tuft_02" } ) { brushModels_[model] = true; }
    for ( const char* model : { "flower_orange", "flower_red", "flower_white" } ) { brushModels_[model] = false; }
}
BackdropEditTool::~BackdropEditTool(){
    // ギズモが代わりの Obj3d を指したまま消えないように
    EditorManager* editor = EditorManager::GetInstance();
    if ( proxy_ && editor->GetGizmoTarget() == proxy_.get() ) { editor->SetGizmoTarget(nullptr); }
}

void BackdropEditTool::LoadThumbnails(){
    paletteModels_.clear();
    for ( const std::string& model : StageBackdrop::KitModelNames() ) {
        if ( IsPaletteHidden(model) ) { continue; }
        paletteModels_.push_back(model);
        const std::string path = "resources/craft/thumbs/" + model + ".png";
        if ( std::filesystem::exists(path) ) { thumbs_[model] = TextureManager::GetInstance()->Load(path).srvIndex; }
    }
    std::stable_sort(paletteModels_.begin(), paletteModels_.end(),
        [](const std::string& a, const std::string& b){ return PaletteOrder(a) < PaletteOrder(b); });
}

bool BackdropEditTool::IsEditing(const SceneEditContext& context) const{
    return enabled_ && context.editing && context.backdrop && context.stageName
        && context.editor->IsPanelVisible(EditorManager::Panel_Backdrop);
}

// ===== 履歴 =====
void BackdropEditTool::PushUndo(StageBackdrop& backdrop){
    undo_.push_back(backdrop.Items());
    if ( undo_.size() > kUndoLimit ) { undo_.erase(undo_.begin()); }
    redo_.clear();
}

void BackdropEditTool::Undo(StageBackdrop& backdrop){
    if ( undo_.empty() ) return;
    redo_.push_back(backdrop.Items());
    backdrop.SetItems(undo_.back());
    undo_.pop_back();
    selection_.clear();
    Changed();
    status_ = "元に戻した";
}

void BackdropEditTool::Redo(StageBackdrop& backdrop){
    if ( redo_.empty() ) return;
    undo_.push_back(backdrop.Items());
    backdrop.SetItems(redo_.back());
    redo_.pop_back();
    selection_.clear();
    Changed();
    status_ = "やり直した";
}

// ===== 置く物 =====
BackdropItem BackdropEditTool::MakeItem(const std::string& model, const Vector3& at){
    auto random01 = [this](){ rng_ = rng_ * 1664525u + 1013904223u; return ( ( rng_ >> 8 ) & 0xFFFF ) / 65535.0f; };
    BackdropItem item;
    item.model = model;
    item.pos = at;
    const float turn = randomTurn_ ? ( random01() * 2.0f - 1.0f ) * turnRange_ : 0.0f;
    item.rot = { 0.0f, 180.0f + turn, 0.0f };
    const float s = scaleMin_ + ( scaleMax_ - scaleMin_ ) * random01();
    item.scale = { s, s, s };
    return item;
}

bool BackdropEditTool::CanSelect(const StageBackdrop& backdrop, int index) const{
    if ( index < 0 || index >= ( int ) backdrop.Items().size() ) return false;
    const BackdropItem& item = backdrop.Items()[index];
    if ( item.removed || backdrop.IsFloor(index) ) return false;
    // 地面・丘・雲・空は「地面・丘・空も選ぶ」の時だけ（大きくて手前の草や木を選びにくくするので）
    return includeGround_ || !( backdrop.IsGround(index) || IsScenery(item.model) );
}

// ===== パネル =====
void BackdropEditTool::DrawPanel(const SceneEditContext& context){
#ifdef USE_IMGUI
    if ( !context.editor->IsPanelVisible(EditorManager::Panel_Backdrop) ) return;
    if ( !ImGui::Begin("背景エディタ (Backdrop)") ) { ImGui::End(); return; }
    StageBackdrop* backdrop = context.backdrop;
    if ( !backdrop || !context.stageName ) {
        ImGui::TextDisabled("このシーンには背景がありません");
        ImGui::End();
        return;
    }
    ImGui::Checkbox("背景を編集する（Game View のクリックを背景に使う）", &enabled_);
    if ( !context.editing ) { ImGui::TextColored({ 1.0f, 0.7f, 0.3f, 1.0f }, "Edit モードの時だけ編集できます"); }

    // 道具（パネルの幅に5つ並べる。狭いアイコンモードのドロワーでもはみ出さない）
    const float toolWidth = ( std::max )( 40.0f, ( ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 4.0f ) / 5.0f );
    auto toolButton = [&](const char* label, Tool tool){
        const bool on = ( tool_ == tool );
        if ( on ) { ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive)); }
        if ( ImGui::Button(label, { toolWidth, 0.0f }) ) { tool_ = tool; }
        if ( on ) { ImGui::PopStyleColor(); }
    };
    toolButton("選ぶ##bd", Tool::Select);   ImGui::SameLine();
    toolButton("置く##bd", Tool::Place);    ImGui::SameLine();
    toolButton("ブラシ##bd", Tool::Brush);  ImGui::SameLine();
    toolButton("消しゴム##bd", Tool::Eraser); ImGui::SameLine();
    toolButton("地面##bd", Tool::Ground);

    switch ( tool_ ) {
    case Tool::Select:
        ImGui::TextDisabled("クリック＝選ぶ（Shift で追加）／空いた所をドラッグ＝範囲選択\n"
                            "つかんでドラッグ＝地面の上を動かす（Ctrl で複製して動かす）\n"
                            "Delete＝消す  Ctrl+D＝複製  右クリック＝メニュー");
        ImGui::Checkbox("地面・丘・空も選ぶ", &includeGround_);
        break;
    case Tool::Place:
        ImGui::TextDisabled("パレットで選んで、Game View の地面をクリック（続けて置ける）");
        break;
    case Tool::Brush:
        ImGui::TextDisabled("左ドラッグで塗る（チェックした物をまぜて置く）");
        ImGui::SliderFloat("ブラシの大きさ", &brushRadius_, 0.5f, 6.0f, "%.1f m");
        ImGui::SliderFloat("すき間", &brushSpacing_, 0.3f, 2.5f, "%.1f m");
        {
            int column = 0;
            for ( auto& [model, on] : brushModels_ ) {
                if ( column++ % 3 != 0 ) { ImGui::SameLine(); }
                ImGui::Checkbox(( model + "##brush" ).c_str(), &on);
            }
        }
        break;
    case Tool::Eraser:
        ImGui::TextDisabled("左ドラッグで消す");
        ImGui::SliderFloat("消しゴムの大きさ", &brushRadius_, 0.5f, 6.0f, "%.1f m");
        ImGui::Checkbox("草・花だけ消す", &eraseDecorOnly_);
        break;
    case Tool::Ground:
        DrawGroundPanel();
        break;
    }
    if ( tool_ == Tool::Place || tool_ == Tool::Brush ) {
        ImGui::Checkbox("向きをばらす", &randomTurn_);
        if ( randomTurn_ ) { ImGui::SameLine(); ImGui::SetNextItemWidth(110.0f); ImGui::SliderFloat("##turn", &turnRange_, 0.0f, 180.0f, "±%.0f°"); }
        ImGui::DragFloatRange2("大きさ", &scaleMin_, &scaleMax_, 0.02f, 0.2f, 6.0f, "%.2f", "%.2f");
    }

    ImGui::Separator();
    DrawPalette();
    ImGui::Separator();
    DrawSelectionInfo(context);
    ImGui::Separator();

    // 保存・履歴・作り直し
    ImGui::BeginDisabled(undo_.empty());
    if ( ImGui::Button("元に戻す (Ctrl+Z)##bd") ) { Undo(*backdrop); }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(redo_.empty());
    if ( ImGui::Button("やり直す (Ctrl+Y)##bd") ) { Redo(*backdrop); }
    ImGui::EndDisabled();
    if ( ImGui::Button(dirty_ ? "手直しを保存 (Ctrl+S) *##bd" : "手直しを保存 (Ctrl+S)##bd") ) { Save(context); }
    ImGui::SameLine();
    if ( ImGui::Button("自動配置をやり直す##bd") ) { Regenerate(context); }
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip("道（マップ）に合わせて自動配置を作り直す（make_stage_backdrop.py）。\n手で直した所はそのまま残る（同じ所にある物に当て直す）");
    }
    if ( ImGui::Button("手直しを全部捨てる##bd") ) { ImGui::OpenPopup("bdDiscard"); }
    if ( ImGui::BeginPopup("bdDiscard") ) {
        ImGui::Text("手で直した所を全部捨てて、自動配置のままに戻しますか？\n（元に戻す で取り消せます）");
        if ( ImGui::Button("捨てる##bdyes") ) {
            PushUndo(*backdrop);
            backdrop->ResetToGenerated();
            selection_.clear();
            Changed();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if ( ImGui::Button("やめる##bdno") ) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    ImGui::TextDisabled("置いてある数 %d  /  手直しファイル %s", ( int ) backdrop->GetObjectCount(),
        StageBackdrop::EditPath(context.stageName).c_str());
    if ( !status_.empty() ) { ImGui::TextWrapped("%s", status_.c_str()); }

    // パネルにマウスがある時もキー操作を受ける（Game View の上と同じ）
    if ( ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && IsEditing(context) ) { HandleKeys(context); }
    ImGui::End();
#else
    ( void ) context;
#endif
}

void BackdropEditTool::DrawPalette(){
#ifdef USE_IMGUI
    ImGui::Text("パレット（クリック＝置く物に選ぶ／Game View へドラッグでも置ける）");
    const float cell = 54.0f;
    const float width = ImGui::GetContentRegionAvail().x;
    const int columns = ( std::max )( 1, ( int ) ( width / ( cell + 8.0f ) ) );
    int column = 0;
    for ( const std::string& model : paletteModels_ ) {
        if ( column++ % columns != 0 ) { ImGui::SameLine(); }
        ImGui::PushID(model.c_str());
        const bool selected = ( model == placeModel_ );
        if ( selected ) { ImGui::PushStyleColor(ImGuiCol_Button, { 0.95f, 0.65f, 0.2f, 0.9f }); }
        bool clicked = false;
        auto thumb = thumbs_.find(model);
        if ( thumb != thumbs_.end() ) {
            const auto handle = SrvManager::GetInstance()->GetGPUDescriptorHandle(thumb->second);
            clicked = ImGui::ImageButton("##thumb", ( ImTextureID ) ( uintptr_t ) handle.ptr, { cell, cell });
        } else {
            clicked = ImGui::Button(model.substr(0, 6).c_str(), { cell + 6.0f, cell + 6.0f });
        }
        if ( selected ) { ImGui::PopStyleColor(); }
        if ( clicked ) {
            placeModel_ = model;
            if ( tool_ != Tool::Brush ) { tool_ = Tool::Place; }
        }
        if ( ImGui::IsItemHovered() ) { ImGui::SetTooltip("%s", model.c_str()); }
        if ( ImGui::BeginDragDropSource() ) {
            ImGui::SetDragDropPayload("DND_MODEL", model.c_str(), model.size() + 1);
            ImGui::Text("背景に置く: %s", model.c_str());
            ImGui::EndDragDropSource();
        }
        ImGui::PopID();
    }
#endif
}

void BackdropEditTool::DrawSelectionInfo(const SceneEditContext& context){
#ifdef USE_IMGUI
    StageBackdrop& backdrop = *context.backdrop;
    if ( selection_.empty() ) {
        ImGui::TextDisabled("選んでいる物はありません");
        return;
    }
    const int primary = selection_.front();
    if ( primary < 0 || primary >= ( int ) backdrop.Items().size() ) { selection_.clear(); return; }
    BackdropItem item = backdrop.Items()[primary];
    if ( selection_.size() == 1 ) {
        ImGui::Text("選択: %s%s", item.model.c_str(), item.source < 0 ? "（手で置いた物）" : "");
        bool edited = false;
        const bool wasActive = ImGui::IsAnyItemActive();
        edited |= ImGui::DragFloat3("位置##bd", &item.pos.x, 0.05f);
        edited |= ImGui::DragFloat3("向き(度)##bd", &item.rot.x, 0.5f);
        edited |= ImGui::DragFloat3("大きさ##bd", &item.scale.x, 0.01f, 0.05f, 20.0f);
        if ( edited ) {
            if ( !wasActive || !gizmoEditing_ ) { PushUndo(backdrop); gizmoEditing_ = true; }
            backdrop.SetItem(primary, item);
            SyncProxy(backdrop);
            Changed();
        }
        if ( !ImGui::IsAnyItemActive() && !ImGui::IsMouseDown(0) ) { gizmoEditing_ = false; }
    } else {
        ImGui::Text("%d 個選んでいます（先頭: %s）", ( int ) selection_.size(), item.model.c_str());
    }
    if ( ImGui::Button("消す (Delete)##bdsel") ) { PushUndo(backdrop); DeleteSelection(backdrop); }
    ImGui::SameLine();
    if ( ImGui::Button("複製 (Ctrl+D)##bdsel") ) { PushUndo(backdrop); DuplicateSelection(backdrop); }
    ImGui::SameLine();
    if ( ImGui::Button("地面に合わせる##bdsel") ) { PushUndo(backdrop); SnapSelectionToGround(backdrop); }
    // ギズモの種類（Transform パネルと同じ設定）
    const int op = context.editor->GetGizmoOperation();
    if ( ImGui::RadioButton("移動##bdop", op == kOpTranslate) ) { context.editor->SetGizmoOperation(kOpTranslate); }
    ImGui::SameLine();
    if ( ImGui::RadioButton("回転##bdop", op == kOpRotate) ) { context.editor->SetGizmoOperation(kOpRotate); }
    ImGui::SameLine();
    if ( ImGui::RadioButton("拡縮##bdop", op == kOpScale) ) { context.editor->SetGizmoOperation(kOpScale); }
#else
    ( void ) context;
#endif
}

// ===== Game View =====
bool BackdropEditTool::UpdateGameView(const SceneEditContext& context){
#ifdef USE_IMGUI
    HandleDrop(context); // ドロップは「背景を編集する」が OFF でも受ける（置いたら編集を ON に）
    if ( !IsEditing(context) ) {
        Cancel(context);
        return false;
    }
    StageBackdrop& backdrop = *context.backdrop;
    // 消えた番号を選んでいたら外す
    selection_.erase(std::remove_if(selection_.begin(), selection_.end(),
        [&](int index){ return index < 0 || index >= ( int ) backdrop.Items().size() || backdrop.Items()[index].removed; }),
        selection_.end());

    const auto& view = context.View();
    // Game View のクリックは背景に使う（レールの選択・ノードのキー操作を止める）。レールの Ctrl+Z/Y もこちらで受ける
    if ( view.hovered || drag_ != Drag::None || stroking_ ) { context.editor->SetExternalDragActive(true); }
    if ( context.railEditor ) { context.railEditor->SkipUndoHotkeyThisFrame(); }

    // マウスの下の物
    int hover = -1;
    Vector3 origin, dir;
    if ( view.hovered && !view.gizmoActive && context.MouseRay(origin, dir) ) {
        hover = backdrop.Pick(origin, dir, includeGround_);
        if ( !CanSelect(backdrop, hover) ) { hover = -1; }
    }

    if ( view.hovered ) { HandleKeys(context); }
    switch ( tool_ ) {
    case Tool::Select: UpdateSelect(context, hover); break;
    case Tool::Place:  UpdatePlace(context);  break;
    case Tool::Brush:  UpdateBrush(context, false); break;
    case Tool::Eraser: UpdateBrush(context, true);  break;
    case Tool::Ground: UpdateGround(context); break;
    }
    UpdateContextMenu(context, hover);
    UpdateGizmo(context);

    // 見た目：マウスの下（白）・選んでいる物（黄、先頭は橙）
    if ( hover >= 0 && tool_ == Tool::Select && drag_ == Drag::None ) { DrawBox(backdrop, hover, { 1.0f, 1.0f, 1.0f, 0.9f }); }
    for ( size_t k = 0; k < selection_.size(); ++k ) {
        DrawBox(backdrop, selection_[k], k == 0 ? Vector4 { 1.0f, 0.55f, 0.1f, 1.0f } : Vector4 { 1.0f, 0.9f, 0.2f, 1.0f });
    }
    return true;
#else
    ( void ) context;
    return false;
#endif
}

void BackdropEditTool::Cancel(const SceneEditContext& context){
    drag_ = Drag::None;
    stroking_ = false;
    ReleaseGizmo(context);
}

void BackdropEditTool::HandleDrop(const SceneEditContext& context){
#ifdef USE_IMGUI
    if ( !context.editing || !context.backdrop ) return;
    std::string model;
    Vector2 mouse;
    if ( !context.editor->ConsumeGameViewDrop(model, mouse) ) return;
    // キットのモデル（背景に置ける物）だけ受ける
    if ( std::find(paletteModels_.begin(), paletteModels_.end(), model) == paletteModels_.end()
        && !ModelManager::GetInstance()->FindModel(model) ) { status_ = model + " は背景に置けません（クラフトのキットの物だけ）"; return; }
    const auto names = StageBackdrop::KitModelNames();
    if ( std::find(names.begin(), names.end(), model) == names.end() ) { status_ = model + " は背景に置けません（クラフトのキットの物だけ）"; return; }
    if ( model.rfind("ground_", 0) == 0 ) {
        // 地面のブロック・タイルは物として置くと地面の上に重なって浮くので、「地面」ブラシで塗る
        tool_ = Tool::Ground;
        groundPaint_ = model.find("path") != std::string::npos ? GroundPaint::Path : GroundPaint::Grass;
        enabled_ = true;
        context.editor->SetPanelVisible(EditorManager::Panel_Backdrop, true);
        status_ = "地面は「地面」ブラシで塗ってください（ブラシに切り替えた）";
        return;
    }
    StageBackdrop& backdrop = *context.backdrop;
    Vector3 origin, dir, at;
    if ( !context.ScreenRay(mouse, origin, dir) || !backdrop.HitGround(origin, dir, at) ) {
        status_ = "地面の上に落としてください";
        return;
    }
    PushUndo(backdrop);
    const int index = backdrop.AddItem(MakeItem(model, at));
    selection_ = { index };
    placeModel_ = model;
    enabled_ = true;
    context.editor->SetPanelVisible(EditorManager::Panel_Backdrop, true);
    tool_ = Tool::Select;
    Changed();
    status_ = model + " を置いた";
#else
    ( void ) context;
#endif
}

void BackdropEditTool::HandleKeys(const SceneEditContext& context){
#ifdef USE_IMGUI
    ImGuiIO& io = ImGui::GetIO();
    if ( io.WantTextInput || ImGui::IsMouseDown(1) || drag_ != Drag::None || stroking_ ) return;
    StageBackdrop& backdrop = *context.backdrop;
    if ( io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false) ) { Undo(backdrop); }
    if ( io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false) ) { Redo(backdrop); }
    if ( io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false) ) { Save(context); }
    if ( !selection_.empty() && ImGui::IsKeyPressed(ImGuiKey_Delete, false) ) { PushUndo(backdrop); DeleteSelection(backdrop); }
    if ( !selection_.empty() && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false) ) { PushUndo(backdrop); DuplicateSelection(backdrop); }
    if ( ImGui::IsKeyPressed(ImGuiKey_Escape, false) ) { selection_.clear(); }
#else
    ( void ) context;
#endif
}

// 選ぶ道具：クリック・Shift・範囲選択・つかんで地面の上を動かす
void BackdropEditTool::UpdateSelect(const SceneEditContext& context, int hover){
#ifdef USE_IMGUI
    StageBackdrop& backdrop = *context.backdrop;
    const auto& view = context.View();
    ImGuiIO& io = ImGui::GetIO();
    Vector3 origin, dir;
    const bool hasRay = context.MouseRay(origin, dir);

    if ( drag_ == Drag::None && view.hovered && !view.gizmoActive && ImGui::IsMouseClicked(0) ) {
        dragStart_ = view.mousePos;
        pressItem_ = hover;
        drag_ = Drag::Pending;
        if ( hover >= 0 ) {
            const bool inSelection = std::find(selection_.begin(), selection_.end(), hover) != selection_.end();
            if ( io.KeyShift ) {
                if ( inSelection ) { selection_.erase(std::find(selection_.begin(), selection_.end(), hover)); }
                else               { selection_.push_back(hover); }
                drag_ = Drag::None; // Shift クリックは選ぶだけ
            } else if ( !inSelection ) {
                selection_ = { hover };
                // 線で引いた道などのまとまりは、1本まるごと選ぶ（Alt+クリックで1つだけ）
                const int group = backdrop.Items()[hover].group;
                if ( group != 0 && !io.KeyAlt ) {
                    for ( int i = 0; i < ( int ) backdrop.Items().size(); ++i ) {
                        if ( i != hover && backdrop.Items()[i].group == group && !backdrop.Items()[i].removed ) { selection_.push_back(i); }
                    }
                }
            } else {
                // 選んでいる中の物をつかんだ：それを先頭に（ギズモの対象）
                selection_.erase(std::find(selection_.begin(), selection_.end(), hover));
                selection_.insert(selection_.begin(), hover);
            }
            SyncProxy(backdrop);
        }
    }
    if ( drag_ == Drag::None ) return;

    const float dx = view.mousePos.x - dragStart_.x, dy = view.mousePos.y - dragStart_.y;
    const bool moved = dx * dx + dy * dy > 16.0f;
    if ( drag_ == Drag::Pending && ImGui::IsMouseDown(0) && moved ) {
        if ( pressItem_ >= 0 && hasRay ) {
            // つかんで動かす：つかんだ物の根元（地面に置いてある所）を基準に。
            //   木のてっぺんなど空を背にした所をつかんでも、根元を指しているのと同じに動かす
            const BackdropItem& item = backdrop.Items()[pressItem_];
            Vector2 baseScreen;
            if ( !context.ProjectToScreen(item.pos, baseScreen) ) { drag_ = Drag::None; return; }
            grabOffset_ = { baseScreen.x - dragStart_.x, baseScreen.y - dragStart_.y };
            Vector3 eye, ray;
            if ( !context.ScreenRay(baseScreen, eye, ray) ) { drag_ = Drag::None; return; }
            if ( !backdrop.HitGround(eye, ray, moveAnchor_) && !RayPlane(eye, ray, item.pos.y, moveAnchor_) ) { drag_ = Drag::None; return; }
            PushUndo(backdrop);
            moveDuplicated_ = io.KeyCtrl;
            if ( moveDuplicated_ ) { DuplicateSelection(backdrop); } // Ctrl＋ドラッグ＝複製して動かす（複製した方を動かす）
            moveStart_.clear();
            for ( int index : selection_ ) { moveStart_.push_back(backdrop.Items()[index]); }
            drag_ = Drag::Move;
        } else {
            drag_ = Drag::Box;
        }
    }

    if ( drag_ == Drag::Move ) {
        Vector3 now, eye, ray;
        const bool baseRay = context.ScreenRay({ view.mousePos.x + grabOffset_.x, view.mousePos.y + grabOffset_.y }, eye, ray);
        if ( baseRay && ( backdrop.HitGround(eye, ray, now) || RayPlane(eye, ray, moveAnchor_.y, now) ) ) {
            const float mx = now.x - moveAnchor_.x, mz = now.z - moveAnchor_.z;
            for ( size_t k = 0; k < selection_.size() && k < moveStart_.size(); ++k ) {
                BackdropItem item = moveStart_[k];
                // 地面からの高さを保つ（段差を越えても地面に沿う）
                float before = 0.0f, after = 0.0f;
                const bool hadGround = backdrop.GroundHeightAt(item.pos.x, item.pos.z, before);
                item.pos.x += mx;
                item.pos.z += mz;
                if ( hadGround && backdrop.GroundHeightAt(item.pos.x, item.pos.z, after) ) { item.pos.y += after - before; }
                backdrop.SetItem(selection_[k], item);
            }
            Changed();
        }
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    }
    if ( drag_ == Drag::Box ) {
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        drawList->AddRectFilled({ dragStart_.x, dragStart_.y }, { view.mousePos.x, view.mousePos.y }, IM_COL32(255, 200, 60, 40));
        drawList->AddRect({ dragStart_.x, dragStart_.y }, { view.mousePos.x, view.mousePos.y }, IM_COL32(255, 200, 60, 220), 0.0f, 0, 1.5f);
    }

    if ( !ImGui::IsMouseDown(0) ) {
        if ( drag_ == Drag::Pending && pressItem_ < 0 && !io.KeyShift ) { selection_.clear(); } // 空いた所をクリック＝選ぶのをやめる
        if ( drag_ == Drag::Box ) {
            const float x0 = ( std::min )( dragStart_.x, view.mousePos.x ), x1 = ( std::max )( dragStart_.x, view.mousePos.x );
            const float y0 = ( std::min )( dragStart_.y, view.mousePos.y ), y1 = ( std::max )( dragStart_.y, view.mousePos.y );
            if ( !io.KeyShift ) { selection_.clear(); }
            for ( int i = 0; i < ( int ) backdrop.Items().size(); ++i ) {
                if ( !CanSelect(backdrop, i) ) continue;
                Vector3 lo, hi;
                if ( !backdrop.WorldBounds(i, lo, hi) ) continue;
                Vector2 screen;
                const Vector3 center { ( lo.x + hi.x ) * 0.5f, ( lo.y + hi.y ) * 0.5f, ( lo.z + hi.z ) * 0.5f };
                if ( !context.ProjectToScreen(center, screen) ) continue;
                if ( screen.x < x0 || screen.x > x1 || screen.y < y0 || screen.y > y1 ) continue;
                if ( std::find(selection_.begin(), selection_.end(), i) == selection_.end() ) { selection_.push_back(i); }
            }
            status_ = std::to_string(selection_.size()) + " 個選んだ";
        }
        if ( drag_ == Drag::Move ) { SyncProxy(backdrop); }
        drag_ = Drag::None;
        SyncProxy(backdrop);
    }
#else
    ( void ) context; ( void ) hover;
#endif
}

// 置く道具：パレットで選んだ物を、クリックした地面へ
void BackdropEditTool::UpdatePlace(const SceneEditContext& context){
#ifdef USE_IMGUI
    StageBackdrop& backdrop = *context.backdrop;
    const auto& view = context.View();
    Vector3 origin, dir, at;
    if ( !view.hovered || view.gizmoActive || !context.MouseRay(origin, dir) ) return;
    if ( !backdrop.HitGround(origin, dir, at) ) {
        if ( ImGui::IsMouseClicked(0) ) { status_ = "地面の無い所です。先に「地面」ブラシで地面を作ってから置いてください"; }
        return;
    }
    DrawModelGhost(placeModel_, at, { 0.4f, 1.0f, 0.5f, 0.9f });
    if ( ImGui::IsMouseClicked(0) ) {
        PushUndo(backdrop);
        const int index = backdrop.AddItem(MakeItem(placeModel_, at));
        selection_ = { index };
        SyncProxy(backdrop);
        Changed();
    }
#else
    ( void ) context;
#endif
}

// ブラシ（草むら・花畑を塗る）／消しゴム。ひと塗り＝元に戻す1回分
void BackdropEditTool::UpdateBrush(const SceneEditContext& context, bool erase){
#ifdef USE_IMGUI
    StageBackdrop& backdrop = *context.backdrop;
    const auto& view = context.View();
    Vector3 origin, dir, center;
    const bool onGround = view.hovered && context.MouseRay(origin, dir) && backdrop.HitGround(origin, dir, center);
    if ( onGround ) {
        DrawCircle(backdrop, center, brushRadius_, erase ? Vector4 { 1.0f, 0.35f, 0.3f, 1.0f } : Vector4 { 0.4f, 1.0f, 0.5f, 1.0f });
    }
    if ( !ImGui::IsMouseDown(0) ) {
        if ( stroking_ && undo_.size() && undo_.back() == backdrop.Items() ) { undo_.pop_back(); } // 何も変わらなかった
        stroking_ = false;
        return;
    }
    if ( !onGround ) return;
    if ( !stroking_ ) {
        if ( !ImGui::IsMouseClicked(0) || view.gizmoActive ) return; // Game View の外から押したまま入ってきた時は塗らない
        PushUndo(backdrop);
        stroking_ = true;
        selection_.clear();
        lastStamp_ = { 1e9f, 0.0f, 0.0f };
    }
    auto random01 = [this](){ rng_ = rng_ * 1664525u + 1013904223u; return ( ( rng_ >> 8 ) & 0xFFFF ) / 65535.0f; };

    if ( erase ) {
        bool any = false;
        for ( int i = ( int ) backdrop.Items().size() - 1; i >= 0; --i ) {
            const BackdropItem& item = backdrop.Items()[i];
            if ( !CanSelect(backdrop, i) ) continue;
            if ( eraseDecorOnly_ && !IsDecor(item.model) ) continue;
            if ( IsScenery(item.model) || backdrop.IsGround(i) ) continue; // 消しゴムで地面・丘・空は消さない
            if ( Dist2D(item.pos, center) > brushRadius_ || std::abs(item.pos.y - center.y) > 3.0f ) continue;
            backdrop.RemoveItem(i);
            any = true;
        }
        if ( any ) { Changed(); }
        return;
    }

    // 塗る：前に置いた所から半径の半分以上動いたら、円の中にすき間を空けて置く
    if ( Dist2D(center, lastStamp_) < brushRadius_ * 0.5f ) return;
    lastStamp_ = center;
    std::vector<std::string> models;
    for ( const auto& [model, on] : brushModels_ ) { if ( on ) { models.push_back(model); } }
    if ( models.empty() ) { status_ = "ブラシで置く物にチェックを入れてください"; return; }
    const int attempts = ( int ) ( brushRadius_ * brushRadius_ * kPi / ( brushSpacing_ * brushSpacing_ ) ) + 1;
    int placed = 0;
    for ( int k = 0; k < attempts; ++k ) {
        const float angle = random01() * 2.0f * kPi;
        const float radius = std::sqrt(random01()) * brushRadius_;
        Vector3 at { center.x + std::cos(angle) * radius, center.y, center.z + std::sin(angle) * radius };
        if ( !backdrop.GroundHeightAt(at.x, at.z, at.y) ) continue;
        bool crowded = false;
        for ( int i = 0; i < ( int ) backdrop.Items().size() && !crowded; ++i ) {
            const BackdropItem& item = backdrop.Items()[i];
            if ( item.removed || backdrop.IsGround(i) || backdrop.IsFloor(i) || IsScenery(item.model) ) continue;
            crowded = Dist2D(item.pos, at) < brushSpacing_ && std::abs(item.pos.y - at.y) < 1.0f;
        }
        if ( crowded ) continue;
        const std::string& model = models[( size_t ) ( random01() * models.size() ) % models.size()];
        backdrop.AddItem(MakeItem(model, at));
        ++placed;
    }
    if ( placed ) { Changed(); }
#else
    ( void ) context; ( void ) erase;
#endif
}

// ===== 地面ブラシ =====
void BackdropEditTool::DrawGroundPanel(){
#ifdef USE_IMGUI
    ImGui::TextDisabled("左ドラッグで升目（1.2m）を塗る。地面の無い所（段ボールの底の上）にも作れる");
    auto paintButton = [&](const char* label, GroundPaint paint){
        if ( ImGui::RadioButton(label, groundPaint_ == paint) ) { groundPaint_ = paint; }
    };
    paintButton("草##gp", GroundPaint::Grass); ImGui::SameLine();
    paintButton("道（紙）##gp", GroundPaint::Path); ImGui::SameLine();
    paintButton("土（段ボール）##gp", GroundPaint::Dirt);
    paintButton("穴にする##gp", GroundPaint::Hole); ImGui::SameLine();
    paintButton("段を上げる##gp", GroundPaint::Raise); ImGui::SameLine();
    paintButton("段を下げる##gp", GroundPaint::Lower);
    paintButton("道を線で引く（つながった道）##gp", GroundPaint::PathLine);
    if ( groundPaint_ == GroundPaint::PathLine ) {
        ImGui::TextDisabled("左ドラッグで線を引くと、なめらかにつながった紙の道（段ボールのふち付き）ができる。\n"
                            "曲がり角と端は丸くなる。できた道はクリック1回で1本まるごと選べる（Alt+クリックで1つだけ）");
        ImGui::SliderFloat("道の幅", &pathWidth_, 0.6f, 4.0f, "%.1f m");
        ImGui::Checkbox("道の上の草花をどける", &clearDecor_);
        return;
    }
    ImGui::SliderInt("塗る大きさ（升）", &groundSize_, 1, 9);
    if ( groundPaint_ != GroundPaint::Hole && groundPaint_ != GroundPaint::Raise && groundPaint_ != GroundPaint::Lower ) {
        ImGui::DragFloat("地面の無い所に作る高さ", &groundHeight_, 0.05f, -5.0f, 20.0f, "%.2f m");
        ImGui::SameLine();
        if ( ImGui::SmallButton("-段##gh") ) { groundHeight_ -= StageBackdrop::kStepHeight; }
        ImGui::SameLine();
        if ( ImGui::SmallButton("+段##gh") ) { groundHeight_ += StageBackdrop::kStepHeight; }
        ImGui::Checkbox("塗り始めた所の地面の高さを使う", &pickHeight_);
        ImGui::Checkbox("塗った所の高さもそろえる", &alignHeight_);
    }
    if ( groundPaint_ == GroundPaint::Path || groundPaint_ == GroundPaint::Hole ) {
        ImGui::Checkbox(groundPaint_ == GroundPaint::Hole ? "穴の上の物もどける" : "道の上の草花をどける", &clearDecor_);
    }
#endif
}

void BackdropEditTool::UpdateGround(const SceneEditContext& context){
#ifdef USE_IMGUI
    if ( groundPaint_ == GroundPaint::PathLine ) { UpdatePathLine(context); return; }
    StageBackdrop& backdrop = *context.backdrop;
    const auto& view = context.View();
    Vector3 origin, dir;
    if ( !view.hovered || !context.MouseRay(origin, dir) ) {
        if ( !ImGui::IsMouseDown(0) ) { stroking_ = false; }
        return;
    }
    // 指している升目：地面があればその上、無ければ「作る高さ」の面
    Vector3 point;
    bool onGround = backdrop.HitGround(origin, dir, point);
    if ( !onGround && !RayPlane(origin, dir, groundHeight_, point) ) { return; }
    int column, row;
    backdrop.CellOf(point.x, point.z, column, row);
    const int half = ( groundSize_ - 1 ) / 2;
    const int first = -half, last = groundSize_ - 1 - half;

    // 見た目：塗る升目の枠（今の高さ・地面が無ければ作る高さ）
    const Vector4 color = groundPaint_ == GroundPaint::Hole ? Vector4 { 1.0f, 0.35f, 0.3f, 1.0f }
                        : groundPaint_ == GroundPaint::Path ? Vector4 { 1.0f, 0.85f, 0.3f, 1.0f }
                        : groundPaint_ == GroundPaint::Dirt ? Vector4 { 0.85f, 0.6f, 0.35f, 1.0f }
                        : Vector4 { 0.4f, 1.0f, 0.5f, 1.0f };
    const float cell = backdrop.GridCell();
    for ( int a = first; a <= last; ++a ) {
        for ( int b = first; b <= last; ++b ) {
            float top = groundHeight_;
            backdrop.CellTop(column + a, row + b, top);
            const Vector3 c = backdrop.CellCenter(column + a, row + b);
            const float h = cell * 0.5f - 0.03f, y = top + 0.04f;
            const Vector3 p0 { c.x - h, y, c.z - h }, p1 { c.x + h, y, c.z - h }, p2 { c.x + h, y, c.z + h }, p3 { c.x - h, y, c.z + h };
            DebugDraw::GetInstance()->Line(p0, p1, color);
            DebugDraw::GetInstance()->Line(p1, p2, color);
            DebugDraw::GetInstance()->Line(p2, p3, color);
            DebugDraw::GetInstance()->Line(p3, p0, color);
        }
    }

    if ( !ImGui::IsMouseDown(0) ) {
        if ( stroking_ && !strokeCells_.empty() ) {
            // 塗った所のまわりを、同じ種類・同じ高さどうしで大きいブロックにまとめ直す（1升ずつのままだと細かく見える）
            int column0 = strokeCells_.begin()->first, column1 = column0;
            int row0 = strokeCells_.begin()->second, row1 = row0;
            for ( const auto& [c, r] : strokeCells_ ) {
                column0 = ( std::min )( column0, c ); column1 = ( std::max )( column1, c );
                row0 = ( std::min )( row0, r );       row1 = ( std::max )( row1, r );
            }
            backdrop.MergeCells(column0 - 2, row0 - 2, column1 + 2, row1 + 2);
        }
        if ( stroking_ && !undo_.empty() && undo_.back() == backdrop.Items() ) { undo_.pop_back(); } // 何も変わらなかった
        stroking_ = false;
        strokeCells_.clear();
        return;
    }
    if ( !stroking_ ) {
        if ( !ImGui::IsMouseClicked(0) || view.gizmoActive ) return;
        PushUndo(backdrop);
        stroking_ = true;
        selection_.clear();
        strokeCells_.clear();
        // 塗り始めた所の地面の高さを使う（続けて地面の無い所へ塗り広げても、同じ高さでつながる）
        float top;
        if ( pickHeight_ && onGround && backdrop.CellTop(column, row, top) ) { groundHeight_ = top; }
        lastStrokePoint_ = point;
    }
    // 前のフレームの所から今の所まで、升目の半分ずつたどって塗る（マウスを速く動かしても途切れない）
    bool any = false;
    const float dx = point.x - lastStrokePoint_.x, dz = point.z - lastStrokePoint_.z;
    const int steps = ( std::max )( 1, ( int ) std::ceil(std::sqrt(dx * dx + dz * dz) / ( cell * 0.5f )) );
    for ( int k = 1; k <= steps; ++k ) {
        const float t = ( float ) k / steps;
        int stepColumn, stepRow;
        backdrop.CellOf(lastStrokePoint_.x + dx * t, lastStrokePoint_.z + dz * t, stepColumn, stepRow);
        for ( int a = first; a <= last; ++a ) {
            for ( int b = first; b <= last; ++b ) {
                if ( !strokeCells_.insert({ stepColumn + a, stepRow + b }).second ) continue; // このひと塗りで塗った所は1回だけ
                ApplyGroundCell(backdrop, stepColumn + a, stepRow + b);
                any = true;
            }
        }
    }
    lastStrokePoint_ = point;
    if ( any ) { Changed(); }
#else
    ( void ) context;
#endif
}

// 道を線で引く：ドラッグした線をなめらかにして、紙の帯と丸（曲がり角・端）を並べる
void BackdropEditTool::UpdatePathLine(const SceneEditContext& context){
#ifdef USE_IMGUI
    StageBackdrop& backdrop = *context.backdrop;
    const auto& view = context.View();
    Vector3 origin, dir, point;
    const bool hasPoint = view.hovered && context.MouseRay(origin, dir)
        && ( backdrop.HitGround(origin, dir, point) || RayPlane(origin, dir, groundHeight_, point) );
    const Vector4 color { 1.0f, 0.85f, 0.3f, 1.0f };
    if ( hasPoint ) { DrawCircle(backdrop, point, pathWidth_ * 0.5f, color); }
    // 引いている途中の線
    for ( size_t k = 1; k < linePoints_.size(); ++k ) {
        DebugDraw::GetInstance()->Line({ linePoints_[k - 1].x, linePoints_[k - 1].y + 0.08f, linePoints_[k - 1].z },
                                       { linePoints_[k].x, linePoints_[k].y + 0.08f, linePoints_[k].z }, color);
    }
    if ( !ImGui::IsMouseDown(0) ) {
        if ( stroking_ ) {
            if ( linePoints_.size() >= 2 ) {
                BuildPath(backdrop);
                Changed();
                status_ = "道を引いた（クリックで1本まるごと選べる）";
            } else if ( !undo_.empty() ) {
                undo_.pop_back(); // 線になっていない（クリックだけ）
            }
        }
        stroking_ = false;
        linePoints_.clear();
        return;
    }
    if ( !hasPoint ) return;
    if ( !stroking_ ) {
        if ( !ImGui::IsMouseClicked(0) || view.gizmoActive ) return;
        PushUndo(backdrop);
        stroking_ = true;
        selection_.clear();
        linePoints_ = { point };
        return;
    }
    if ( Dist2D(point, linePoints_.back()) >= 0.3f ) { linePoints_.push_back(point); }
#else
    ( void ) context;
#endif
}

namespace {
    // モデルの元の大きさ（余白なし）
    bool RawModelBounds(const std::string& name, Vector3& outMin, Vector3& outMax){
        Model* model = ModelManager::GetInstance()->FindModel(name);
        if ( !model || model->GetModelData().vertices.empty() ) return false;
        outMin = { 1e9f, 1e9f, 1e9f };
        outMax = { -1e9f, -1e9f, -1e9f };
        for ( const auto& vertex : model->GetModelData().vertices ) {
            outMin = { ( std::min )( outMin.x, vertex.position.x ), ( std::min )( outMin.y, vertex.position.y ), ( std::min )( outMin.z, vertex.position.z ) };
            outMax = { ( std::max )( outMax.x, vertex.position.x ), ( std::max )( outMax.y, vertex.position.y ), ( std::max )( outMax.z, vertex.position.z ) };
        }
        return true;
    }
    // 線を間引く（Ramer-Douglas-Peucker。手ぶれのギザギザを消す）
    void Simplify(const std::vector<Vector3>& points, size_t first, size_t last, float epsilon, std::vector<bool>& keep){
        if ( last <= first + 1 ) return;
        const Vector3& a = points[first];
        const Vector3& b = points[last];
        const float lx = b.x - a.x, lz = b.z - a.z;
        const float length = std::sqrt(lx * lx + lz * lz);
        float worst = -1.0f;
        size_t worstIndex = first;
        for ( size_t k = first + 1; k < last; ++k ) {
            const float px = points[k].x - a.x, pz = points[k].z - a.z;
            const float d = length > 1e-4f ? std::abs(px * lz - pz * lx) / length : std::sqrt(px * px + pz * pz);
            if ( d > worst ) { worst = d; worstIndex = k; }
        }
        if ( worst > epsilon ) {
            keep[worstIndex] = true;
            Simplify(points, first, worstIndex, epsilon, keep);
            Simplify(points, worstIndex, last, epsilon, keep);
        }
    }
}

void BackdropEditTool::BuildPath(StageBackdrop& backdrop){
    // 1) 手ぶれを間引いて、2) なめらかな曲線（Catmull-Rom）にして 0.5m ごとに点を取る
    std::vector<bool> keep(linePoints_.size(), false);
    keep.front() = keep.back() = true;
    Simplify(linePoints_, 0, linePoints_.size() - 1, 0.25f, keep);
    std::vector<Vector3> control;
    for ( size_t k = 0; k < linePoints_.size(); ++k ) { if ( keep[k] ) { control.push_back(linePoints_[k]); } }
    std::vector<Vector3> samples;
    for ( size_t k = 0; k + 1 < control.size(); ++k ) {
        const Vector3& p1 = control[k];
        const Vector3& p2 = control[k + 1];
        const Vector3 p0 = k > 0 ? control[k - 1] : Vector3 { 2 * p1.x - p2.x, p1.y, 2 * p1.z - p2.z };
        const Vector3 p3 = k + 2 < control.size() ? control[k + 2] : Vector3 { 2 * p2.x - p1.x, p2.y, 2 * p2.z - p1.z };
        const int count = ( std::max )( 1, ( int ) std::ceil(Dist2D(p1, p2) / 0.5f) );
        for ( int i = 0; i < count; ++i ) {
            const float t = ( float ) i / count, t2 = t * t, t3 = t2 * t;
            auto cr = [&](float a, float b, float c, float d){ return 0.5f * ( 2 * b + ( -a + c ) * t + ( 2 * a - 5 * b + 4 * c - d ) * t2 + ( -a + 3 * b - 3 * c + d ) * t3 ); };
            samples.push_back({ cr(p0.x, p1.x, p2.x, p3.x), 0.0f, cr(p0.z, p1.z, p2.z, p3.z) });
        }
    }
    samples.push_back(control.back());
    // 地面の高さ（無い所は「作る高さ」）
    for ( Vector3& p : samples ) {
        float y;
        p.y = backdrop.GroundHeightAt(p.x, p.z, y) ? y : groundHeight_;
    }
    // 道の上の草花をどける
    if ( clearDecor_ ) {
        for ( int i = ( int ) backdrop.Items().size() - 1; i >= 0; --i ) {
            const BackdropItem& item = backdrop.Items()[i];
            if ( item.removed || !( IsDecor(item.model) || item.model.rfind("paper_rock", 0) == 0 ) ) continue;
            for ( const Vector3& p : samples ) {
                if ( Dist2D(item.pos, p) < pathWidth_ * 0.5f + 0.35f && std::abs(item.pos.y - p.y) < 0.6f ) { backdrop.RemoveItem(i); break; }
            }
        }
    }
    // 3) 帯（2点の間）と丸（曲がり角・端）を並べる。下に少し広い段ボールのふち
    const int group = backdrop.NextGroup();
    Vector3 stripMin, stripMax, discMin, discMax;
    if ( !RawModelBounds(StageBackdrop::kPathStrip, stripMin, stripMax) || !RawModelBounds(StageBackdrop::kPathDisc, discMin, discMax) ) return;
    const float edge = 0.22f; // ふちの幅（両側の合計）
    auto place = [&](const char* model, const Vector3& at, float yaw, const Vector3& scale, float bottom, float minY, float pitch = 0.0f){
        BackdropItem item;
        item.model = model;
        item.pos = { at.x, at.y + bottom - minY * scale.y, at.z };
        item.rot = { pitch, yaw, 0.0f };
        item.scale = scale;
        item.group = group;
        backdrop.AddItem(item);
    };
    for ( size_t k = 0; k + 1 < samples.size(); ++k ) {
        const Vector3& a = samples[k];
        const Vector3& b = samples[k + 1];
        const float dx = b.x - a.x, dz = b.z - a.z;
        const float length = std::sqrt(dx * dx + dz * dz);
        if ( length < 0.05f ) continue;
        // 段差をまたぐ所は、帯を傾けて小さな坂にする（浮いたり埋まったりしない）
        const float dy = b.y - a.y;
        const float slope = std::sqrt(length * length + dy * dy);
        const float pitch = -std::atan2(dy, length) * kDeg; // 行ベクトル×Rx：正で +Z が下を向くので、上り坂は負
        const Vector3 mid { ( a.x + b.x ) * 0.5f, ( a.y + b.y ) * 0.5f, ( a.z + b.z ) * 0.5f };
        const float yaw = std::atan2(dx, dz) * kDeg; // 行ベクトル×Ry：ローカルの +Z が (sin, 0, cos) を向く
        place(StageBackdrop::kPathEdgeStrip, mid, yaw, { pathWidth_ + edge, 0.012f, slope + 0.04f }, 0.002f, stripMin.y, pitch);
        place(StageBackdrop::kPathStrip,     mid, yaw, { pathWidth_,        0.020f, slope + 0.04f }, 0.006f, stripMin.y, pitch);
    }
    // 丸：端と、向きが変わる所だけ（まっすぐな所は帯どうしがつながっている）
    for ( size_t k = 0; k < samples.size(); ++k ) {
        bool corner = ( k == 0 || k + 1 == samples.size() );
        if ( !corner ) {
            const float a0 = std::atan2(samples[k].x - samples[k - 1].x, samples[k].z - samples[k - 1].z);
            const float a1 = std::atan2(samples[k + 1].x - samples[k].x, samples[k + 1].z - samples[k].z);
            float turn = std::abs(a1 - a0);
            if ( turn > kPi ) turn = 2 * kPi - turn;
            corner = turn > 0.04f;
        }
        if ( !corner ) continue;
        place(StageBackdrop::kPathEdgeDisc, samples[k], 0.0f, { pathWidth_ + edge, 0.0135f, pathWidth_ + edge }, 0.002f, discMin.y);
        place(StageBackdrop::kPathDisc,     samples[k], 0.0f, { pathWidth_,        0.0215f, pathWidth_ },        0.006f, discMin.y);
    }
}

void BackdropEditTool::ApplyGroundCell(StageBackdrop& backdrop, int column, int row){
    float top = 0.0f;
    GroundKind kind = GroundKind::Grass;
    const bool exists = backdrop.CellTop(column, row, top, &kind);
    switch ( groundPaint_ ) {
    case GroundPaint::Grass:
    case GroundPaint::Path:
    case GroundPaint::Dirt: {
        const GroundKind next = groundPaint_ == GroundPaint::Path ? GroundKind::Path
                              : groundPaint_ == GroundPaint::Dirt ? GroundKind::Dirt : GroundKind::Grass;
        const float nextTop = ( exists && !alignHeight_ ) ? top : groundHeight_;
        if ( exists && std::abs(nextTop - top) > 0.001f ) { MoveCellDecor(backdrop, column, row, top, nextTop - top); }
        backdrop.PaintCell(column, row, next, nextTop);
        if ( groundPaint_ == GroundPaint::Path && clearDecor_ ) { ClearCellDecor(backdrop, column, row, false); }
        break;
    }
    case GroundPaint::Hole:
        if ( !exists ) break;
        if ( clearDecor_ ) { ClearCellDecor(backdrop, column, row, true); }
        backdrop.EraseCell(column, row);
        break;
    case GroundPaint::Raise:
    case GroundPaint::Lower: {
        if ( !exists ) break;
        const float delta = groundPaint_ == GroundPaint::Raise ? StageBackdrop::kStepHeight : -StageBackdrop::kStepHeight;
        MoveCellDecor(backdrop, column, row, top, delta);
        backdrop.PaintCell(column, row, kind, top + delta);
        break;
    }
    }
}

// 升目の上の飾りを消す（includeProps=true なら木・柵・岩なども）
void BackdropEditTool::ClearCellDecor(StageBackdrop& backdrop, int column, int row, bool includeProps){
    for ( int i = ( int ) backdrop.Items().size() - 1; i >= 0; --i ) {
        const BackdropItem& item = backdrop.Items()[i];
        if ( item.removed || IsGroundModel(item.model) || IsScenery(item.model) ) continue;
        if ( !includeProps && !IsDecor(item.model) && item.model.rfind("paper_rock", 0) != 0 ) continue;
        int c, r;
        backdrop.CellOf(item.pos.x, item.pos.z, c, r);
        if ( c == column && r == row ) { backdrop.RemoveItem(i); }
    }
}

// 升目の上に置いてある物を、地面と一緒に上げ下げする
void BackdropEditTool::MoveCellDecor(StageBackdrop& backdrop, int column, int row, float fromTop, float delta){
    for ( int i = 0; i < ( int ) backdrop.Items().size(); ++i ) {
        BackdropItem item = backdrop.Items()[i];
        if ( item.removed || IsGroundModel(item.model) || IsScenery(item.model) ) continue;
        if ( std::abs(item.pos.y - fromTop) > 0.3f ) continue; // その地面の上に立っている物だけ
        int c, r;
        backdrop.CellOf(item.pos.x, item.pos.z, c, r);
        if ( c != column || r != row ) continue;
        item.pos.y += delta;
        backdrop.SetItem(i, item);
    }
}

// 右クリック（動かさずに離す）＝メニュー
void BackdropEditTool::UpdateContextMenu(const SceneEditContext& context, int hover){
#ifdef USE_IMGUI
    StageBackdrop& backdrop = *context.backdrop;
    const auto& view = context.View();
    ImGuiIO& io = ImGui::GetIO();
    const float dragX = io.MousePos.x - io.MouseClickedPos[1].x;
    const float dragY = io.MousePos.y - io.MouseClickedPos[1].y;
    // 右ドラッグでカメラを回した時は開かない（カーソルがほぼ動かなくても、カメラが動いていれば回した）
    if ( ImGui::IsMouseClicked(ImGuiMouseButton_Right) ) { rightPressViewProj_ = view.viewProj; }
    bool cameraMoved = false;
    for ( int a = 0; a < 4 && !cameraMoved; ++a ) {
        for ( int b = 0; b < 4; ++b ) {
            if ( std::abs(view.viewProj.m[a][b] - rightPressViewProj_.m[a][b]) > 1e-4f ) { cameraMoved = true; break; }
        }
    }
    const bool rightClickedStill = ImGui::IsMouseReleased(ImGuiMouseButton_Right) && std::abs(dragX) < 4.0f && std::abs(dragY) < 4.0f
        && !cameraMoved;
    if ( view.hovered && !view.gizmoActive && rightClickedStill && drag_ == Drag::None && !stroking_ ) {
        menuItem_ = hover;
        if ( hover >= 0 && std::find(selection_.begin(), selection_.end(), hover) == selection_.end() ) {
            selection_ = { hover };
            SyncProxy(backdrop);
        }
        ImGui::OpenPopup("bdContext");
    }
    if ( ImGui::BeginPopup("bdContext") ) {
        if ( !selection_.empty() ) {
            const std::string model = backdrop.Items()[selection_.front()].model;
            ImGui::TextDisabled("%s%s", model.c_str(), selection_.size() > 1 ? "（ほか）" : "");
            ImGui::Separator();
            if ( ImGui::MenuItem("消す", "Delete") ) { PushUndo(backdrop); DeleteSelection(backdrop); }
            if ( ImGui::MenuItem("複製", "Ctrl+D") ) { PushUndo(backdrop); DuplicateSelection(backdrop); }
            if ( ImGui::MenuItem("地面に合わせる") ) { PushUndo(backdrop); SnapSelectionToGround(backdrop); }
            if ( ImGui::MenuItem(( "同じ種類を全部選ぶ（" + model + "）" ).c_str()) ) { SelectSameModel(backdrop, model); }
        } else {
            ImGui::TextDisabled("何もありません");
        }
        ImGui::Separator();
        if ( ImGui::MenuItem("ここに置く（パレットの物）") ) {
            Vector3 origin, dir, at;
            if ( context.ScreenRay({ io.MouseClickedPos[1].x, io.MouseClickedPos[1].y }, origin, dir) && backdrop.HitGround(origin, dir, at) ) {
                PushUndo(backdrop);
                selection_ = { backdrop.AddItem(MakeItem(placeModel_, at)) };
                SyncProxy(backdrop);
                Changed();
            }
        }
        ImGui::EndPopup();
    }
#else
    ( void ) context; ( void ) hover;
#endif
}

// ===== ギズモ（選んだ物の代わりの Obj3d） =====
void BackdropEditTool::SyncProxy(const StageBackdrop& backdrop){
    if ( selection_.empty() || !proxy_ ) return;
    const int primary = selection_.front();
    if ( primary < 0 || primary >= ( int ) backdrop.Items().size() ) return;
    const BackdropItem& item = backdrop.Items()[primary];
    proxy_->SetTranslation(item.pos);
    proxy_->SetRotation({ item.rot.x / kDeg, item.rot.y / kDeg, item.rot.z / kDeg });
    proxy_->SetScale(item.scale);
    proxyT_ = proxy_->GetTranslation();
    proxyR_ = proxy_->GetRotation();
    proxyS_ = proxy_->GetScale();
}

void BackdropEditTool::UpdateGizmo(const SceneEditContext& context){
#ifdef USE_IMGUI
    StageBackdrop& backdrop = *context.backdrop;
    if ( selection_.empty() || drag_ == Drag::Move ) {
        ReleaseGizmo(context);
        return;
    }
    if ( !proxy_ ) {
        Model* model = ModelManager::GetInstance()->FindModel(backdrop.Items()[selection_.front()].model);
        if ( !model ) return;
        proxy_ = std::make_unique<Obj3d>();
        proxy_->Initialize(model);
        SyncProxy(backdrop);
    }
    if ( context.editor->GetGizmoTarget() != proxy_.get() ) {
        context.editor->SetGizmoTarget(proxy_.get());
        SyncProxy(backdrop);
        return;
    }
    const Vector3 t = proxy_->GetTranslation(), r = proxy_->GetRotation(), s = proxy_->GetScale();
    const bool changed = !NearVec(t, proxyT_, 1e-5f) || !NearVec(r, proxyR_, 1e-5f) || !NearVec(s, proxyS_, 1e-5f);
    if ( !changed ) {
        if ( gizmoEditing_ && !ImGui::IsMouseDown(0) && !ImGui::IsAnyItemActive() ) { gizmoEditing_ = false; }
        return;
    }
    if ( !gizmoEditing_ ) { PushUndo(backdrop); gizmoEditing_ = true; }
    const int op = context.editor->GetGizmoOperation();
    const bool byGizmo = context.View().gizmoActive && ImGui::IsMouseDown(0);
    if ( selection_.size() == 1 ) {
        // 1つだけ：そのまま（ギズモで動かしている時は、その操作の値だけ使う。分解し直しで向きが変わらないように）
        BackdropItem item = backdrop.Items()[selection_.front()];
        if ( !byGizmo || op == kOpTranslate ) { item.pos = t; }
        if ( !byGizmo || op == kOpRotate )    { item.rot = { r.x * kDeg, r.y * kDeg, r.z * kDeg }; }
        if ( !byGizmo || op == kOpScale )     { item.scale = s; }
        backdrop.SetItem(selection_.front(), item);
    } else {
        // たくさん：移動はそのまま足す／回転は先頭を中心に水平に回す／拡縮は割合を掛ける
        const Vector3 moved { t.x - proxyT_.x, t.y - proxyT_.y, t.z - proxyT_.z };
        float turn = YawOf(r) - YawOf(proxyR_);
        while ( turn > 180.0f ) turn -= 360.0f;
        while ( turn < -180.0f ) turn += 360.0f;
        const Vector3 ratio { proxyS_.x != 0.0f ? s.x / proxyS_.x : 1.0f, proxyS_.y != 0.0f ? s.y / proxyS_.y : 1.0f,
                              proxyS_.z != 0.0f ? s.z / proxyS_.z : 1.0f };
        const bool useMove = !byGizmo || op == kOpTranslate, useTurn = !byGizmo || op == kOpRotate, useScale = !byGizmo || op == kOpScale;
        const Vector3 pivot = proxyT_;
        const float c = std::cos(turn / kDeg), sn = std::sin(turn / kDeg);
        for ( int index : selection_ ) {
            BackdropItem item = backdrop.Items()[index];
            if ( useMove ) { item.pos = { item.pos.x + moved.x, item.pos.y + moved.y, item.pos.z + moved.z }; }
            if ( useTurn && std::abs(turn) > 1e-4f ) {
                const float dx = item.pos.x - pivot.x, dz = item.pos.z - pivot.z;
                item.pos.x = pivot.x + dx * c + dz * sn; // 行ベクトル×Ry と同じ回り（ゲームの向きと合わせる）
                item.pos.z = pivot.z - dx * sn + dz * c;
                item.rot.y += turn;
            }
            if ( useScale ) { item.scale = { item.scale.x * ratio.x, item.scale.y * ratio.y, item.scale.z * ratio.z }; }
            backdrop.SetItem(index, item);
        }
    }
    proxyT_ = t;
    proxyR_ = r;
    proxyS_ = s;
    Changed();
#else
    ( void ) context;
#endif
}

void BackdropEditTool::ReleaseGizmo(const SceneEditContext& context){
    if ( proxy_ && context.editor && context.editor->GetGizmoTarget() == proxy_.get() ) { context.editor->SetGizmoTarget(nullptr); }
    gizmoEditing_ = false;
}

// ===== まとめた操作 =====
void BackdropEditTool::DeleteSelection(StageBackdrop& backdrop){
    std::vector<int> indices = selection_;
    std::sort(indices.rbegin(), indices.rend()); // 後ろから（手で置いた物は取り除かれて番号が詰まる）
    for ( int index : indices ) { backdrop.RemoveItem(index); }
    status_ = std::to_string(indices.size()) + " 個消した";
    selection_.clear();
    Changed();
}

void BackdropEditTool::DuplicateSelection(StageBackdrop& backdrop){
    std::vector<int> copies;
    for ( int index : selection_ ) {
        BackdropItem item = backdrop.Items()[index];
        item.pos.x += moveDuplicated_ ? 0.0f : 0.6f; // ボタン・Ctrl+D は少しずらす（Ctrl＋ドラッグは同じ所から動かす）
        copies.push_back(backdrop.AddItem(item));
    }
    selection_ = copies;
    moveDuplicated_ = false;
    SyncProxy(backdrop);
    Changed();
}

void BackdropEditTool::SelectSameModel(const StageBackdrop& backdrop, const std::string& model){
    selection_.clear();
    for ( int i = 0; i < ( int ) backdrop.Items().size(); ++i ) {
        if ( backdrop.Items()[i].model == model && CanSelect(backdrop, i) ) { selection_.push_back(i); }
    }
    SyncProxy(backdrop);
    status_ = model + " を " + std::to_string(selection_.size()) + " 個選んだ";
}

void BackdropEditTool::SnapSelectionToGround(StageBackdrop& backdrop){
    for ( int index : selection_ ) {
        BackdropItem item = backdrop.Items()[index];
        float y;
        if ( backdrop.GroundHeightAt(item.pos.x, item.pos.z, y) ) { item.pos.y = y; backdrop.SetItem(index, item); }
    }
    SyncProxy(backdrop);
    Changed();
}

void BackdropEditTool::Save(const SceneEditContext& context){
    if ( !context.backdrop || !context.stageName ) return;
    if ( context.backdrop->SaveEdits(context.stageName) ) {
        dirty_ = false;
        status_ = "保存した: " + StageBackdrop::EditPath(context.stageName);
    } else {
        status_ = "保存できませんでした: " + StageBackdrop::EditPath(context.stageName);
    }
}

// 自動配置を作り直す（python で make_stage_backdrop.py を流して、手直しを重ね直す）
void BackdropEditTool::Regenerate(const SceneEditContext& context){
    if ( !context.backdrop || !context.stageName ) return;
    StageBackdrop& backdrop = *context.backdrop;
    const nlohmann::json edits = backdrop.MakeEdits();
    std::string command = std::string("python ../tools/stage_intro/make_stage_backdrop.py ") + context.stageName;
    STARTUPINFOA startup {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION process {};
    std::vector<char> line(command.begin(), command.end());
    line.push_back('\0');
    if ( !CreateProcessA(nullptr, line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) ) {
        status_ = "python を起動できませんでした（python が PATH にあるか確認）";
        return;
    }
    WaitForSingleObject(process.hProcess, 60000);
    DWORD exitCode = 1;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    if ( exitCode != 0 ) {
        status_ = "自動配置の作り直しに失敗しました（make_stage_backdrop.py のエラー）";
        return;
    }
    PushUndo(backdrop);
    int missed = 0;
    backdrop.ReloadBase(context.stageName, edits, &missed);
    undo_.clear(); // 自動配置が変わったので、前の状態へは戻さない（番号が合わない）
    redo_.clear();
    selection_.clear();
    Changed();
    status_ = "自動配置を作り直した" + ( missed ? "（手直しのうち " + std::to_string(missed) + " 個は、元の物が無くなったので当たらなかった）" : std::string() );
}

// ===== 見た目 =====
void BackdropEditTool::DrawBox(const StageBackdrop& backdrop, int index, const Vector4& color) const{
    Vector3 c[8];
    if ( !backdrop.WorldCorners(index, c) ) return;
    static const int kEdges[12][2] = { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 },
                                       { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
    for ( const auto& edge : kEdges ) { DebugDraw::GetInstance()->Line(c[edge[0]], c[edge[1]], color); }
}

void BackdropEditTool::DrawModelGhost(const std::string& model, const Vector3& at, const Vector4& color) const{
    // 置く予定の物の箱（置く時の大きさの真ん中）
    Model* data = ModelManager::GetInstance()->FindModel(model);
    if ( !data ) return;
    Vector3 lo { 1e9f, 1e9f, 1e9f }, hi { -1e9f, -1e9f, -1e9f };
    for ( const auto& vertex : data->GetModelData().vertices ) {
        lo = { ( std::min )( lo.x, vertex.position.x ), ( std::min )( lo.y, vertex.position.y ), ( std::min )( lo.z, vertex.position.z ) };
        hi = { ( std::max )( hi.x, vertex.position.x ), ( std::max )( hi.y, vertex.position.y ), ( std::max )( hi.z, vertex.position.z ) };
    }
    const float s = ( scaleMin_ + scaleMax_ ) * 0.5f;
    DebugDraw::GetInstance()->Box({ at.x, at.y + ( lo.y + hi.y ) * 0.5f * s, at.z },
        { ( hi.x - lo.x ) * s, ( hi.y - lo.y ) * s, ( std::max )( ( hi.z - lo.z ) * s, 0.05f ) }, color);
    DebugDraw::GetInstance()->Sphere(at, 0.08f, color, 8);
}

void BackdropEditTool::DrawCircle(const StageBackdrop& backdrop, const Vector3& center, float radius, const Vector4& color) const{
    const int segments = 32;
    Vector3 prev {};
    for ( int k = 0; k <= segments; ++k ) {
        const float angle = 2.0f * kPi * k / segments;
        Vector3 p { center.x + std::cos(angle) * radius, center.y, center.z + std::sin(angle) * radius };
        float y;
        if ( backdrop.GroundHeightAt(p.x, p.z, y) ) { p.y = y; }
        p.y += 0.05f;
        if ( k > 0 ) { DebugDraw::GetInstance()->Line(prev, p, color); }
        prev = p;
    }
}
