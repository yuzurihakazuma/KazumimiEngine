#include "ScreenCapture.h"

#include <d3d12.h>
#include "externals/DirectXTex/DirectXTex.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/Input.h"
#include "engine/base/TimeManager.h"
#include "engine/camera/Camera.h"
#include "engine/graphics/RenderTexture.h"
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <Windows.h>
#include <shellapi.h>
#include <wincodec.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>

#pragma comment(lib, "shell32.lib")

namespace {
    constexpr int    kWorkerCount = 3;     // PNG の書き出しを並べるスレッドの数
    constexpr size_t kMaxPendingJobs = 8;  // 保存待ちがこれを超えたら待つ（1枚 8MB ほど）
    constexpr int    kFramesPerSecond = 60;

    // UTF-8 の文字列 ⇔ パス（日本語の名前でも化けないように）
    std::filesystem::path PathFromUtf8(const std::string& text){
        return std::filesystem::path(std::u8string(text.begin(), text.end()));
    }
    std::string Utf8FromPath(const std::filesystem::path& path){
        const std::u8string text = path.generic_u8string();
        return std::string(text.begin(), text.end());
    }
    std::wstring Quote(const std::filesystem::path& path){ return L"\"" + path.wstring() + L"\""; }
}

// 書き出し待ちの1枚
struct ScreenCapture::Job {
    DirectX::ScratchImage image;
    std::filesystem::path path;
    bool resize = false;
};

ScreenCapture::ScreenCapture() = default;
ScreenCapture::~ScreenCapture() = default;

ScreenCapture* ScreenCapture::GetInstance(){
    static ScreenCapture instance;
    return &instance;
}

// =====================================================================
//  初期化・終了
// =====================================================================
void ScreenCapture::Initialize(){
    if ( initialized_ ) { return; }
    initialized_ = true;
    stopWorkers_ = false;
    for ( int i = 0; i < kWorkerCount; ++i ) { workers_.emplace_back([this]{ WorkerLoop(); }); }
    DirectXCommon::GetInstance()->SetBeforePresentHook([this](ID3D12Resource* backBuffer){ BeforePresent(backBuffer); });
}

void ScreenCapture::Finalize(){
    if ( !initialized_ ) { return; }
    DirectXCommon::GetInstance()->SetBeforePresentHook(nullptr);
    recording_ = false;
    takeState_ = TakeState::Idle;
    Time::GetInstance()->SetFixedStep(false);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopWorkers_ = true;
    }
    jobReady_.notify_all();
    for ( auto& worker : workers_ ) { if ( worker.joinable() ) { worker.join(); } }
    workers_.clear();
    if ( gifProcess_ ) { CloseHandle(static_cast< HANDLE >( gifProcess_ )); gifProcess_ = nullptr; }
    hooks_ = {};
    source_ = nullptr;
    camera_ = nullptr;
    initialized_ = false;
}

std::filesystem::path ScreenCapture::CaptureDir() const{
    std::filesystem::path dir = "capture";
    std::error_code error;
    std::filesystem::create_directories(dir, error);
    return dir;
}

std::string ScreenCapture::TimeStamp(){
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local {};
    localtime_s(&local, &time);
    char text[32];
    std::strftime(text, sizeof(text), "%Y%m%d_%H%M%S", &local);
    return text;
}

// 固定コマ送り：録画中（設定が ON の時）と、テイクの記録・再生中（入力はフレーム単位なので、dt がずれると別の動きになる）
void ScreenCapture::UpdateFixedStep(){
    const bool fixed = ( recording_ && fixedStep_ ) || takeState_ != TakeState::Idle;
    Time::GetInstance()->SetFixedStep(fixed);
}

