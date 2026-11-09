#pragma once
// =====================================================================
//  ScreenCapture：ゲーム画面をそのまま保存する撮影機能（撮影機能 企画書）
//   キー（遷移の演出中・文字入力中でも効く）
//     F12 … スクショ（capture/shot_日時.png。クリップボードにも入るので、スライドへそのまま貼れる）
//     F9  … 録画の開始/停止（3・2・1 で始まる。止めると自動で GIF・mp4 も作る。capture/rec_日時/ と rec_日時.gif/.mp4）
//     F5  … 直前を保存（いつも直前の数秒をためていて、押すとそこを GIF・mp4 にする。capture/replay_日時.gif/.mp4）
//   写すもの
//     ゲーム画面だけ … ImGui も HUD も無い最終画像
//     ゲーム＋HUD    … ImGui を重ねる前の画面（撮る時・録る時はエディタを自動で隠して、終わったら戻す）
//     画面そのまま   … 見えているまま（エディタの窓も入る）
//   保存したら右下に知らせ（開く・フォルダ）。パネル「撮影 (Capture)」に最近の保存の一覧
//   テイク … キー入力をフレームごとに記録して capture/takes/名前.json に保存し、再生しながら録画する（SetTakeHooks）
//   プリセット … カメラの位置・向き・視野角を名前付きで capture/presets.json に保存し、呼び出して固定する
//  写し取り：フレームのコマンドの中で、写すもの（最終画像 / バックバッファ）を読み出し用のバッファへコピーしておき、
//   次のフレームの頭（GPU が描き終わった後）に取り出す。PNG の書き出しは別スレッド。GIF・mp4 は tools/capture/make_gif.py
// =====================================================================
#include "externals/nlohmann/json.hpp"

#include <d3d12.h>
#include <wrl.h>

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class RenderTexture;
class Camera;

class ScreenCapture {
public:
    static ScreenCapture* GetInstance();

    void Initialize();   // Framework の初期化の最後（DirectXCommon にフックを付ける）
    void Finalize();     // 保存待ちを書き切って、スレッドを止める

    // ---- 毎フレーム ----
    // Framework::Update の Input::Update の直後：前のフレームに写した絵の取り出し・キー・テイク・このフレームに写すかを決める
    void OnInputUpdated();
    // BaseScene::ComposeFrame：最終画像ができた所（ゲーム画面だけ を写す時はここでコピーを積む。PIXEL_SHADER_RESOURCE のまま渡す）
    void OnFinalImage(ID3D12GraphicsCommandList* commandList, RenderTexture* finalTarget);
    // Framework::Draw：ImGui を重ねる前（ゲーム＋HUD を写す時はここでコピーを積む。バックバッファは RENDER_TARGET）
    void OnBeforeImGui(ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer);
    // シーンのカメラ（プリセットの登録・呼び出しに使う）。シーンの終わりに nullptr
    void SetCamera(Camera* camera){ camera_ = camera; }
    // プリセットで画角を固定している間、camera へその画角を入れる（シーンがカメラを決めた後に呼ぶ）。固定中なら true
    bool ApplyCameraLock(Camera* camera);

    // ---- 撮影 ----
    void RequestShot(){ shotRequested_ = true; }
    void StartRecording(int limitFrames = 0); // すぐ始める（0＝止めるまで）。キーやボタンは 3・2・1 の後にこれを呼ぶ
    void StopRecording();
    bool IsRecording() const{ return recording_; }
    void SaveReplay();                        // 直前の数秒を GIF・mp4 にする

    // ---- テイク：始める時の状態をシーンが作る ----
    //   capture … 今の状態から「始める時の状態」を JSON に書く（記録の開始時）。できなければ false と理由
    //   apply   … その状態へ戻す（記録・再生の開始時。敵・コインを置き直し、プレイヤーを置き、カメラを合わせる）
    struct TakeHooks {
        std::string scene;   // テイクを撮ったシーン（違うシーンでは再生しない）
        std::function<bool(nlohmann::json& outStart, std::string& outError)> capture;
        std::function<void(const nlohmann::json& start)> apply;
    };
    void SetTakeHooks(const TakeHooks& hooks){ hooks_ = hooks; }
    void ClearTakeHooks(){ hooks_ = {}; }
    bool IsTakePlaying() const{ return takeState_ == TakeState::Playing; }

    // ---- UI（ImGui）----
    void DrawPanel(const char* windowTitle); // パネル「撮影 (Capture)」
    void DrawOverlay();                      // 録画中の「REC」・3・2・1・保存の知らせ（エディタを隠していても出す）

private:
    ScreenCapture();
    ~ScreenCapture(); // Job が cpp の中にあるため
    ScreenCapture(const ScreenCapture&) = delete;
    ScreenCapture& operator=(const ScreenCapture&) = delete;

    enum Source { kGame = 0, kGameHud = 1, kScreen = 2 };

    // 写し取り
    struct Grab { bool shot = false; bool record = false; int recIndex = 0; bool replay = false; bool Any() const{ return shot || record || replay; } };
    void PlanGrab();
    void RecordCopy(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource, D3D12_RESOURCE_STATES state);
    void OnFrameEnd(ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer);
    void ProcessCopied();
    void PushReplayFrame(const uint8_t* source, uint32_t width, uint32_t height, uint32_t pitch);
    void CopyToClipboard(const uint8_t* source, uint32_t width, uint32_t height, uint32_t pitch);
    void PushJob(const uint8_t* source, uint32_t width, uint32_t height, uint32_t pitch, const std::filesystem::path& path);

