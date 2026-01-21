#include "AudioManager.h"
#include <fstream>
#include <cassert>

AudioManager* AudioManager::GetInstance(){
    static AudioManager instance;
    return &instance;
}

void AudioManager::Initialize(){
    HRESULT result;
    // XAudio2エンジンのインスタンス生成
    result = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
    assert(SUCCEEDED(result));

    // マスターボイス生成
    result = xAudio2_->CreateMasteringVoice(&masterVoice_);
    assert(SUCCEEDED(result));
}

void AudioManager::Finalize(){
    // XAudio2の解放
    xAudio2_.Reset();

    // 音声データの解放
    for ( auto& pair : soundDatas_ ) {
        Unload(&pair.second);
    }
    soundDatas_.clear();
}

void AudioManager::LoadWave(const std::string& filename){
    // 既に読み込み済みなら何もしない
    if ( soundDatas_.contains(filename) ) {
        return;
    }

    // ファイルから読み込んでマップに保存
    soundDatas_[filename] = LoadWaveInternal(filename);
}

void AudioManager::Unload(SoundData* soundData){
    // バッファのメモリを解放
    delete[] soundData->pBuffer;
    soundData->pBuffer = nullptr;
    soundData->bufferSize = 0;
    soundData->wfex = {};
}

void AudioManager::PlayWave(const std::string& filename, bool loop, float volume){
    // 未読み込みならエラー（またはロードする）
    assert(soundDatas_.contains(filename));
    SoundData& soundData = soundDatas_[filename];

    HRESULT result;

    // ソースボイス（音源）の生成
    IXAudio2SourceVoice* pSourceVoice = nullptr;
    result = xAudio2_->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
    assert(SUCCEEDED(result));

    // 再生設定
    XAUDIO2_BUFFER buf {};
    buf.pAudioData = soundData.pBuffer;
    buf.AudioBytes = soundData.bufferSize;
    buf.Flags = XAUDIO2_END_OF_STREAM;

    // ループ設定
    if ( loop ) {
        buf.LoopCount = XAUDIO2_LOOP_INFINITE; // 無限ループ
    }

    // 音量の設定
    pSourceVoice->SetVolume(volume);

    // バッファを登録
    result = pSourceVoice->SubmitSourceBuffer(&buf);
    assert(SUCCEEDED(result));

    // 再生開始
    result = pSourceVoice->Start();
    assert(SUCCEEDED(result));
}

// 内部用：WAVファイル読み込みの実装（main.cppにあったものを移植）
SoundData AudioManager::LoadWaveInternal(const std::string& filename){
    std::ifstream file(filename, std::ios::binary);
    assert(file.is_open());

    // RIFFヘッダー確認
    RiffHeader riff;
    file.read(( char* ) &riff, sizeof(riff));
    assert(strncmp(riff.chunk.id, "RIFF", 4) == 0);
    assert(strncmp(riff.type, "WAVE", 4) == 0);

    FormatChunk format = {};

    // チャンク情報の初期化
    ChunkHeader chunk;
    WAVEFORMATEX wfex = {};
    BYTE* pBuffer = nullptr;
    uint32_t bufferSize = 0;

    // ファイルを走査
    while ( file.read(( char* ) &chunk, sizeof(chunk)) ) {
        if ( strncmp(chunk.id, "fmt ", 4) == 0 ) {
            // fmtチャンク読み込み
            assert(chunk.size <= sizeof(WAVEFORMATEX));
            file.read(( char* ) &wfex, chunk.size);
        } else if ( strncmp(chunk.id, "data", 4) == 0 ) {
            // dataチャンク読み込み（音声データ本体）
            pBuffer = new BYTE[chunk.size];
            file.read(( char* ) pBuffer, chunk.size);
            bufferSize = chunk.size;
        } else {
            // 不要なチャンクはスキップ
            file.seekg(chunk.size, std::ios_base::cur);
        }

        // 必要な情報が揃ったらループを抜ける
        if ( wfex.cbSize != 0 && pBuffer != nullptr ) {
            break;
        }
    }
    file.close();

    SoundData soundData = {};
    soundData.wfex = wfex;
    soundData.pBuffer = pBuffer;
    soundData.bufferSize = bufferSize;

    return soundData;
}