// =====================================================================
//  毎フレーム：キー・テイク
// =====================================================================
void ScreenCapture::OnInputUpdated(){
    if ( !initialized_ ) { return; }
    Input* input = Input::GetInstance();

    // 撮影のキー（遷移の演出中・文字入力中でも効く）
    if ( input->TriggerkeyRaw(DIK_F12) ) { RequestShot(); }
    if ( input->TriggerkeyRaw(DIK_F9) ) { // F11 はフルスクリーンの切り替え（WindowProc）なので F9
        if ( recording_ ) { StopRecording(); }
        else              { StartRecording(useLimit_ ? limitSeconds_ * kFramesPerSecond : 0); }
    }

    // テイクの記録：始める時の状態を作ってから、毎フレームのキーを覚える
    if ( takeState_ == TakeState::RecordPending ) {
        nlohmann::json start = nlohmann::json::object();
        std::string error;
        if ( hooks_.capture && !hooks_.capture(start, error) ) {
            status_ = "テイク：" + error;
            takeState_ = TakeState::Idle;
        } else {
            takeStart_ = start;
            takeScene_ = hooks_.scene;
            if ( hooks_.apply ) { hooks_.apply(takeStart_); }
            takeFrames_.clear();
            takeState_ = TakeState::Recording;
            status_ = hooks_.apply ? "テイクを記録中（始める時の状態へ戻した）" : "テイクを記録中（このシーンは始める時の状態へ戻せない）";
        }
    }
    if ( takeState_ == TakeState::Recording ) {
        takeFrames_.push_back(input->GetPressedKeysForTake());
    }

    // テイクの再生：始める時の状態へ戻し、記録したキーをフレームごとに差し替える
    if ( takeState_ == TakeState::PlayPending ) {
        if ( hooks_.apply ) { hooks_.apply(takeStart_); }
        playIndex_ = 0;
        takeState_ = TakeState::Playing;
        if ( playWithRecording_ ) {
            StartRecording(0);
            recordForTake_ = true;
        }
        status_ = "テイクを再生中";
    }
    if ( takeState_ == TakeState::Playing ) {
        if ( playIndex_ < takeFrames_.size() ) {
            input->OverrideKeysForTake(takeFrames_[playIndex_++]);
        } else {
            StopTakePlay();
        }
    }

    UpdateFixedStep();
    PollMakeGif();
}

// =====================================================================
//  写す（画面に出す直前）
// =====================================================================
void ScreenCapture::BeforePresent(ID3D12Resource* backBuffer){
    if ( !shotRequested_ && !recording_ ) { return; }

    ID3D12Resource* resource = nullptr;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    if ( captureMode_ == 1 ) {
        resource = backBuffer;                    // ImGui ごと（エディタの窓も入る）
        state = D3D12_RESOURCE_STATE_PRESENT;
    } else if ( source_ ) {
        resource = source_->GetResource().Get();  // ImGui を重ねる前の最終画像
    }
    if ( !resource ) {
        shotRequested_ = false;
        status_ = "写す画像がまだない";
        return;
    }

    DirectX::ScratchImage image;
    const HRESULT result = DirectX::CaptureTexture(DirectXCommon::GetInstance()->GetCommandQueue(), resource, false, image, state, state);
    if ( FAILED(result) || !image.GetImage(0, 0, 0) ) {
        shotRequested_ = false;
        status_ = "写し取りに失敗した";
        return;
    }
    // 最終画像は _SRGB の形式。中身はそのまま（sRGB で書かれた色）を PNG にしたいので、形式の名前だけ UNORM にする
    if ( DirectX::IsSRGB(image.GetMetadata().format) ) { image.OverrideFormat(DirectX::MakeLinear(image.GetMetadata().format)); }

    auto push = [this](DirectX::ScratchImage&& scratch, const std::filesystem::path& path){
        auto job = std::make_unique<Job>();
        job->image = std::move(scratch);
        job->path = path;
        job->resize = resizeTo1080_;
        std::unique_lock<std::mutex> lock(mutex_);
        jobSpace_.wait(lock, [this]{ return jobs_.size() < kMaxPendingJobs || stopWorkers_; });
        jobs_.push_back(std::move(job));
        lock.unlock();
        jobReady_.notify_one();
    };

    if ( shotRequested_ ) {
        shotRequested_ = false;
        const std::filesystem::path dir = CaptureDir();
        const std::string stamp = TimeStamp();
        std::filesystem::path path = dir / ( "shot_" + stamp + ".png" );
        for ( int n = 2; std::filesystem::exists(path); ++n ) { path = dir / ( "shot_" + stamp + "_" + std::to_string(n) + ".png" ); }
        lastShot_ = Utf8FromPath(path);
        status_ = "撮った：" + lastShot_;
        if ( recording_ ) {
            DirectX::ScratchImage copy;
            copy.InitializeFromImage(*image.GetImage(0, 0, 0));
            push(std::move(copy), path);
        } else {
            push(std::move(image), path);
        }
    }
    if ( recording_ ) {
        char name[16];
        std::snprintf(name, sizeof(name), "%05d.png", recFrame_);
        push(std::move(image), recDir_ / name);
        ++recFrame_;
        if ( recLimitFrames_ > 0 && recFrame_ >= recLimitFrames_ ) { StopRecording(); }
    }
}

