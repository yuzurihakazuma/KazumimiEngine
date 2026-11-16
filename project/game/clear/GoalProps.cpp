#include "game/clear/GoalProps.h"

#include "engine/3d/model/Model.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/3d/obj/Obj3d.h"
#include "engine/audio/AudioManager.h"
#include "engine/math/Matrix4x4.h"
#include "engine/math/VectorMath.h"
#include "engine/utils/Easing.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {
    constexpr const char* kLetterTexture = "resources/clear/goal_letter_";
    constexpr const char* kStarTexture = "resources/clear/goal_star_";
    constexpr int   kStarColors = 3;           // 星の色の数（goal_star_0〜2.png）
    constexpr float kDepthStep = 0.03f;        // 板どうしが同じ奥行きで重ならないよう、1 枚ごとに手前へずらす(m)
    constexpr float kSwingStart = 0.6f;        // 文字：下りる時間のこの割合から、糸の揺れが始まる
    constexpr float kSwingDecay = 3.0f;        // 糸の揺れがおさまる速さ
    constexpr float kSwingRate = 1.4f;         // 糸の揺れの速さ(回/秒)
    constexpr float kSwayRate = 0.35f;         // その後の、ゆっくりした揺れの速さ(回/秒)
    constexpr float kStarWobble = 0.05f;       // 星：出た後の、小さな揺れ(rad)
    constexpr float kLetterVolume = 0.6f, kStarVolume = 0.4f;

    // 絵を貼った板のモデルを作る（光は当てない＝絵の色のまま）
    void CreateBoardModel(const std::string& name, const std::string& texture, float width, float height){
        ModelManager* modelManager = ModelManager::GetInstance();
        if ( !modelManager->FindModel(name) ) { modelManager->CreatePlaneModel(name, width, height); }
        if ( Model* model = modelManager->FindModel(name) ) {
            model->SetTexture(texture);
            if ( model->GetMaterial() ) { model->GetMaterial()->enableLighting = 0; }
        }
    }
}

GoalProps::GoalProps() = default;
GoalProps::~GoalProps() = default;

void GoalProps::Initialize(const ClearSettings& settings){
    settings_ = &settings;
    const ClearLetters& letters = settings.letters;
    const ClearStars& stars = settings.stars;
    const ClearSounds& sounds = settings.sounds;

    // 切り文字：絵は「文字のマス＋上の糸」の縦長。マスの高さ letters.height に合わせて板の大きさを決める
    letterBoards_.clear();
    const float letterHeight = letters.height * ( 1.0f + letters.string );
    const float letterWidth = letterHeight * letters.textureSize.x / ( std::max )( letters.textureSize.y, 1.0f );
    for ( int i = 0; i < letters.count; ++i ) {
        const std::string name = "goalLetter" + std::to_string(i);
        CreateBoardModel(name, kLetterTexture + std::to_string(i) + ".png", letterWidth, letterHeight);
        Board board;
        board.object = Obj3d::Create(name);
        board.width = letterWidth;
        board.height = letterHeight;
        if ( board.object ) { board.object->SetPipelineType(PipelineType::Object3D_CullNone); }
        letterBoards_.push_back(std::move(board));
    }

    // 棒付きの星：絵は「星＋下の棒」の縦長。左右に perSide 本ずつ
    starBoards_.clear();
    const float starHeight = stars.height * stars.textureSize.y / ( std::max )( stars.textureSize.x, 1.0f );
    for ( int color = 0; color < kStarColors; ++color ) {
        CreateBoardModel("goalStar" + std::to_string(color), kStarTexture + std::to_string(color) + ".png", stars.height, starHeight);
    }
    for ( int i = 0; i < stars.perSide * 2; ++i ) {
        Board board;
        board.object = Obj3d::Create("goalStar" + std::to_string(i % kStarColors));
        board.width = stars.height;
        board.height = starHeight;
        if ( board.object ) { board.object->SetPipelineType(PipelineType::Object3D_CullNone); }
        starBoards_.push_back(std::move(board));
    }

    AudioManager* audio = AudioManager::GetInstance();
    audio->LoadWave(sounds.letter);
    audio->LoadWave(sounds.star);
}

void GoalProps::Begin(const Vector3& origin, const Vector3& toCamera){
    origin_ = origin;
    toCamera_ = VectorMath::NormalizeSafe({ toCamera.x, 0.0f, toCamera.z }, { 1.0f, 0.0f, 0.0f });
    const Vector3 view = toCamera_ * -1.0f;                 // カメラが見ている向き
    right_ = VectorMath::Cross({ 0.0f, 1.0f, 0.0f }, view); // 画面の右
    faceYaw_ = std::atan2(view.x, view.z);                  // 板の表（-Z）をカメラへ
    visible_ = true;
    for ( Board& board : letterBoards_ ) { board.soundPlayed = false; }
    for ( Board& board : starBoards_ ) { board.soundPlayed = false; }
    Update(0.0f);
}

void GoalProps::Hide(){
    visible_ = false;
}

