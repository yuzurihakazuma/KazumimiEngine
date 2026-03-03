#include "AudioManager.h"
// --- 標準ライブラリ ---
#include <fstream>
#include <cassert>
#include <cstring>

AudioManager* AudioManager::GetInstance(){
    static AudioManager instance;
    return &instance;
}

void AudioManager::Initialize(){
    HRESULT result;
    // COMとMedia Foundationの初期化
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION);

    // XAudio2エンジンのインスタンス生成
    result = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
    assert(SUCCEEDED(result));

    // マスターボイス生成
    result = xAudio2_->CreateMasteringVoice(&masterVoice_);
    assert(SUCCEEDED(result));
}

void AudioManager::Finalize(){
	// 再生中の音声ボイスの破棄
    playingVoices_.clear();
    
   
	// マスターボイスの破棄
    if ( masterVoice_ ) {
        masterVoice_->DestroyVoice();
        masterVoice_ = nullptr;
    }
    // XAudio2の解放
    xAudio2_.Reset();

	// 読み込んだ音声データの解放
    soundDatas_.clear();

    MFShutdown();
    CoUninitialize();
}

void AudioManager::UpdateVoices(){
    // playingVoices_ を走査して、再生が終わったボイスを削除する
    for ( auto it = playingVoices_.begin(); it != playingVoices_.end(); ) {

        XAUDIO2_VOICE_STATE st {};
        ( *it )->GetState(&st);

        // BuffersQueued == 0 なら、もう再生するバッファがない＝再生終了扱い
        if ( st.BuffersQueued == 0 ) {
            // erase で SourceVoicePtr が破棄され、DestroyVoice() が呼ばれる
            it = playingVoices_.erase(it);
        } else {
            ++it;
        }
    }
}


void AudioManager::LoadWave(const std::string& filename) {
    // 既に読み込み済みならスルー
    if (soundDatas_.find(filename) != soundDatas_.end()) {
        return;
    }

    // ".mp3" が含まれているかで分岐
    if (filename.find(".mp3") != std::string::npos) {
        soundDatas_[filename] = LoadMP3Internal(filename);
    }
    else {
        soundDatas_[filename] = LoadWaveInternal(filename);
    }
}
void AudioManager::PlayWave(const std::string& filename, bool loop, float volume){
    // 未読み込みならエラー（またはロードする）
    assert(soundDatas_.contains(filename));
    SoundData& soundData = soundDatas_[filename];

	// ソースボイスの取得
    HRESULT result;

    // ソースボイス（音源）の生成
    IXAudio2SourceVoice* rawVoice = nullptr;
    result = xAudio2_->CreateSourceVoice(&rawVoice, &soundData.wfex);
    assert(SUCCEEDED(result));

	// スマートポインタに変換
    SourceVoicePtr voice(rawVoice);

    // 再生設定
    XAUDIO2_BUFFER buf {};
    buf.pAudioData = soundData.buffer.data();                
    buf.AudioBytes = static_cast< UINT32 >( soundData.buffer.size() );
    buf.Flags = XAUDIO2_END_OF_STREAM;

    // ループ設定
    if ( loop ) {
        buf.LoopCount = XAUDIO2_LOOP_INFINITE; // 無限ループ
        buf.Flags = 0;
    }

    voice->SetVolume(volume);

 
    // バッファを登録
    result = voice->SubmitSourceBuffer(&buf);
    assert(SUCCEEDED(result));

    // 再生開始
    result = voice->Start();
    assert(SUCCEEDED(result));

	// 再生中ボイスリストに追加
    playingVoices_.push_back(std::move(voice));
}

SoundData AudioManager::LoadMP3Internal(const std::string& filename) {
    HRESULT hr;
    SoundData soundData = {};

    // 1. std::string を std::wstring に変換 (Media Foundationはワイド文字必須なため)
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &filename[0], (int)filename.size(), NULL, 0);
    std::wstring wFilename(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &filename[0], (int)filename.size(), &wFilename[0], size_needed);

    // 2. ソースリーダーの作成（ファイルを開く）
    IMFSourceReader* pReader = nullptr;
    hr = MFCreateSourceReaderFromURL(wFilename.c_str(), nullptr, &pReader);
    assert(SUCCEEDED(hr));

    // 3. オーディオストリームを選択し、強制的に「PCM（無圧縮WAV）」に変換する設定
    IMFMediaType* pPartialType = nullptr;
    hr = MFCreateMediaType(&pPartialType);
    assert(SUCCEEDED(hr));
    hr = pPartialType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    hr = pPartialType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    hr = pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pPartialType);
    assert(SUCCEEDED(hr));
    pPartialType->Release();

    // 4. 変換後のメディアタイプ（WAVEFORMATEX）を取得
    IMFMediaType* pUncompressedAudioType = nullptr;
    hr = pReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pUncompressedAudioType);
    assert(SUCCEEDED(hr));

    WAVEFORMATEX* pWavFormat = nullptr;
    UINT32 cbFormat = 0;
    hr = MFCreateWaveFormatExFromMFMediaType(pUncompressedAudioType, &pWavFormat, &cbFormat);
    assert(SUCCEEDED(hr));

    // フォーマットをコピーして確保
    memcpy(&soundData.wfex, pWavFormat, sizeof(WAVEFORMATEX));
    CoTaskMemFree(pWavFormat);
    pUncompressedAudioType->Release();

    // 5. データを最後まで読み込んでバッファに詰める
    IMFSample* pSample = nullptr;
    DWORD dwFlags = 0;
    while (true) {
        hr = pReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &dwFlags, nullptr, &pSample);
        assert(SUCCEEDED(hr));

        if (dwFlags & MF_SOURCE_READERF_ENDOFSTREAM) {
            break; // 終端に達した
        }

        if (pSample) {
            IMFMediaBuffer* pBuffer = nullptr;
            hr = pSample->ConvertToContiguousBuffer(&pBuffer);
            assert(SUCCEEDED(hr));

            BYTE* pAudioData = nullptr;
            DWORD cbCurrentLength = 0;
            hr = pBuffer->Lock(&pAudioData, nullptr, &cbCurrentLength);
            assert(SUCCEEDED(hr));

            // ベクターの末尾に解凍されたデータを追加
            size_t currentSize = soundData.buffer.size();
            soundData.buffer.resize(currentSize + cbCurrentLength);
            memcpy(soundData.buffer.data() + currentSize, pAudioData, cbCurrentLength);

            pBuffer->Unlock();
            pBuffer->Release();
            pSample->Release();
        }
    }

    pReader->Release();

    return soundData; // 解凍済みのピカピカなデータを返す！
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
    std::vector<BYTE> buffer;

    // ファイルを走査
    while ( file.read(( char* ) &chunk, sizeof(chunk)) ) {
        if ( strncmp(chunk.id, "fmt ", 4) == 0 ) {
            // fmtチャンク読み込み
            assert(chunk.size <= sizeof(WAVEFORMATEX));
            file.read(( char* ) &wfex, chunk.size);
        } else if ( strncmp(chunk.id, "data", 4) == 0 ) {
            // dataチャンク読み込み（音声データ本体）
            buffer.resize(chunk.size);
            file.read(( char* ) buffer.data(), chunk.size);
        } else {
            // 不要なチャンクはスキップ
            file.seekg(chunk.size, std::ios_base::cur);
        }

        // 必要な情報が揃ったらループを抜ける
        if ( wfex.nChannels != 0 && !buffer.empty() ) {
            break;
        }
    }
    file.close();

    SoundData soundData {};
    soundData.wfex = wfex;
    soundData.buffer = std::move(buffer);

    return soundData;
}