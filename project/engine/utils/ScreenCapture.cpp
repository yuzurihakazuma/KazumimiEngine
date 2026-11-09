#include "ScreenCapture.h"

#include "externals/DirectXTex/DirectXTex.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/Input.h"
#include "engine/base/TimeManager.h"
#include "engine/camera/Camera.h"
#include "engine/graphics/RenderTexture.h"
#include "engine/utils/EditorManager.h"
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
#include <cstring>
#include <ctime>
#include <fstream>

#pragma comment(lib, "shell32.lib")

namespace {
    constexpr int    kWorkerCount = 3;     // PNG の書き出しを並べるスレッドの数
    constexpr size_t kMaxPendingJobs = 8;  // 保存待ちがこれを超えたら待つ（1枚 8MB ほど）
    constexpr int    kFramesPerSecond = 60;
    constexpr int    kReplayFps = 30;      // 直前を保存：2フレームに1枚ためる
    constexpr float  kToastSeconds = 6.0f;
    constexpr size_t kRecentMax = 10;

    // UTF-8 の文字列 ⇔ パス（日本語の名前でも化けないように）
    std::filesystem::path PathFromUtf8(const std::string& text){
        return std::filesystem::path(std::u8string(text.begin(), text.end()));
    }
    std::string Utf8FromPath(const std::filesystem::path& path){
        const std::u8string text = path.generic_u8string();
        return std::string(text.begin(), text.end());
    }
    std::wstring Quote(const std::filesystem::path& path){ return L"\"" + path.wstring() + L"\""; }