    void WorkerLoop();
    void UpdateFixedStep();
    void SetEditorHidden(bool hidden);
    void BeginCountdownOrRecord();
    std::filesystem::path CaptureDir() const;
    static std::string TimeStamp();

    // 保存の知らせ・最近の保存
    void Notify(const std::string& text, const std::filesystem::path& file = {});
    void AddRecent(const std::filesystem::path& file);
    static void OpenFile(const std::filesystem::path& file);
    static void RevealFile(const std::filesystem::path& file);

    // テイク
    enum class TakeState { Idle, RecordPending, Recording, PlayPending, Playing };
    void StartTakeRecord();
    void StopTakeRecord();
    void StartTakePlay(bool withRecording);
    void StopTakePlay();
    bool SaveTake(const std::string& name);
    bool LoadTake(const std::string& name);
    void RefreshTakeList();

    // プリセット
    struct Preset { std::string name; float pos[3] {}; float rot[3] {}; float fov = 0.78f; };
    void LoadPresets();
    void SavePresets();
    void ApplyPreset(const Preset& preset, Camera* camera) const;

    // まとめる（GIF・mp4）
    std::string FfmpegCommand(bool mp4) const;
    bool LaunchMakeGif(const std::wstring& arguments, const std::filesystem::path& stem, const std::string& label);
    void PollProcesses();

    // 写す
    Camera* camera_ = nullptr;
    bool initialized_ = false;
    int  captureMode_ = kGameHud;
    bool resizeTo1080_ = true;    // PNG を 1920×1080 にそろえる
    bool copyToClipboard_ = true; // スクショをクリップボードにも入れる
    bool shotRequested_ = false;
    bool shotHidEditor_ = false;  // スクショのためにエディタを隠した（写したら戻す）
    std::string status_;

    // 読み出し用のバッファ（前のフレームに積んだコピーを、このフレームの頭で取り出す）
    Microsoft::WRL::ComPtr<ID3D12Resource> readback_;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint_ {};
    uint64_t readbackBytes_ = 0;
    uint32_t grabWidth_ = 0, grabHeight_ = 0;
    DXGI_FORMAT grabFormat_ = DXGI_FORMAT_UNKNOWN;
    Grab frameGrab_;              // このフレームに写す物
    int  frameSource_ = kGame;    // このフレームに写すもの
    bool copied_ = false;         // このフレームにコピーを積んだ（次のフレームの頭で取り出す）
    Grab copiedGrab_;

    // 録画
    bool recording_ = false;
    bool recordForTake_ = false;  // テイクの再生に合わせた録画（再生の終わりで止める）
    bool recordHidEditor_ = false;
    bool fixedStep_ = true;       // 録画中は固定コマ送り
    bool useLimit_ = true;        // 秒数で止める
    int  limitSeconds_ = 4;
    bool countdownOn_ = true;     // 3・2・1 で始める
    float countdown_ = -1.0f;     // 残り秒（負＝数えていない）
    bool autoGif_ = true;         // 止めたら GIF・mp4 も作る
    bool autoGifPending_ = false;
    int  recLimitFrames_ = 0;
    int  recFrame_ = 0;
    std::filesystem::path recDir_;
    std::filesystem::path lastRecDir_;
    int  gifWidthIndex_ = 0;      // 0=960 / 1=640
    int  gifFpsIndex_ = 0;        // 0=30 / 1=15

    // 直前を保存（リプレイ）：30fps で縮めた絵をためておく
    bool replayOn_ = true;
    int  replaySeconds_ = 8;
    int  replayWidthIndex_ = 0;   // 0=640 / 1=960
    uint64_t replayTick_ = 0;
    std::deque<std::vector<uint8_t>> replayFrames_;
    uint32_t replayW_ = 0, replayH_ = 0;
    struct ReplaySave { std::future<bool> written; std::filesystem::path raw; uint32_t width = 0, height = 0; };
    std::vector<ReplaySave> replaySaves_;

    // 書き出しのスレッド
    struct Job;
    std::vector<std::thread> workers_;
    std::deque<std::unique_ptr<Job>> jobs_;
    std::mutex mutex_;
    std::condition_variable jobReady_;
    std::condition_variable jobSpace_;
    bool stopWorkers_ = false;
    int  busyWorkers_ = 0;
    int  failedWrites_ = 0;

    // GIF にまとめるプロセス
    struct Process { void* handle = nullptr; std::filesystem::path stem; std::string label; };
    std::vector<Process> processes_;

    // 知らせ・最近の保存
    std::string toastText_;
    std::filesystem::path toastFile_;
    float toastTime_ = 0.0f;
    std::vector<std::filesystem::path> recent_;

    // テイク
    TakeHooks hooks_;
    TakeState takeState_ = TakeState::Idle;
    std::vector<std::vector<uint8_t>> takeFrames_;
    nlohmann::json takeStart_;
    std::string takeScene_;
    size_t playIndex_ = 0;
    bool playWithRecording_ = false;
    char takeName_[64] = "take";
    std::vector<std::string> takeList_;
    int  selectedTake_ = -1;
    bool takeListDirty_ = true;

    // プリセット
    std::vector<Preset> presets_;
    bool presetsLoaded_ = false;
    int  selectedPreset_ = -1;
    bool cameraLocked_ = false;
    Preset lockedPreset_ {};
    char presetName_[64] = "shot";
};
