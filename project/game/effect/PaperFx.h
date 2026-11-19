#pragma once
#include "engine/math/struct.h"

#include <cstdint>
#include <memory>
#include <random>
#include <vector>

namespace KazumimiEngine {
class Camera;
class InstancedGroup;
}

// =====================================================================
//  PaperFx：工作の世界に合わせた、立体の小さなパーティクル（ステージ選択）。
//   ・紙吹雪 … 色紙の薄い切れはし。パッと舞い上がり、くるくる回りながらひらひら落ちる
//   ・綿のけむり … フェルトの玉。ふくらみながら上がって、しぼんで消える（汽車の煙・土ぼこり）
//   色ごとに1つの一括描画（InstancedGroup）。描くのは BaseScene::OnDrawInstanced から
// =====================================================================
class PaperFx {
public:
    enum class PuffColor { White, Kraft };

    PaperFx();
    ~PaperFx();

    void Initialize(uint32_t noiseSrv);
    void Clear();

    // 紙吹雪：at から上へ、count 枚。speed＝舞い上がる速さ(m/秒)、size＝1枚の大きさ(m)、
    //   lifeScale＝消えるまでの長さの倍率（上から降らせる時は長くする）
    void Confetti(const KazumimiEngine::Vector3& at, int count, float speed, float size, float lifeScale = 1.0f);
    // 綿のけむり：size＝いちばんふくらんだ時の直径(m)、life＝消えるまで(秒)
    void Puff(const KazumimiEngine::Vector3& at, const KazumimiEngine::Vector3& velocity, float size, float life, PuffColor color);
    // 綿のけむりを輪にして広げる（着地・跳んだ時の土ぼこり）
    void PuffRing(const KazumimiEngine::Vector3& at, int count, float speed, float size, PuffColor color);

    void Update(float deltaTime, const KazumimiEngine::Camera* camera);
    void Draw(const KazumimiEngine::Camera* camera);

private:
    // 紙吹雪 1 枚・綿のけむり 1 つ（位置・速さ・回転・大きさ・寿命）
    struct Piece {
        int     group = 0;
        bool    confetti = true;
        KazumimiEngine::Vector3 position {}, velocity {}, rotation {}, spin {};
        float   size = 0.1f;
        float   age = 0.0f, life = 1.0f;
        float   phase = 0.0f; // ひらひらの位相
    };
    struct Group;
    float Random(float lo, float hi);

    std::vector<Group> groups_;
    std::vector<Piece> pieces_;
    std::mt19937 random_ { 7u };
    int confettiGroups_ = 0; // groups_ の先頭からこの数が紙吹雪の色
};