// 舞台の点（x＝画面の右・y＝上・z＝手前）→ 世界
Vector3 GoalProps::ToWorld(const Vector3& stagePoint) const{
    return origin_ + right_ * stagePoint.x + Vector3 { 0.0f, stagePoint.y, 0.0f } + toCamera_ * stagePoint.z;
}

void GoalProps::PlaceBoard(Board& board, const Vector3& pivot, float angle, bool hangs, float scale){
    if ( !board.object ) { return; }
    using namespace MatrixMath;
    // 支点から板の真ん中まで：ぶら下がる板は下へ、立つ板は上へ。画面の中で angle だけ回す（左回りが＋）
    const float half = board.height * 0.5f * scale * ( hangs ? -1.0f : 1.0f );
    const Vector3 center { pivot.x - std::sin(angle) * half, pivot.y + std::cos(angle) * half, pivot.z };
    board.object->SetTranslation(ToWorld(center));
    // 板の中で回してから（Z）、カメラの方へ向ける（Y）
    board.object->SetRotation(ToEulerXYZ(Multiply(MakeRotateZ(angle), MakeRotateY(faceYaw_))));
    board.object->SetScale({ scale, scale, 1.0f });
    board.object->Update();
}

void GoalProps::Update(float elapsed){
    if ( !visible_ || !settings_ ) { return; }
    const ClearLetters& letters = settings_->letters;
    const ClearStars& stars = settings_->stars;
    const ClearSounds& sounds = settings_->sounds;
    AudioManager* audio = AudioManager::GetInstance();

    // --- 切り文字：上から下がる（行き過ぎて戻る）→ 糸で揺れる ---
    const int letterCount = static_cast<int>( letterBoards_.size() );
    for ( int i = 0; i < letterCount; ++i ) {
        Board& board = letterBoards_[i];
        const float time = elapsed - ( letters.start + letters.interval * static_cast<float>( i ) );
        const float drop = ( letters.dropTime > 0.0f ) ? Easing::Clamp01(time / letters.dropTime) : 1.0f;
        if ( time >= 0.0f && !board.soundPlayed ) {
            board.soundPlayed = true;
            audio->PlayWave(sounds.letter, false, kLetterVolume);
        }
        // 支点＝糸の上のはし。下り切った時に、文字のマスの真ん中が centerHeight に来る
        const float restTop = letters.centerHeight - letters.height * 0.5f + board.height;
        const float lift = letters.dropHeight * ( 1.0f - Easing::EaseOutBack(drop) );
        const float swingTime = ( std::max )( time - letters.dropTime * kSwingStart, 0.0f );
        const float swing = letters.swing * std::exp(-kSwingDecay * swingTime) * std::sin(2.0f * Easing::kPi * kSwingRate * swingTime);
        const float sway = letters.sway * std::sin(2.0f * Easing::kPi * kSwayRate * elapsed + static_cast<float>( i ) * 1.7f);
        const float across = ( static_cast<float>( i ) - static_cast<float>( letterCount - 1 ) * 0.5f ) * letters.spacing;
        const Vector3 pivot { across, restTop + ( time >= 0.0f ? lift : letters.dropHeight ), -letters.back + kDepthStep * static_cast<float>( i ) };
        PlaceBoard(board, pivot, ( time >= 0.0f ) ? swing + sway : 0.0f, true, 1.0f);
    }

    // --- 棒付きの星：左右の下から、扇状に飛び出す（行き過ぎて戻る）---
    const int starCount = static_cast<int>( starBoards_.size() );
    for ( int i = 0; i < starCount; ++i ) {
        Board& board = starBoards_[i];
        const int slot = i / 2;                       // 内側から何本目か
        const float side = ( i % 2 == 0 ) ? -1.0f : 1.0f; // 左・右
        const float time = elapsed - ( stars.start + stars.interval * static_cast<float>( i ) );
        const float pop = ( stars.popTime > 0.0f ) ? Easing::Clamp01(time / stars.popTime) : 1.0f;
        if ( time >= 0.0f && !board.soundPlayed ) {
            board.soundPlayed = true;
            audio->PlayWave(sounds.star, false, kStarVolume);
        }
        const float fan = stars.angles.empty() ? 0.6f : stars.angles[( std::min )( slot, static_cast<int>( stars.angles.size() ) - 1 )];
        const float wobble = kStarWobble * std::sin(2.0f * Easing::kPi * kSwayRate * 2.0f * elapsed + static_cast<float>( i ));
        // 左の星は右（内側）へ、右の星は左へ倒す。左回りが＋なので、左の星はマイナス
        const float angle = side * ( fan + wobble );
        const float scale = ( time >= 0.0f ) ? Easing::EaseOutBack(pop) : 0.0f; // 根元から伸びて出る
        const Vector3 pivot { side * stars.pivotSide, stars.pivotHeight, -stars.back - kDepthStep * static_cast<float>( i ) };
        PlaceBoard(board, pivot, angle, false, ( std::max )( scale, 0.001f ));
    }
}

void GoalProps::Draw(){
    if ( !visible_ ) { return; }
    // 奥の物から（星 → 文字）
    for ( Board& board : starBoards_ ) { if ( board.object ) { board.object->Draw(); } }
    for ( Board& board : letterBoards_ ) { if ( board.object ) { board.object->Draw(); } }
}