void ScreenCapture::WorkerLoop(){
    // WIC（PNG の書き出し・縮小）はスレッドごとに COM の初期化が要る
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    while ( true ) {
        std::unique_ptr<Job> job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            jobReady_.wait(lock, [this]{ return !jobs_.empty() || stopWorkers_; });
            if ( jobs_.empty() ) { break; } // 止める指示があり、もう残っていない
            job = std::move(jobs_.front());
            jobs_.pop_front();
            ++busyWorkers_;
        }
        jobSpace_.notify_one();

        const DirectX::Image* image = job->image.GetImage(0, 0, 0);
        DirectX::ScratchImage cropped, resized;
        if ( image && job->resize && !( image->width == 1920 && image->height == 1080 ) ) {
            // 真ん中を 16:9 に切り抜いてから 1920×1080 へ（ゆがませずに、スライドの 16:9 の枠にぴったり合わせる）
            size_t cropWidth = image->width, cropHeight = image->height;
            if ( image->width * 9 > image->height * 16 ) { cropWidth = image->height * 16 / 9; }  // 横長：左右を切る
            else                                         { cropHeight = image->width * 9 / 16; }  // 縦長：上下を切る
            if ( cropWidth != image->width || cropHeight != image->height ) {
                if ( SUCCEEDED(cropped.Initialize2D(image->format, cropWidth, cropHeight, 1, 1)) ) {
                    const DirectX::Rect rect((image->width - cropWidth) / 2, (image->height - cropHeight) / 2, cropWidth, cropHeight);
                    if ( SUCCEEDED(DirectX::CopyRectangle(*image, rect, *cropped.GetImage(0, 0, 0), DirectX::TEX_FILTER_DEFAULT, 0, 0)) ) {
                        image = cropped.GetImage(0, 0, 0);
                    }
                }
            }
            if ( SUCCEEDED(DirectX::Resize(*image, 1920, 1080, DirectX::TEX_FILTER_DEFAULT, resized)) ) { image = resized.GetImage(0, 0, 0); }
        }
        bool ok = false;
        if ( image ) {
            // 24bit（アルファなし）で保存：最終画像のアルファが 1 でない所が透けないように
            ok = SUCCEEDED(DirectX::SaveToWICFile(*image, DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG),
                job->path.c_str(), &GUID_WICPixelFormat24bppBGR));
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            --busyWorkers_;
            if ( !ok ) { ++failedWrites_; }
        }
        jobSpace_.notify_one();
    }
    if ( SUCCEEDED(comResult) ) { CoUninitialize(); }
}

// =====================================================================
//  録画
// =====================================================================
void ScreenCapture::StartRecording(int limitFrames){
    if ( recording_ ) { return; }
    recDir_ = CaptureDir() / ( "rec_" + TimeStamp() );
    for ( int n = 2; std::filesystem::exists(recDir_); ++n ) { recDir_ = CaptureDir() / ( "rec_" + TimeStamp() + "_" + std::to_string(n) ); }
    std::error_code error;
    std::filesystem::create_directories(recDir_, error);
    recFrame_ = 0;
    recLimitFrames_ = limitFrames;
    recording_ = true;
    recordForTake_ = false;
    status_ = "録画中：" + Utf8FromPath(recDir_);
    UpdateFixedStep(); // 次のフレームから 1/60 秒で進める
}

void ScreenCapture::StopRecording(){
    if ( !recording_ ) { return; }
    recording_ = false;
    recordForTake_ = false;
    lastRecDir_ = recDir_;
    status_ = std::to_string(recFrame_) + " 枚録った：" + Utf8FromPath(recDir_);
    gifStatus_.clear();
    UpdateFixedStep();
}

