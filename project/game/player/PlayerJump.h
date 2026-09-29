#pragma once
// =====================================================================
//  PlayerJump：プレイヤーの縦の動き（レールに乗っている間）。
//   height = 足のレール面からの高さ(m)。ジャンプ・ふんばり（ヨッシー風の長押し滞空）・重力を
//   m, m/s, m/s^2 で扱う（フレームレート非依存）。
//   ふんばりは空中で SPACE を押し直すと何度でもできるが、回数ごとに上がる力が弱くなる。
//   ブロックや穴との当たり（着地先の決定）は Player と PlayerBlockContact が行う
// =====================================================================
class PlayerJump {
public:
    static constexpr float kFlutterTime   = 0.6f; // ふんばりで滞空できる最大時間（秒）
    static constexpr float kFlutterTarget = 1.2f; // 滞空中になめらかに近づく上向き速度
    static constexpr float kFlutterEase   = 6.0f; // 目標へ近づく速さ
    static constexpr float kFlutterDecay  = 0.65f; // 押し直すたびに目標速度へ掛かる倍率

    float height = 0.0f;      // 足のレール面からの高さ (m)
    float velocity = 0.0f;    // 上下速度 (m/s)
    bool  grounded = true;    // 接地しているか（空中で再ジャンプ不可）
    float power = 8.0f;       // ジャンプ初速 (m/s)
    float gravity = 25.0f;    // 重力加速度 (m/s^2)
    float flutterTime = 0.0f; // 滞空できる残り時間 (秒)
    int   flutterCount = 0;   // このジャンプ中にふんばった回数
    bool  fluttering = false; // 今まさにふんばり中か（足バタバタアニメの切り替えに使う）

    void Reset();

    // 上へ跳ぶ（ジャンプ・踏みつけの跳ね返り・ジャンプ台・ノックバックの小跳ね）
    void Launch(float upVelocity){ velocity = upVelocity; grounded = false; }
    // 地面（高さ groundY）に着地する。ふんばりの残りと回数もリセット
    void Land(float groundY);

    // 1フレームぶん上下速度を進める（ジャンプ開始・ふんばり・重力）。高さは動かさない
    //   pressed=SPACEを押した瞬間 / held=押している間 / locked=構え中などでジャンプ禁止
    void UpdateVelocity(bool pressed, bool held, bool locked, float dt);

    // ふんばり（長押し中は target へなめらかに近づく）か重力の1フレーム。ふんばった時 true。
    //   レール外の空中（自由落下）でも同じ式を使う
    static bool StepFlutterOrGravity(float& upVelocity, float& flutterTimeLeft, float target,
                                     float gravityAccel, bool held, float dt);
};
