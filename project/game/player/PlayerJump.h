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

    // --- 今の状態 ---
    float Height() const{ return height_; }         // 足のレール面からの高さ (m)
    float Velocity() const{ return velocity_; }     // 上下速度 (m/s)
    bool  IsGrounded() const{ return grounded_; }   // 接地しているか
    bool  IsFluttering() const{ return fluttering_; } // 今まさにふんばり中か（足バタバタアニメの切り替えに使う）
    float Power() const{ return power_; }           // ジャンプ初速 (m/s)
    void  SetPower(float power){ power_ = power; }  // 調整用（ノードエディタの「ジャンプ力」）

    // --- Player が当たりの結果を反映する ---
    void SetHeight(float height){ height_ = height; }
    void AddHeight(float delta){ height_ += delta; }
    void SetVelocity(float velocity){ velocity_ = velocity; }
    void SetGrounded(bool grounded){ grounded_ = grounded; }
    // 高さ groundY で止まって接地する（ふんばりの回数は持ち越す。回数まで戻すのは Land）
    void Settle(float groundY);
    // ふんばりで滞空できる時間を満タンにする（踏みつけの跳ね返り）／空の時だけ満タンにする（落下の始まり）
    void RefillFlutter(){ flutterTime_ = kFlutterTime; }
    void RefillFlutterIfEmpty(){ if ( flutterTime_ <= 0.0f ) { flutterTime_ = kFlutterTime; } }

    void Reset();

    // 上へ跳ぶ（ジャンプ・踏みつけの跳ね返り・ジャンプ台・ノックバックの小跳ね）
    void Launch(float upVelocity){ velocity_ = upVelocity; grounded_ = false; }
    // 地面（高さ groundY）に着地する。ふんばりの残りと回数もリセット
    void Land(float groundY);

    // 1フレームぶん上下速度を進める（ジャンプ開始・ふんばり・重力）。高さは動かさない
    //   pressed=SPACEを押した瞬間 / held=押している間 / locked=構え中などでジャンプ禁止
    void UpdateVelocity(bool pressed, bool held, bool locked, float dt);

    // ふんばり（長押し中は target へなめらかに近づく）か重力の1フレーム。ふんばった時 true。
    //   レール外の空中（自由落下）でも同じ式を使う
    static bool StepFlutterOrGravity(float& upVelocity, float& flutterTimeLeft, float target,
                                     float gravityAccel, bool held, float dt);
    // レール外の空中（自由落下）の上下速度を 1 フレーム進める（ふんばりの残り時間と重力は自分の物を使う。回数での減衰は無し）
    bool StepAirVelocity(float& upVelocity, bool held, float dt){
        return StepFlutterOrGravity(upVelocity, flutterTime_, kFlutterTarget, gravity_, held, dt);
    }

private:
    float height_ = 0.0f;      // 足のレール面からの高さ (m)
    float velocity_ = 0.0f;    // 上下速度 (m/s)
    bool  grounded_ = true;    // 接地しているか（空中で再ジャンプ不可）
    float power_ = 8.0f;       // ジャンプ初速 (m/s)
    float gravity_ = 25.0f;    // 重力加速度 (m/s^2)
    float flutterTime_ = 0.0f; // 滞空できる残り時間 (秒)
    int   flutterCount_ = 0;   // このジャンプ中にふんばった回数
    bool  fluttering_ = false; // 今まさにふんばり中か
};