// =====================================================================
//  GIF・mp4 にまとめる
// =====================================================================
std::string ScreenCapture::FfmpegCommand(bool mp4) const{
    if ( lastRecDir_.empty() ) { return ""; }
    const std::string input = Utf8FromPath(lastRecDir_ / "%05d.png");
    const std::string output = Utf8FromPath(lastRecDir_) + ( mp4 ? ".mp4" : ".gif" );
    if ( mp4 ) {
        return "ffmpeg -y -framerate 60 -i \"" + input + "\" -c:v libx264 -pix_fmt yuv420p -crf 18 \"" + output + "\"";
    }
    const int width = gifWidthIndex_ == 0 ? 960 : 640;
    const int fps = gifFpsIndex_ == 0 ? 30 : 15;
    return "ffmpeg -y -framerate 60 -i \"" + input + "\" -vf \"fps=" + std::to_string(fps) + ",scale=" + std::to_string(width)
        + ":-1:flags=lanczos,split[a][b];[a]palettegen[p];[b][p]paletteuse\" \"" + output + "\"";
}

// tools/capture/make_gif.py を動かす（ffmpeg が PATH に無くても、Python の imageio-ffmpeg に入っている ffmpeg を使う）
void ScreenCapture::StartMakeGif(){
    if ( gifProcess_ || lastRecDir_.empty() ) { return; }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if ( !jobs_.empty() || busyWorkers_ > 0 ) { gifStatus_ = "まだ保存中。終わってから押す"; return; }
    }
    std::error_code error;
    const std::filesystem::path script = std::filesystem::absolute("../tools/capture/make_gif.py", error);
    if ( !std::filesystem::exists(script) ) { gifStatus_ = "tools/capture/make_gif.py が見つからない"; return; }
    const int width = gifWidthIndex_ == 0 ? 960 : 640;
    const int fps = gifFpsIndex_ == 0 ? 30 : 15;
    std::wstring command = L"python " + Quote(script) + L" " + Quote(std::filesystem::absolute(lastRecDir_, error))
        + L" --width " + std::to_wstring(width) + L" --fps " + std::to_wstring(fps);
    STARTUPINFOW startup {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process {};
    if ( !CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) ) {
        gifStatus_ = "python を起動できない（ffmpeg のコマンドをコピーして使う）";
        return;
    }
    CloseHandle(process.hThread);
    gifProcess_ = process.hProcess;
    gifStatus_ = "GIF・mp4 を作っている…";
}

void ScreenCapture::PollMakeGif(){
    if ( !gifProcess_ ) { return; }
    HANDLE process = static_cast< HANDLE >( gifProcess_ );
    if ( WaitForSingleObject(process, 0) != WAIT_OBJECT_0 ) { return; }
    DWORD code = 1;
    GetExitCodeProcess(process, &code);
    CloseHandle(process);
    gifProcess_ = nullptr;
    gifStatus_ = code == 0 ? "できた：" + Utf8FromPath(lastRecDir_) + ".gif / .mp4" : "作れなかった（終了コード " + std::to_string(code) + "）";
}

// =====================================================================
//  テイク（キー入力の記録と再生）
// =====================================================================
void ScreenCapture::StartTakeRecord(){
    if ( takeState_ != TakeState::Idle ) { return; }
    takeState_ = TakeState::RecordPending; // 次のフレームの頭で、始める時の状態を作ってから記録を始める
    UpdateFixedStep();
}

void ScreenCapture::StopTakeRecord(){
    if ( takeState_ != TakeState::Recording && takeState_ != TakeState::RecordPending ) { return; }
    takeState_ = TakeState::Idle;
    status_ = "テイク：" + std::to_string(takeFrames_.size()) + " フレーム記録した（名前を付けて保存できる）";
    UpdateFixedStep();
}

void ScreenCapture::StartTakePlay(bool withRecording){
    if ( takeState_ != TakeState::Idle || takeFrames_.empty() ) { return; }
    if ( !takeScene_.empty() && !hooks_.scene.empty() && takeScene_ != hooks_.scene ) {
        status_ = "テイクは「" + takeScene_ + "」で撮ったもの。今のシーンでは再生できない";
        return;
    }
    if ( hooks_.scene.empty() && !takeScene_.empty() ) {
        status_ = "このシーンはテイクを再生できない（始める時の状態へ戻せない）";
        return;
    }
    playWithRecording_ = withRecording;
    takeState_ = TakeState::PlayPending;
    UpdateFixedStep();
}

void ScreenCapture::StopTakePlay(){
    if ( takeState_ != TakeState::Playing && takeState_ != TakeState::PlayPending ) { return; }
    takeState_ = TakeState::Idle;
    if ( recordForTake_ ) { StopRecording(); }
    else { status_ = "テイクの再生が終わった"; }
    UpdateFixedStep();
}

