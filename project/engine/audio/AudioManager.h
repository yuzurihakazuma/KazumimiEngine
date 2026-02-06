#pragma once


#include <xaudio2.h>
#include <wrl/client.h>
#include <string>
#include <map>
#include <cstdint>
#include <vector>
#include <memory>

#pragma comment(lib,"xaudio2.lib")

// 音声データ
struct SoundData{
	// 波型フォーマット
	WAVEFORMATEX wfex;
	// 音声バッファ
    std::vector<BYTE> buffer;

};

// IXAudio2SourceVoiceのデリータ
struct SourceVoiceDeleter{
	// デストラクタ
    void operator()(IXAudio2SourceVoice* v) const noexcept{
        if ( v ) {
            v->DestroyVoice();
        }
    }
};
// IXAudio2SourceVoiceのスマートポインタ
using SourceVoicePtr = std::unique_ptr<IXAudio2SourceVoice, SourceVoiceDeleter>;


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
    
	// 音声更新
    void UpdateVoices();

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

private:

    // XAudio2の本体
    Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
    // マスターボイス（スピーカー出力）
    IXAudio2MasteringVoice* masterVoice_ = nullptr;

    // 読み込んだ音声データの管理コンテナ
    // キー: ファイル名, 値: 音声データ
    std::map<std::string, SoundData> soundDatas_;

	// 再生中の音声ボイスの管理コンテナ
    std::vector<SourceVoicePtr> playingVoices_;

};