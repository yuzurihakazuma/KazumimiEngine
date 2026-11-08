#pragma once
// =====================================================================
//  ScreenCapture：ゲーム画面をそのまま PNG で保存する撮影機能（撮影機能 企画書）
//   ・スクショ   … F12 かパネルのボタン。capture/shot_日時.png
//   ・連番録画   … F9 で開始/停止（秒数で自動停止も）。capture/rec_日時/00000.png 〜
//                   録画中は固定コマ送り（1フレーム＝1/60秒）にして、保存が遅くても動きはなめらか
//   ・まとめる   … 録画を GIF・mp4 にする（ffmpeg のコマンドをコピー / tools/capture/make_gif.py を実行）
//   ・テイク     … キー入力をフレームごとに記録して capture/takes/名前.json に保存し、再生しながら録画する。
//                   記録・再生の始めに、シーンが「始める時の状態」へ戻す（SetTakeHooks）
//   ・プリセット … カメラの位置・向き・視野角を名前付きで capture/presets.json に保存し、呼び出して固定する
//  写すのは ImGui を重ねる前の最終画像（BaseScene::ComposeFrame が SetSource で渡す）。
//  「ImGui ごと」を選ぶと、画面に出すバックバッファ（エディタの窓も入る）を写す。
//  写し取りは画面に出す直前（DirectXCommon の BeforePresent フック）、PNG の書き出しは別スレッド
// =====================================================================
#include "externals/nlohmann/json.hpp"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct ID3D12Resource;
class RenderTexture;
class Camera;

class ScreenCapture {
public:
    static ScreenCapture* GetInstance();

    void Initialize();   // Framework の初期化の最後（DirectXCommon にフックを付ける）
    void Finalize();     // 保存待ちを書き切って、スレッドを止める

    // ---- 毎フレーム ----
    // Framework::Update の Input::Update の直後：F9/F12、テイクの記録・再生（キーの差し替え）
    void OnInputUpdated();
    // BaseScene::ComposeFrame：写す最終画像（ImGui を重ねる前。PIXEL_SHADER_RESOURCE のまま渡す）
    void SetSource(RenderTexture* finalTarget){ source_ = finalTarget; }
    // シーンのカメラ（プリセットの登録・呼び出しに使う）。シーンの終わりに nullptr
    void SetCamera(Camera* camera){ camera_ = camera; }
    // プリセットで画角を固定している間、camera へその画角を入れる（シーンがカメラを決めた後に呼ぶ）。固定中なら true
    bool ApplyCameraLock(Camera* camera);

    // ---- 撮影 ----
    void RequestShot(){ shotRequested_ = true; }
    void StartRecording(int limitFrames = 0); // 0＝止めるまで
    void StopRecording();
    bool IsRecording() const{ return recording_; }

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
    void DrawOverlay();                      // 録画中の「● REC」（ImGui なので録画には入らない）

private:
    ScreenCapture();
    ~ScreenCapture(); // Job が cpp の中にあるため
    ScreenCapture(const ScreenCapture&) = delete;
    ScreenCapture& operator=(const ScreenCapture&) = delete;

    void BeforePresent(ID3D12Resource* backBuffer);
    void WorkerLoop();
    void UpdateFixedStep();
    std::filesystem::path CaptureDir() const;
    static std::string TimeStamp();

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
    void StartMakeGif();
    void PollMakeGif();

    // 写す
    RenderTexture* source_ = nullptr;
    Camera* camera_ = nullptr;
    bool initialized_ = false;
    bool shotRequested_ = false;
    int  captureMode_ = 0;        // 0=ゲーム画面だけ（最終画像） / 1=ImGui ごと（バックバッファ）
    bool resizeTo1080_ = true;    // 1920×1080 にそろえる（ウィンドウの大きさに関係なくスライドの枠に合う）
    std::string lastShot_;
    std::string status_;

    // 録画
    bool recording_ = false;
    bool recordForTake_ = false;  // テイクの再生に合わせた録画（再生の終わりで止める）
    bool fixedStep_ = true;       // 録画中は固定コマ送り
    bool useLimit_ = true;        // 秒数で止める
    int  limitSeconds_ = 3;
    int  recLimitFrames_ = 0;
    int  recFrame_ = 0;
    std::filesystem::path recDir_;
    std::filesystem::path lastRecDir_;
    int  gifWidthIndex_ = 0;      // 0=960 / 1=640
    int  gifFpsIndex_ = 0;        // 0=30 / 1=15

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
    void* gifProcess_ = nullptr;  // HANDLE
    std::string gifStatus_;

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