bool ScreenCapture::SaveTake(const std::string& name){
    if ( name.empty() || takeFrames_.empty() ) { return false; }
    const std::filesystem::path dir = CaptureDir() / "takes";
    std::error_code error;
    std::filesystem::create_directories(dir, error);
    // 同じキーが続く所はまとめる：[[続くフレーム数, [キー...]], ...]
    nlohmann::json frames = nlohmann::json::array();
    for ( size_t i = 0; i < takeFrames_.size(); ) {
        size_t run = 1;
        while ( i + run < takeFrames_.size() && takeFrames_[i + run] == takeFrames_[i] ) { ++run; }
        frames.push_back(nlohmann::json::array({ run, takeFrames_[i] }));
        i += run;
    }
    nlohmann::json root;
    root["version"] = 1;
    root["scene"] = takeScene_;
    root["start"] = takeStart_;
    root["frameCount"] = takeFrames_.size();
    root["frames"] = frames;
    std::ofstream file(dir / PathFromUtf8(name + ".json"));
    if ( !file ) { return false; }
    file << root.dump(1);
    takeListDirty_ = true;
    return true;
}

bool ScreenCapture::LoadTake(const std::string& name){
    std::ifstream file(CaptureDir() / "takes" / PathFromUtf8(name + ".json"));
    if ( !file ) { return false; }
    nlohmann::json root = nlohmann::json::parse(file, nullptr, false);
    if ( root.is_discarded() || !root.contains("frames") ) { return false; }
    takeFrames_.clear();
    for ( const auto& entry : root["frames"] ) {
        if ( !entry.is_array() || entry.size() != 2 ) { continue; }
        const size_t run = entry[0].get<size_t>();
        const std::vector<uint8_t> keys = entry[1].get<std::vector<uint8_t>>();
        for ( size_t i = 0; i < run; ++i ) { takeFrames_.push_back(keys); }
    }
    takeScene_ = root.value("scene", std::string());
    takeStart_ = root.contains("start") ? root["start"] : nlohmann::json::object();
    return true;
}

void ScreenCapture::RefreshTakeList(){
    takeListDirty_ = false;
    takeList_.clear();
    std::error_code error;
    const std::filesystem::path dir = CaptureDir() / "takes";
    if ( !std::filesystem::exists(dir, error) ) { return; }
    for ( const auto& entry : std::filesystem::directory_iterator(dir, error) ) {
        if ( entry.path().extension() == ".json" ) { takeList_.push_back(Utf8FromPath(entry.path().stem())); }
    }
    std::sort(takeList_.begin(), takeList_.end());
}

// =====================================================================
//  画角のプリセット
// =====================================================================
void ScreenCapture::LoadPresets(){
    presetsLoaded_ = true;
    presets_.clear();
    std::ifstream file(CaptureDir() / "presets.json");
    if ( !file ) { return; }
    nlohmann::json root = nlohmann::json::parse(file, nullptr, false);
    if ( root.is_discarded() || !root.is_array() ) { return; }
    for ( const auto& entry : root ) {
        Preset preset;
        preset.name = entry.value("name", std::string("preset"));
        for ( int i = 0; i < 3; ++i ) {
            if ( entry.contains("pos") ) { preset.pos[i] = entry["pos"][i].get<float>(); }
            if ( entry.contains("rot") ) { preset.rot[i] = entry["rot"][i].get<float>(); }
        }
        preset.fov = entry.value("fov", 0.78f);
        presets_.push_back(preset);
    }
}

void ScreenCapture::SavePresets(){
    nlohmann::json root = nlohmann::json::array();
    for ( const Preset& preset : presets_ ) {
        root.push_back({ { "name", preset.name },
                         { "pos", { preset.pos[0], preset.pos[1], preset.pos[2] } },
                         { "rot", { preset.rot[0], preset.rot[1], preset.rot[2] } },
                         { "fov", preset.fov } });
    }
    std::ofstream file(CaptureDir() / "presets.json");
    if ( file ) { file << root.dump(1); }
}

void ScreenCapture::ApplyPreset(const Preset& preset, Camera* camera) const{
    if ( !camera ) { return; }
    camera->SetTranslation({ preset.pos[0], preset.pos[1], preset.pos[2] });
    camera->SetRotation({ preset.rot[0], preset.rot[1], preset.rot[2] });
    camera->SetFovY(preset.fov);
    camera->Update();
}

