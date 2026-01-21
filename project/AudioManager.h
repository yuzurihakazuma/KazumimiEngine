#pragma once
#include <d3d12.h>
#include <xaudio2.h>
#include <wrl.h>
#include <string>
#include <map>
#include <cstdint>

#pragma comment(lib,"xaudio2.lib")

// 音声データ
struct SoundData{
	// 波型フォーマット
	WAVEFORMATEX wfex;
	// バッファの先頭アドレス
	BYTE* pBuffer;
	// バッファのサイズ
	unsigned int bufferSize;

};

class AudioManager{
public: // シングルトンパターン
    static AudioManager* GetInstance();

public: // メンバ関数

    // 初期化
    void Initialize();

    // 終了処理
    void Finalize();

    // WAVファイルの読み込み
    // filename: ファイルパス ("resources/bgm.wav" など)
    void LoadWave(const std::string& filename);

    // 音声再生
    // volume: 音量 (0.0f ~ 1.0f)
    void PlayWave(const std::string& filename, bool loop = false, float volume = 1.0f);

    // 音声データの解放
    void Unload(SoundData* soundData);

private: // 内部構造体（WAV読み込み用）
    // チャンクヘッダ
    struct ChunkHeader{
        char id[4]; // チャンクID
        int32_t size; // チャンクのサイズ
    };
    // RIFFヘッダチャンク
    struct RiffHeader{
        ChunkHeader chunk; // RIFF
        char type[4]; // WAVE
    };
    // FMTチャンク
    struct FormatChunk{
        ChunkHeader chunk; // FMT
        WAVEFORMATEX fmt;  // 波形フォーマット
    };

    // 内部的なWAV読み込み関数
    SoundData LoadWaveInternal(const std::string& filename);

private:
    AudioManager() = default;
    ~AudioManager() = default;
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    // XAudio2の本体
    Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
    // マスターボイス（スピーカー出力）
    IXAudio2MasteringVoice* masterVoice_ = nullptr;

    // 読み込んだ音声データの管理コンテナ
    // キー: ファイル名, 値: 音声データ
    std::map<std::string, SoundData> soundDatas_;
};