    void Transition(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after){
        D3D12_RESOURCE_BARRIER barrier {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = resource;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;
        commandList->ResourceBarrier(1, &barrier);
    }
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
#ifndef USE_IMGUI
    replayOn_ = false; // エディタの無いビルドでは、直前をためない（メモリと時間を使わない）
#endif
    for ( int i = 0; i < kWorkerCount; ++i ) { workers_.emplace_back([this]{ WorkerLoop(); }); }
    DirectXCommon::GetInstance()->SetFrameEndHook([this](ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer){
        OnFrameEnd(commandList, backBuffer);
    });
}

void ScreenCapture::Finalize(){
    if ( !initialized_ ) { return; }
    DirectXCommon::GetInstance()->SetFrameEndHook(nullptr);
    recording_ = false;
    takeState_ = TakeState::Idle;
    Time::GetInstance()->SetFixedStep(false);
    for ( ReplaySave& save : replaySaves_ ) { if ( save.written.valid() ) { save.written.wait(); } }
    replaySaves_.clear();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopWorkers_ = true;
    }
    jobReady_.notify_all();
    for ( auto& worker : workers_ ) { if ( worker.joinable() ) { worker.join(); } }
    workers_.clear();
    for ( Process& process : processes_ ) { if ( process.handle ) { CloseHandle(static_cast< HANDLE >( process.handle )); } }
    processes_.clear();
    replayFrames_.clear();
    readback_.Reset();
    hooks_ = {};
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

void ScreenCapture::SetEditorHidden(bool hidden){
    EditorManager::GetInstance()->SetActive(!hidden);
}

// =====================================================================
//  毎フレーム：取り出し・キー・テイク・このフレームに写すか
// =====================================================================
void ScreenCapture::OnInputUpdated(){
    if ( !initialized_ ) { return; }
    ProcessCopied(); // 前のフレームに積んだコピー（PostDraw で GPU を待ったので、もう読める）
    Input* input = Input::GetInstance();
    const float dt = Time::GetInstance()->GetUnscaledDeltaTime();

    // 撮影のキー（遷移の演出中・文字入力中でも効く）
    if ( input->TriggerkeyRaw(DIK_F12) ) { RequestShot(); }
    if ( input->TriggerkeyRaw(DIK_F9) ) { // F11 はフルスクリーンの切り替え（WindowProc）なので F9
        if ( recording_ ) { StopRecording(); }
        else if ( countdown_ >= 0.0f ) { countdown_ = -1.0f; status_ = "録画をやめた"; }
        else { BeginCountdownOrRecord(); }
    }
    if ( input->TriggerkeyRaw(DIK_F5) ) { SaveReplay(); }

    // 3・2・1
    if ( countdown_ >= 0.0f ) {
        countdown_ -= dt;
        if ( countdown_ < 0.0f ) { StartRecording(useLimit_ ? limitSeconds_ * kFramesPerSecond : 0); }
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
    PlanGrab();
    PollProcesses();
}

void ScreenCapture::BeginCountdownOrRecord(){
    if ( recording_ ) { return; }
    if ( countdownOn_ ) { countdown_ = 3.0f; status_ = "3・2・1 で録画を始める"; }
    else { StartRecording(useLimit_ ? limitSeconds_ * kFramesPerSecond : 0); }
}

// このフレームに何を写すか・どこで写すかを決める（エディタの表示はこの後の EditorManager::Update で決まるので、ここで隠せば今のフレームから効く）
void ScreenCapture::PlanGrab(){
    frameGrab_ = {};
    copied_ = false;
    EditorManager* editor = EditorManager::GetInstance();
    if ( shotRequested_ ) {
        shotRequested_ = false;
        if ( captureMode_ == kGameHud && editor->IsActive() ) { SetEditorHidden(true); shotHidEditor_ = true; } // 写したら戻す
        frameGrab_.shot = true;
    }
    if ( recording_ ) {
        frameGrab_.record = true;
        frameGrab_.recIndex = recFrame_++;
        if ( recLimitFrames_ > 0 && recFrame_ >= recLimitFrames_ ) { StopRecording(); } // この1枚は写して取り出す
    }
    if ( replayOn_ && ( replayTick_++ % ( kFramesPerSecond / kReplayFps ) ) == 0 ) { frameGrab_.replay = true; }
    // 写すもの：ゲーム＋HUD はエディタが出ている間は作れない（ゲームが Game View に描かれる）→ ゲーム画面だけ
    frameSource_ = captureMode_;
    if ( frameSource_ == kGameHud && editor->IsActive() ) { frameSource_ = kGame; }
}

void ScreenCapture::OnFinalImage(ID3D12GraphicsCommandList* commandList, RenderTexture* finalTarget){
    if ( frameSource_ != kGame || !frameGrab_.Any() || copied_ || !finalTarget ) { return; }
    RecordCopy(commandList, finalTarget->GetResource().Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void ScreenCapture::OnBeforeImGui(ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer){
    if ( frameSource_ != kGameHud || !frameGrab_.Any() || copied_ || !backBuffer ) { return; }
    RecordCopy(commandList, backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void ScreenCapture::OnFrameEnd(ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer){
    if ( frameSource_ != kScreen || !frameGrab_.Any() || copied_ || !backBuffer ) { return; }
    RecordCopy(commandList, backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

// 写すものを読み出し用のバッファへコピーするコマンドを積む（大きさが変わったらバッファを作り直す）
void ScreenCapture::RecordCopy(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource, D3D12_RESOURCE_STATES state){
    const D3D12_RESOURCE_DESC desc = resource->GetDesc();
    if ( !readback_ || desc.Width != grabWidth_ || desc.Height != grabHeight_ || desc.Format != grabFormat_ ) {
        ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();
        device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint_, nullptr, nullptr, &readbackBytes_);
        D3D12_HEAP_PROPERTIES heap {};
        heap.Type = D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC buffer {};
        buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        buffer.Width = readbackBytes_;
        buffer.Height = 1;
        buffer.DepthOrArraySize = 1;
        buffer.MipLevels = 1;
        buffer.SampleDesc.Count = 1;
        buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        readback_.Reset();
        if ( FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback_))) ) {
            status_ = "読み出し用のバッファを作れなかった";
            return;
        }
        readback_->SetName(L"ScreenCapture readback");
        grabWidth_ = ( uint32_t ) desc.Width;
        grabHeight_ = desc.Height;
        grabFormat_ = desc.Format;
    }
    Transition(commandList, resource, state, D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION destination {};
    destination.pResource = readback_.Get();
    destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    destination.PlacedFootprint = footprint_;
    D3D12_TEXTURE_COPY_LOCATION source {};
    source.pResource = resource;
    source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    source.SubresourceIndex = 0;
    commandList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    Transition(commandList, resource, D3D12_RESOURCE_STATE_COPY_SOURCE, state);
    copied_ = true;
    copiedGrab_ = frameGrab_;
}

// 前のフレームのコピーを取り出して、スクショ・録画の PNG・直前のためへ
void ScreenCapture::ProcessCopied(){
    if ( !copied_ || !readback_ ) { copied_ = false; return; }
    copied_ = false;
    void* mapped = nullptr;
    const D3D12_RANGE readRange { 0, ( SIZE_T ) readbackBytes_ };
    if ( FAILED(readback_->Map(0, &readRange, &mapped)) ) { return; }
    const uint8_t* pixels = static_cast< const uint8_t* >( mapped ) + footprint_.Offset;
    const uint32_t pitch = footprint_.Footprint.RowPitch;

    if ( copiedGrab_.shot ) {
        const std::filesystem::path dir = CaptureDir();
        const std::string stamp = TimeStamp();
        std::filesystem::path path = dir / ( "shot_" + stamp + ".png" );
        for ( int n = 2; std::filesystem::exists(path); ++n ) { path = dir / ( "shot_" + stamp + "_" + std::to_string(n) + ".png" ); }
        PushJob(pixels, grabWidth_, grabHeight_, pitch, path);
        if ( copyToClipboard_ ) { CopyToClipboard(pixels, grabWidth_, grabHeight_, pitch); }
        AddRecent(path);
        Notify(copyToClipboard_ ? "撮った（クリップボードにも入れた）" : "撮った", path);
        if ( shotHidEditor_ && !recordHidEditor_ ) { SetEditorHidden(false); }
        shotHidEditor_ = false;
    }
    if ( copiedGrab_.record ) {
        char name[16];
        std::snprintf(name, sizeof(name), "%05d.png", copiedGrab_.recIndex);
        PushJob(pixels, grabWidth_, grabHeight_, pitch, recDir_ / name);
    }
    if ( copiedGrab_.replay ) { PushReplayFrame(pixels, grabWidth_, grabHeight_, pitch); }

    const D3D12_RANGE written { 0, 0 };
    readback_->Unmap(0, &written);
}

void ScreenCapture::PushJob(const uint8_t* source, uint32_t width, uint32_t height, uint32_t pitch, const std::filesystem::path& path){
    auto job = std::make_unique<Job>();
    if ( FAILED(job->image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1)) ) { return; }
    const DirectX::Image* image = job->image.GetImage(0, 0, 0);
    for ( uint32_t y = 0; y < height; ++y ) {
        std::memcpy(image->pixels + y * image->rowPitch, source + ( size_t ) y * pitch, ( size_t ) width * 4);
    }
    job->path = path;
    job->resize = resizeTo1080_;
    std::unique_lock<std::mutex> lock(mutex_);
    jobSpace_.wait(lock, [this]{ return jobs_.size() < kMaxPendingJobs || stopWorkers_; });
    jobs_.push_back(std::move(job));
    lock.unlock();
    jobReady_.notify_one();
}

// 直前を保存：真ん中を 16:9 に切り抜いて、640 / 960 の幅へ縮めて（4点の平均）、ためる
void ScreenCapture::PushReplayFrame(const uint8_t* source, uint32_t width, uint32_t height, uint32_t pitch){
    const uint32_t outW = replayWidthIndex_ == 0 ? 640 : 960;
    const uint32_t outH = outW * 9 / 16;
    if ( outW != replayW_ || outH != replayH_ ) { replayFrames_.clear(); replayW_ = outW; replayH_ = outH; }
    uint32_t cropW = width, cropH = height;
    if ( ( uint64_t ) width * 9 > ( uint64_t ) height * 16 ) { cropW = height * 16 / 9; } else { cropH = width * 9 / 16; }
    const uint32_t x0 = ( width - cropW ) / 2, y0 = ( height - cropH ) / 2;
    const size_t cap = ( size_t ) replaySeconds_ * kReplayFps;
    std::vector<uint8_t> frame;
    if ( replayFrames_.size() >= cap ) { frame = std::move(replayFrames_.front()); replayFrames_.pop_front(); } // 一番古い入れ物を使い回す
    frame.resize(( size_t ) outW * outH * 3);
    for ( uint32_t oy = 0; oy < outH; ++oy ) {
        const uint32_t syA = y0 + ( uint32_t ) ( ( ( uint64_t ) oy * 4 + 1 ) * cropH / ( outH * 4 ) );
        const uint32_t syB = y0 + ( uint32_t ) ( ( ( uint64_t ) oy * 4 + 3 ) * cropH / ( outH * 4 ) );
        const uint8_t* rowA = source + ( size_t ) syA * pitch;
        const uint8_t* rowB = source + ( size_t ) syB * pitch;
        uint8_t* out = frame.data() + ( size_t ) oy * outW * 3;
        for ( uint32_t ox = 0; ox < outW; ++ox ) {
            const uint32_t sxA = x0 + ( uint32_t ) ( ( ( uint64_t ) ox * 4 + 1 ) * cropW / ( outW * 4 ) );
            const uint32_t sxB = x0 + ( uint32_t ) ( ( ( uint64_t ) ox * 4 + 3 ) * cropW / ( outW * 4 ) );
            const uint8_t* a = rowA + sxA * 4; const uint8_t* b = rowA + sxB * 4;
            const uint8_t* c = rowB + sxA * 4; const uint8_t* d = rowB + sxB * 4;
            out[ox * 3 + 0] = ( uint8_t ) ( ( a[0] + b[0] + c[0] + d[0] + 2 ) >> 2 );
            out[ox * 3 + 1] = ( uint8_t ) ( ( a[1] + b[1] + c[1] + d[1] + 2 ) >> 2 );
            out[ox * 3 + 2] = ( uint8_t ) ( ( a[2] + b[2] + c[2] + d[2] + 2 ) >> 2 );
        }
    }
    replayFrames_.push_back(std::move(frame));
    while ( replayFrames_.size() > cap ) { replayFrames_.pop_front(); }
}

// スクショをクリップボードへ（24bit の DIB。スライドや画像の編集ソフトへそのまま貼れる）
void ScreenCapture::CopyToClipboard(const uint8_t* source, uint32_t width, uint32_t height, uint32_t pitch){
    const size_t rowBytes = ( ( size_t ) width * 3 + 3 ) & ~( size_t ) 3;
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + rowBytes * height);
    if ( !memory ) { return; }
    auto* header = static_cast< BITMAPINFOHEADER* >( GlobalLock(memory) );
    if ( !header ) { GlobalFree(memory); return; }
    *header = {};
    header->biSize = sizeof(BITMAPINFOHEADER);
    header->biWidth = ( LONG ) width;
    header->biHeight = ( LONG ) height; // 下から上へ並べる
    header->biPlanes = 1;
    header->biBitCount = 24;
    header->biCompression = BI_RGB;
    header->biSizeImage = ( DWORD ) ( rowBytes * height );
    uint8_t* bits = reinterpret_cast< uint8_t* >( header + 1 );
    for ( uint32_t y = 0; y < height; ++y ) {
        const uint8_t* src = source + ( size_t ) ( height - 1 - y ) * pitch;
        uint8_t* dst = bits + ( size_t ) y * rowBytes;
        for ( uint32_t x = 0; x < width; ++x ) { dst[x * 3 + 0] = src[x * 4 + 2]; dst[x * 3 + 1] = src[x * 4 + 1]; dst[x * 3 + 2] = src[x * 4 + 0]; }
    }
    GlobalUnlock(memory);
    if ( OpenClipboard(nullptr) ) {
        EmptyClipboard();
        if ( !SetClipboardData(CF_DIB, memory) ) { GlobalFree(memory); }
        CloseClipboard();
    } else {
        GlobalFree(memory);
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
    countdown_ = -1.0f;
    recDir_ = CaptureDir() / ( "rec_" + TimeStamp() );
    for ( int n = 2; std::filesystem::exists(recDir_); ++n ) { recDir_ = CaptureDir() / ( "rec_" + TimeStamp() + "_" + std::to_string(n) ); }
    std::error_code error;
    std::filesystem::create_directories(recDir_, error);
    recFrame_ = 0;
    recLimitFrames_ = limitFrames;
    recording_ = true;
    recordForTake_ = false;
    // ゲーム＋HUD を録る間は、エディタを隠す（止めたら戻す）
    if ( captureMode_ == kGameHud && EditorManager::GetInstance()->IsActive() ) { SetEditorHidden(true); recordHidEditor_ = true; }
    status_ = "録画中：" + Utf8FromPath(recDir_);
    UpdateFixedStep(); // 次のフレームから 1/60 秒で進める
}

void ScreenCapture::StopRecording(){
    if ( !recording_ ) { return; }
    recording_ = false;
    recordForTake_ = false;
    lastRecDir_ = recDir_;
    if ( recordHidEditor_ ) { SetEditorHidden(false); recordHidEditor_ = false; }
    status_ = std::to_string(recFrame_) + " 枚録った：" + Utf8FromPath(recDir_);
    autoGifPending_ = autoGif_; // 書き出しが終わったら GIF・mp4 を作る（PollProcesses）
    if ( !autoGif_ ) { AddRecent(recDir_); Notify("録画した（" + std::to_string(recFrame_) + " 枚）", recDir_); }
    UpdateFixedStep();
}

// =====================================================================
//  直前を保存
// =====================================================================
void ScreenCapture::SaveReplay(){
    if ( !replayOn_ ) { Notify("直前を保存は OFF（パネルの設定で ON にできる）"); return; }
    if ( replayFrames_.size() < ( size_t ) kReplayFps / 2 ) { Notify("まだたまっていない"); return; }
    auto frames = std::make_shared<std::deque<std::vector<uint8_t>>>(std::move(replayFrames_));
    replayFrames_.clear();
    ReplaySave save;
    save.raw = CaptureDir() / ( "replay_" + TimeStamp() + ".rgb" );
    save.width = replayW_;
    save.height = replayH_;
    const std::filesystem::path raw = save.raw;
    save.written = std::async(std::launch::async, [frames, raw]{
        std::ofstream file(raw, std::ios::binary);
        if ( !file ) { return false; }
        for ( const auto& frame : *frames ) { file.write(reinterpret_cast< const char* >( frame.data() ), ( std::streamsize ) frame.size()); }
        return ( bool ) file;
    });
    const float seconds = frames->size() / ( float ) kReplayFps;
    replaySaves_.push_back(std::move(save));
    char text[64];
    std::snprintf(text, sizeof(text), "直前 %.1f 秒を GIF・mp4 にしている…", seconds);
    Notify(text);
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
bool ScreenCapture::LaunchMakeGif(const std::wstring& arguments, const std::filesystem::path& stem, const std::string& label){
    std::error_code error;
    const std::filesystem::path script = std::filesystem::absolute("../tools/capture/make_gif.py", error);
    if ( !std::filesystem::exists(script) ) { Notify("tools/capture/make_gif.py が見つからない"); return false; }
    std::wstring command = L"python " + Quote(script) + L" " + arguments;
    STARTUPINFOW startup {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process {};
    if ( !CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) ) {
        Notify("python を起動できない（パネルの「ffmpeg のコマンドをコピー」で作れる）");
        return false;
    }
    CloseHandle(process.hThread);
    processes_.push_back({ process.hProcess, stem, label });
    return true;
}

void ScreenCapture::PollProcesses(){
    std::error_code error;
    const int gifWidth = gifWidthIndex_ == 0 ? 960 : 640;
    const int gifFps = gifFpsIndex_ == 0 ? 30 : 15;
    // 止めた録画：PNG を書き切ったら GIF・mp4 を作る
    if ( autoGifPending_ && !copied_ ) {
        bool busy = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            busy = !jobs_.empty() || busyWorkers_ > 0;
        }
        if ( !busy ) {
            autoGifPending_ = false;
            const std::wstring arguments = Quote(std::filesystem::absolute(lastRecDir_, error)) + L" --width " + std::to_wstring(gifWidth) + L" --fps " + std::to_wstring(gifFps);
            if ( LaunchMakeGif(arguments, lastRecDir_, "録画") ) { Notify("録画を GIF・mp4 にしている…"); }
        }
    }
    // 直前を保存：書き出しが終わったら GIF・mp4 を作る
    for ( auto it = replaySaves_.begin(); it != replaySaves_.end(); ) {
        if ( it->written.wait_for(std::chrono::seconds(0)) != std::future_status::ready ) { ++it; continue; }
        if ( it->written.get() ) {
            const int fps = gifFpsIndex_ == 0 ? 30 : 15;
            const std::wstring arguments = Quote(std::filesystem::absolute(it->raw, error)) + L" --raw " + std::to_wstring(it->width) + L"x"
                + std::to_wstring(it->height) + L" --rate " + std::to_wstring(kReplayFps) + L" --width " + std::to_wstring(it->width) + L" --fps " + std::to_wstring(fps);
            std::filesystem::path stem = it->raw;
            stem.replace_extension();
            LaunchMakeGif(arguments, stem, "直前");
        } else {
            Notify("直前を書き出せなかった");
        }
        it = replaySaves_.erase(it);
    }
    // できあがり
    for ( auto it = processes_.begin(); it != processes_.end(); ) {
        HANDLE handle = static_cast< HANDLE >( it->handle );
        if ( WaitForSingleObject(handle, 0) != WAIT_OBJECT_0 ) { ++it; continue; }
        DWORD code = 1;
        GetExitCodeProcess(handle, &code);
        CloseHandle(handle);
        if ( code == 0 ) {
            std::filesystem::path gif = it->stem;
            gif += ".gif";
            std::filesystem::path mp4 = it->stem;
            mp4 += ".mp4";
            AddRecent(mp4);
            AddRecent(gif);
            Notify(it->label + "の GIF・mp4 ができた", gif);
        } else {
            Notify(it->label + "の GIF・mp4 を作れなかった（終了コード " + std::to_string(code) + "）");
        }
        it = processes_.erase(it);
    }
}

// =====================================================================
//  知らせ・最近の保存
// =====================================================================
void ScreenCapture::Notify(const std::string& text, const std::filesystem::path& file){
    toastText_ = text;
    toastFile_ = file;
    toastTime_ = kToastSeconds;
    status_ = text + ( file.empty() ? "" : "：" + Utf8FromPath(file) );
}

void ScreenCapture::AddRecent(const std::filesystem::path& file){
    recent_.erase(std::remove(recent_.begin(), recent_.end(), file), recent_.end());
    recent_.insert(recent_.begin(), file);
    if ( recent_.size() > kRecentMax ) { recent_.resize(kRecentMax); }
}

void ScreenCapture::OpenFile(const std::filesystem::path& file){
    std::error_code error;
    const std::filesystem::path path = std::filesystem::absolute(file, error);
    ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void ScreenCapture::RevealFile(const std::filesystem::path& file){
    std::error_code error;
    const std::filesystem::path path = std::filesystem::absolute(file, error);
    const std::wstring arguments = L"/select," + Quote(path);
    ShellExecuteW(nullptr, L"open", L"explorer.exe", arguments.c_str(), nullptr, SW_SHOWNORMAL);
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
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImDrawList* draw = ImGui::GetForegroundDrawList(ImGui::GetMainViewport()); // 今の窓ではなく、ゲームの窓の一番手前
    const ImVec2 size = viewport->Size;
    const ImVec2 origin = viewport->Pos;
    // 「画面そのまま」を録っている間は、印が録画に入るので何も出さない
    const bool quiet = recording_ && captureMode_ == kScreen;

    // 3・2・1（大きな数字。録画はこの後から）
    if ( countdown_ >= 0.0f ) {
        const int number = ( int ) std::ceil(countdown_);
        const float t = countdown_ - std::floor(countdown_);  // 1 → 0
        const float radius = 70.0f + 20.0f * t;
        const ImVec2 center { origin.x + size.x * 0.5f, origin.y + size.y * 0.45f };
        draw->AddCircleFilled(center, radius, IM_COL32(0, 0, 0, 140), 48);
        draw->AddCircle(center, radius, IM_COL32(235, 60, 50, 255), 48, 6.0f);
        const std::string text = std::to_string(( std::max )( number, 1 ));
        const float fontSize = 96.0f;
        const ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.c_str());
        draw->AddText(ImGui::GetFont(), fontSize, { center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f }, IM_COL32(255, 255, 255, 255), text.c_str());
    }

    float y = origin.y + 40.0f;
    auto badge = [&](ImU32 color, const std::string& text){
        const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
        const float x = origin.x + ( size.x - textSize.x ) * 0.5f; // 上の真ん中（左右のパネルに重ならない）
        draw->AddRectFilled({ x - 8.0f, y - 4.0f }, { x + textSize.x + 28.0f, y + textSize.y + 4.0f }, IM_COL32(0, 0, 0, 150), 6.0f);
        draw->AddCircleFilled({ x + 6.0f, y + textSize.y * 0.5f }, 6.0f, color);
        draw->AddText({ x + 18.0f, y }, IM_COL32(255, 255, 255, 255), text.c_str());
        y += textSize.y + 14.0f;
    };
    if ( !quiet ) {
        if ( recording_ ) {
            char text[80];
            std::snprintf(text, sizeof(text), "REC  %.2f 秒（%d 枚）  F9 で止める", recFrame_ / static_cast< float >( kFramesPerSecond ), recFrame_);
            badge(IM_COL32(235, 40, 40, 255), text);
        }
        if ( takeState_ == TakeState::Recording ) { badge(IM_COL32(255, 160, 30, 255), "テイク記録中  " + std::to_string(takeFrames_.size())); }
        if ( takeState_ == TakeState::Playing ) {
            badge(IM_COL32(60, 200, 90, 255), "テイク再生中  " + std::to_string(playIndex_) + " / " + std::to_string(takeFrames_.size()));
        }
    }

    // 保存の知らせ（右下。開く・フォルダ）
    if ( toastTime_ > 0.0f ) {
        toastTime_ -= ImGui::GetIO().DeltaTime;
        if ( !quiet ) {
            ImGui::SetNextWindowPos({ origin.x + size.x - 16.0f, origin.y + size.y - 16.0f }, ImGuiCond_Always, { 1.0f, 1.0f });
            ImGui::SetNextWindowBgAlpha(0.88f);
            const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
                | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDocking;
            if ( ImGui::Begin("##capture_toast", nullptr, flags) ) {
                ImGui::TextUnformatted(toastText_.c_str());
                if ( !toastFile_.empty() ) {
                    ImGui::TextDisabled("%s", Utf8FromPath(toastFile_.filename()).c_str());
                    if ( ImGui::SmallButton("開く##toast") ) { OpenFile(toastFile_); toastTime_ = 0.0f; }
                    ImGui::SameLine();
                    if ( ImGui::SmallButton("フォルダ##toast") ) { RevealFile(toastFile_); toastTime_ = 0.0f; }
                    ImGui::SameLine();
                }
                if ( ImGui::SmallButton("閉じる##toast") ) { toastTime_ = 0.0f; }
            }
            ImGui::End();
        }
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

    // ---- 大きなボタン3つ ----
    const float width = ( std::max )( 80.0f, ( ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2.0f ) / 3.0f );
    const ImVec2 buttonSize { width, 44.0f };
    if ( ImGui::Button("撮る\nF12", buttonSize) ) { RequestShot(); }
    ImGui::SameLine();
    if ( recording_ ) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4 { 0.75f, 0.15f, 0.15f, 1.0f });
        if ( ImGui::Button("止める\nF9", buttonSize) ) { StopRecording(); }
        ImGui::PopStyleColor();
    } else if ( countdown_ >= 0.0f ) {
        if ( ImGui::Button("やめる\nF9", buttonSize) ) { countdown_ = -1.0f; }
    } else {
        if ( ImGui::Button("録画\nF9", buttonSize) ) { BeginCountdownOrRecord(); }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!replayOn_);
    char replayLabel[48];
    std::snprintf(replayLabel, sizeof(replayLabel), "直前 %d 秒を保存\nF5", replaySeconds_);
    if ( ImGui::Button(replayLabel, buttonSize) ) { SaveReplay(); }
    ImGui::EndDisabled();

    // ---- 写すもの ----
    ImGui::TextUnformatted("写すもの");
    ImGui::SameLine(); ImGui::RadioButton("ゲーム画面だけ", &captureMode_, kGame);
    if ( ImGui::IsItemHovered() ) { ImGui::SetTooltip("HUD もエディタも無い、ゲームの絵だけ"); }
    ImGui::SameLine(); ImGui::RadioButton("ゲーム＋HUD", &captureMode_, kGameHud);
    if ( ImGui::IsItemHovered() ) { ImGui::SetTooltip("遊んでいる時の画面（HUD 入り）。撮る・録る間はエディタを自動で隠して、終わったら戻す\n（直前を保存は、エディタが出ている間はゲーム画面だけになる）"); }
    ImGui::SameLine(); ImGui::RadioButton("画面そのまま", &captureMode_, kScreen);
    if ( ImGui::IsItemHovered() ) { ImGui::SetTooltip("見えているまま（エディタの窓も入る）"); }

    // 今の様子
    if ( recording_ ) { ImGui::TextColored({ 1.0f, 0.45f, 0.4f, 1.0f }, "録画中：%d 枚（%.2f 秒）", recFrame_, recFrame_ / static_cast< float >( kFramesPerSecond )); }
    else if ( replayOn_ ) { ImGui::TextDisabled("直前 %.1f 秒 たまっている（F5 で保存）", replayFrames_.size() / ( float ) kReplayFps); }
    if ( pending > 0 ) { ImGui::TextColored({ 1.0f, 0.8f, 0.3f, 1.0f }, "保存待ち %d 枚", pending); }
    if ( !processes_.empty() || !replaySaves_.empty() ) { ImGui::TextColored({ 1.0f, 0.8f, 0.3f, 1.0f }, "GIF・mp4 を作っている…"); }
    if ( failed > 0 ) { ImGui::TextColored({ 1.0f, 0.4f, 0.4f, 1.0f }, "保存に失敗した %d 枚", failed); }

    // ---- 最近の保存 ----
    if ( ImGui::CollapsingHeader("最近の保存", ImGuiTreeNodeFlags_DefaultOpen) ) {
        if ( recent_.empty() ) { ImGui::TextDisabled("まだ無い（capture/ に保存する）"); }
        for ( size_t i = 0; i < recent_.size(); ++i ) {
            ImGui::PushID(( int ) i);
            if ( ImGui::SmallButton("開く") ) { OpenFile(recent_[i]); }
            ImGui::SameLine();
            if ( ImGui::SmallButton("フォルダ") ) { RevealFile(recent_[i]); }
            ImGui::SameLine();
            ImGui::TextUnformatted(Utf8FromPath(recent_[i].filename()).c_str());
            ImGui::PopID();
        }
        if ( ImGui::SmallButton("capture フォルダを開く") ) { OpenFile(CaptureDir()); }
    }

    // ---- 設定 ----
    if ( ImGui::CollapsingHeader("設定") ) {
        ImGui::SeparatorText("撮る");
        ImGui::Checkbox("1920×1080 にそろえる", &resizeTo1080_);
        if ( ImGui::IsItemHovered() ) { ImGui::SetTooltip("ウィンドウの大きさに関係なく、スライドの 16:9 の枠に合う大きさで PNG にする\n（16:9 でない時は、真ん中を 16:9 に切り抜いてから縮める）"); }
        ImGui::Checkbox("クリップボードにも入れる（スライドへそのまま貼れる）", &copyToClipboard_);

        ImGui::SeparatorText("録る");
        ImGui::Checkbox("3・2・1 で始める", &countdownOn_);
        ImGui::Checkbox("秒数で止める", &useLimit_);
        if ( useLimit_ ) { ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); ImGui::SliderInt("秒", &limitSeconds_, 1, 15); }
        ImGui::Checkbox("止めたら GIF・mp4 も作る", &autoGif_);
        if ( ImGui::Checkbox("固定コマ送り（1フレーム＝1/60 秒）", &fixedStep_) ) { UpdateFixedStep(); }
        if ( ImGui::IsItemHovered() ) { ImGui::SetTooltip("録画中は保存で1フレームが遅くなる。ON なら、それでもゲームは 1/60 秒ずつ進むので、できた動画はなめらか"); }

        ImGui::SeparatorText("直前を保存（F5）");
        ImGui::Checkbox("いつも直前をためておく", &replayOn_);
        if ( !replayOn_ ) { replayFrames_.clear(); }
        ImGui::SetNextItemWidth(160.0f);
        ImGui::SliderInt("ためる秒数", &replaySeconds_, 3, 15);
        ImGui::TextUnformatted("大きさ"); ImGui::SameLine();
        ImGui::RadioButton("640×360##rw", &replayWidthIndex_, 0); ImGui::SameLine();
        ImGui::RadioButton("960×540##rw", &replayWidthIndex_, 1);
        ImGui::TextDisabled("メモリ：およそ %d MB", ( int ) ( replaySeconds_ * kReplayFps * ( replayWidthIndex_ == 0 ? 640 * 360 : 960 * 540 ) * 3 / ( 1024 * 1024 ) ));

        ImGui::SeparatorText("GIF");
        ImGui::TextUnformatted("録画の GIF の幅"); ImGui::SameLine();
        ImGui::RadioButton("960##gw", &gifWidthIndex_, 0); ImGui::SameLine();
        ImGui::RadioButton("640##gw", &gifWidthIndex_, 1);
        ImGui::TextUnformatted("GIF の fps"); ImGui::SameLine();
        ImGui::RadioButton("30##gf", &gifFpsIndex_, 0); ImGui::SameLine();
        ImGui::RadioButton("15（軽い）##gf", &gifFpsIndex_, 1);
        ImGui::BeginDisabled(lastRecDir_.empty() || recording_);
        if ( ImGui::Button("最後の録画を GIF・mp4 にする") ) { autoGifPending_ = true; }
        ImGui::SameLine();
        if ( ImGui::Button("ffmpeg のコマンドをコピー") ) {
            const std::string text = "cd /d \"" + Utf8FromPath(std::filesystem::current_path()) + "\"\n" + FfmpegCommand(false) + "\n" + FfmpegCommand(true) + "\n";
            ImGui::SetClipboardText(text.c_str());
            Notify("コピーした（GIF と mp4 の2行）");
        }
        ImGui::EndDisabled();
    }

    // ---- テイク ----
    if ( ImGui::CollapsingHeader("テイク（同じ動きを撮り直す）") ) {
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
    }

    // ---- プリセット ----
    if ( ImGui::CollapsingHeader("画角のプリセット") ) {
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
    }
    if ( cameraLocked_ ) {
        ImGui::TextColored({ 0.5f, 0.9f, 1.0f, 1.0f }, "画角を「%s」に固定中", lockedPreset_.name.c_str());
        ImGui::SameLine();
        if ( ImGui::SmallButton("固定をやめる") ) { cameraLocked_ = false; }
    }

    if ( !status_.empty() ) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", status_.c_str());
    }
    ImGui::End();
#endif
}