bool ScreenCapture::ApplyCameraLock(Camera* camera){
    if ( !cameraLocked_ || !camera ) { return false; }
    ApplyPreset(lockedPreset_, camera);
    return true;
}

// =====================================================================
//  UI
// =====================================================================
void ScreenCapture::DrawOverlay(){
#ifdef USE_IMGUI
    if ( !ImGui::GetCurrentContext() ) { return; }
    ImDrawList* draw = ImGui::GetForegroundDrawList(ImGui::GetMainViewport()); // 今の窓ではなく、ゲームの窓の一番手前
    const ImVec2 size = ImGui::GetMainViewport()->Size;
    const ImVec2 origin = ImGui::GetMainViewport()->Pos;
    float y = origin.y + 40.0f;
    auto badge = [&](ImU32 color, const std::string& text){
        const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
        const float x = origin.x + ( size.x - textSize.x ) * 0.5f; // 上の真ん中（左右のパネルに重ならない）
        draw->AddRectFilled({ x - 8.0f, y - 4.0f }, { x + textSize.x + 28.0f, y + textSize.y + 4.0f }, IM_COL32(0, 0, 0, 150), 6.0f);
        draw->AddCircleFilled({ x + 6.0f, y + textSize.y * 0.5f }, 6.0f, color);
        draw->AddText({ x + 18.0f, y }, IM_COL32(255, 255, 255, 255), text.c_str());
        y += textSize.y + 14.0f;
    };
    // 「ImGui ごと」を写している間は、印が録画に入ってしまうので出さない
    if ( recording_ && captureMode_ == 0 ) {
        char text[64];
        std::snprintf(text, sizeof(text), "REC  %.2f 秒（%d 枚）", recFrame_ / static_cast< float >( kFramesPerSecond ), recFrame_);
        badge(IM_COL32(235, 40, 40, 255), text);
    }
    if ( takeState_ == TakeState::Recording ) { badge(IM_COL32(255, 160, 30, 255), "テイク記録中  " + std::to_string(takeFrames_.size())); }
    if ( takeState_ == TakeState::Playing ) {
        badge(IM_COL32(60, 200, 90, 255), "テイク再生中  " + std::to_string(playIndex_) + " / " + std::to_string(takeFrames_.size()));
    }
#endif
}

void ScreenCapture::DrawPanel(const char* windowTitle){
#ifdef USE_IMGUI
    if ( !ImGui::Begin(windowTitle) ) { ImGui::End(); return; }
    if ( !presetsLoaded_ ) { LoadPresets(); }
    if ( takeListDirty_ ) { RefreshTakeList(); }

    int pending = 0, failed = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending = static_cast< int >( jobs_.size() ) + busyWorkers_;
        failed = failedWrites_;
    }

    // ---- 撮る ----
    ImGui::SeparatorText("撮る");
    ImGui::TextUnformatted("写すもの");
    ImGui::SameLine(); ImGui::RadioButton("ゲーム画面だけ", &captureMode_, 0);
    ImGui::SameLine(); ImGui::RadioButton("ImGui ごと（エディタの画面）", &captureMode_, 1);
    ImGui::Checkbox("1920×1080 にそろえる", &resizeTo1080_);
    if ( ImGui::IsItemHovered() ) { ImGui::SetTooltip("ウィンドウの大きさに関係なく、スライドの 16:9 の枠に合う大きさで保存する\n（16:9 でない時は、真ん中を 16:9 に切り抜いてから縮める）"); }
    if ( ImGui::Button("撮る (F12)") ) { RequestShot(); }
    ImGui::SameLine();
    if ( ImGui::Button("保存先を開く") ) {
        std::error_code error;
        const std::filesystem::path dir = std::filesystem::absolute(CaptureDir(), error);
        ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    if ( !lastShot_.empty() ) { ImGui::TextDisabled("最後に撮った：%s", lastShot_.c_str()); }

    // ---- 録る ----
    ImGui::SeparatorText("録る（連番 PNG）");
    if ( !recording_ ) {
        if ( ImGui::Button("録画開始 (F9)") ) { StartRecording(useLimit_ ? limitSeconds_ * kFramesPerSecond : 0); }
    } else {
        if ( ImGui::Button("停止 (F9)") ) { StopRecording(); }
    }
    ImGui::SameLine();
    ImGui::Checkbox("秒数で止める", &useLimit_);
    if ( useLimit_ ) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::SliderInt("秒", &limitSeconds_, 1, 10);
    }
    if ( ImGui::Checkbox("固定コマ送り（1フレーム＝1/60 秒）", &fixedStep_) ) { UpdateFixedStep(); }
    if ( ImGui::IsItemHovered() ) { ImGui::SetTooltip("録画中は保存で1フレームが遅くなる。ON なら、それでもゲームは 1/60 秒ずつ進むので、できた動画はなめらか"); }
    if ( recording_ ) { ImGui::Text("録画中：%d 枚（%.2f 秒）", recFrame_, recFrame_ / static_cast< float >( kFramesPerSecond )); }
    if ( pending > 0 ) { ImGui::TextColored({ 1.0f, 0.8f, 0.3f, 1.0f }, "保存待ち %d 枚", pending); }
    if ( failed > 0 ) { ImGui::TextColored({ 1.0f, 0.4f, 0.4f, 1.0f }, "保存に失敗した %d 枚", failed); }

    // ---- まとめる ----
    ImGui::SeparatorText("GIF・mp4 にまとめる");
    if ( lastRecDir_.empty() ) {
        ImGui::TextDisabled("録画すると、ここでまとめられる");
    } else {
        ImGui::TextDisabled("%s", Utf8FromPath(lastRecDir_).c_str());
        ImGui::TextUnformatted("GIF の幅"); ImGui::SameLine();
        ImGui::RadioButton("960##gw", &gifWidthIndex_, 0); ImGui::SameLine();
        ImGui::RadioButton("640##gw", &gifWidthIndex_, 1); ImGui::SameLine();
        ImGui::TextUnformatted("  fps"); ImGui::SameLine();
        ImGui::RadioButton("30##gf", &gifFpsIndex_, 0); ImGui::SameLine();
        ImGui::RadioButton("15##gf", &gifFpsIndex_, 1);
        ImGui::BeginDisabled(gifProcess_ != nullptr || recording_);
        if ( ImGui::Button("GIF・mp4 を作る") ) { StartMakeGif(); }
        ImGui::EndDisabled();
        if ( ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) ) { ImGui::SetTooltip("tools/capture/make_gif.py を動かす（ffmpeg が無くても、Python の imageio-ffmpeg の ffmpeg を使う）"); }
        ImGui::SameLine();
        if ( ImGui::Button("ffmpeg のコマンドをコピー") ) {
            const std::string text = "cd /d \"" + Utf8FromPath(std::filesystem::current_path()) + "\"\n" + FfmpegCommand(false) + "\n" + FfmpegCommand(true) + "\n";
            ImGui::SetClipboardText(text.c_str());
            gifStatus_ = "コピーした（GIF と mp4 の2行）";
        }
        if ( !gifStatus_.empty() ) { ImGui::TextWrapped("%s", gifStatus_.c_str()); }
    }

    // ---- テイク ----
    ImGui::SeparatorText("テイク（入力の記録と再生）");
    if ( hooks_.scene.empty() ) {
        ImGui::TextDisabled("このシーンは始める時の状態へ戻せない（キーだけ記録・再生する）");
    } else {
        ImGui::TextDisabled("記録・再生の始めに、敵・コインなどを置き直し、記録を始めた場所から始める");
    }
    const bool takeRecording = takeState_ == TakeState::Recording || takeState_ == TakeState::RecordPending;
    const bool takePlaying = takeState_ == TakeState::Playing || takeState_ == TakeState::PlayPending;
    ImGui::BeginDisabled(takePlaying);
    if ( !takeRecording ) {
        if ( ImGui::Button("記録開始") ) { StartTakeRecord(); }
    } else {
        if ( ImGui::Button("記録停止") ) { StopTakeRecord(); }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if ( !takePlaying ) {
        ImGui::BeginDisabled(takeRecording || takeFrames_.empty());
        if ( ImGui::Button("再生") ) { StartTakePlay(false); }
        ImGui::SameLine();
        if ( ImGui::Button("再生しながら録画") ) { StartTakePlay(true); }
        ImGui::EndDisabled();
    } else {
        if ( ImGui::Button("再生をやめる") ) { StopTakePlay(); }
    }
    ImGui::Text("今のテイク：%d フレーム（%.2f 秒）%s%s", static_cast< int >( takeFrames_.size() ),
        takeFrames_.size() / static_cast< float >( kFramesPerSecond ), takeScene_.empty() ? "" : "  シーン ", takeScene_.c_str());
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputText("名前##take", takeName_, sizeof(takeName_));
    ImGui::SameLine();
    ImGui::BeginDisabled(takeFrames_.empty() || takeRecording);
    if ( ImGui::Button("保存##take") ) {
        status_ = SaveTake(takeName_) ? std::string("テイクを保存した：capture/takes/") + takeName_ + ".json" : "テイクを保存できなかった";
    }
    ImGui::EndDisabled();
    if ( !takeList_.empty() ) {
        ImGui::BeginDisabled(takeRecording || takePlaying);
        if ( ImGui::BeginListBox("##takes", { -1.0f, 90.0f }) ) {
            for ( int i = 0; i < static_cast< int >( takeList_.size() ); ++i ) {
                if ( ImGui::Selectable(takeList_[i].c_str(), selectedTake_ == i) ) {
                    selectedTake_ = i;
                    if ( LoadTake(takeList_[i]) ) {
                        std::snprintf(takeName_, sizeof(takeName_), "%s", takeList_[i].c_str());
                        status_ = "テイクを読み込んだ：" + takeList_[i];
                    } else {
                        status_ = "テイクを読み込めなかった：" + takeList_[i];
                    }
                }
            }
            ImGui::EndListBox();
        }
        ImGui::EndDisabled();
    }
    if ( ImGui::SmallButton("一覧を読み直す") ) { takeListDirty_ = true; }

    // ---- プリセット ----
    ImGui::SeparatorText("画角のプリセット");
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputText("名前##preset", presetName_, sizeof(presetName_));
    ImGui::SameLine();
    ImGui::BeginDisabled(!camera_);
    if ( ImGui::Button("今のカメラを登録") && camera_ ) {
        Preset preset;
        preset.name = presetName_;
        const Vector3 pos = camera_->GetWorldPosition();
        const Vector3 rot = camera_->GetRotation();
        preset.pos[0] = pos.x; preset.pos[1] = pos.y; preset.pos[2] = pos.z;
        preset.rot[0] = rot.x; preset.rot[1] = rot.y; preset.rot[2] = rot.z;
        preset.fov = camera_->GetFovY();
        auto same = std::find_if(presets_.begin(), presets_.end(), [&](const Preset& p){ return p.name == preset.name; });
        if ( same != presets_.end() ) { *same = preset; } else { presets_.push_back(preset); }
        SavePresets();
        status_ = "プリセットを登録した：" + preset.name;
    }
    ImGui::EndDisabled();
    if ( !presets_.empty() ) {
        if ( ImGui::BeginListBox("##presets", { -1.0f, 90.0f }) ) {
            for ( int i = 0; i < static_cast< int >( presets_.size() ); ++i ) {
                const bool locked = cameraLocked_ && lockedPreset_.name == presets_[i].name;
                std::string label = presets_[i].name + ( locked ? "  （固定中）" : "" );
                if ( ImGui::Selectable(label.c_str(), selectedPreset_ == i) ) { selectedPreset_ = i; }
            }
            ImGui::EndListBox();
        }
        const bool valid = selectedPreset_ >= 0 && selectedPreset_ < static_cast< int >( presets_.size() );
        ImGui::BeginDisabled(!valid || !camera_);
        if ( ImGui::Button("この画角にする") && valid ) {
            lockedPreset_ = presets_[selectedPreset_];
            cameraLocked_ = true;
            ApplyPreset(lockedPreset_, camera_);
        }
        ImGui::SameLine();
        if ( ImGui::Button("削除##preset") && valid ) {
            presets_.erase(presets_.begin() + selectedPreset_);
            selectedPreset_ = -1;
            SavePresets();
        }
        ImGui::EndDisabled();
    }
    if ( cameraLocked_ ) {
        ImGui::TextColored({ 0.5f, 0.9f, 1.0f, 1.0f }, "画角を「%s」に固定中（Play 中の追従カメラも止める）", lockedPreset_.name.c_str());
        if ( ImGui::Button("固定をやめる") ) { cameraLocked_ = false; }
    }

    if ( !status_.empty() ) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", status_.c_str());
    }
    ImGui::End();
#endif
}
