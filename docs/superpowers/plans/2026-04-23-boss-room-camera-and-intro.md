# Boss Room Camera And Intro Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** ボス部屋の雑魚敵が遠距離で止まらないようにし、ボス戦カメラをプレイヤー主役に調整し、ボス導入演出を見上げ開始の4段構成へ強化する。

**Architecture:** `Enemy` にボス部屋専用の追跡距離・探索猶予を追加し、`EnemyManager` からボス部屋フラグを渡す。`GamePlayScene` のボス戦カメラは中点固定からプレイヤー主役のクランプ付き構図へ置き換え、導入演出は `BossManager::IntroCameraState` を拡張して `SkyLook -> BossReveal -> BossDropFollow -> BossLandImpact -> ToBattle` の順で進行させる。

**Tech Stack:** C++20, 既存ゲームループ, `GamePlayScene`, `Enemy`, `EnemyManager`, `BossManager`

---

### Task 1: ボス部屋での雑魚追跡を安定化

**Files:**
- Modify: `project/game/enemy/Enemy.h`
- Modify: `project/game/enemy/Enemy.cpp`
- Modify: `project/game/enemy/EnemyManager.cpp`
- Test: `project/game/enemy/Enemy.cpp`

- [ ] **Step 1: 追跡パラメータ受け口を追加**

```cpp
// Enemy.h
void SetBossRoomBehavior(bool isBossRoom) { isBossRoom_ = isBossRoom; }

bool isBossRoom_ = false;
float bossRoomChaseRange_ = 30.0f;
int bossRoomInvestigateFrames_ = 300;
```

- [ ] **Step 2: ボス部屋用の有効追跡距離と探索時間を使う**

```cpp
// Enemy.cpp
const float activeChaseRange = isBossRoom_ ? bossRoomChaseRange_ : chaseRange_;
const int investigateFrames = isBossRoom_ ? bossRoomInvestigateFrames_ : 150;

if (playerDist <= activeChaseRange) {
    lastKnownPlayerPos_ = playerPos_;
    investigateTimer_ = investigateFrames;
} else if (investigateTimer_ > 0) {
    investigateTimer_--;
}
```

- [ ] **Step 3: 追跡更新でも同じ距離を参照**

```cpp
// Enemy.cpp
const float activeChaseRange = isBossRoom_ ? bossRoomChaseRange_ : chaseRange_;
if (Length(playerPos_ - pos_) > activeChaseRange && investigateTimer_ > 0) {
    targetPos = lastKnownPlayerPos_;
}
```

- [ ] **Step 4: EnemyManager からボス部屋フラグを渡す**

```cpp
// EnemyManager.cpp
const bool isBossRoom = mapManager->IsBossMap();
enemy->SetBossRoomBehavior(isBossRoom);
```

- [ ] **Step 5: ビルドで確認**

Run: `& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' project\CG2_00_01.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`
Expected: `0 エラー`

### Task 2: ボス戦カメラをプレイヤー主役に変更

**Files:**
- Modify: `project/game/scene/GamePlayScene.cpp`
- Test: `project/game/scene/GamePlayScene.cpp`

- [ ] **Step 1: 通常ボス戦カメラの中点基準を置き換える**

```cpp
Vector3 toBoss = {
    bossPos.x - playerPos_.x,
    0.0f,
    bossPos.z - playerPos_.z
};

float distance = Length(toBoss);
Vector3 bossDir = distance > 0.01f ? Normalize(toBoss) : Vector3{ 0.0f, 0.0f, 1.0f };

float sideLead = (std::min)(distance * 0.18f, 4.0f);
float back = 14.0f;
float height = 15.0f;

Vector3 focus = {
    playerPos_.x + bossDir.x * sideLead,
    playerPos_.y + 2.0f,
    playerPos_.z + bossDir.z * sideLead
};

targetPos = {
    focus.x,
    focus.y + height,
    focus.z - back
};
```

- [ ] **Step 2: 距離依存の引きすぎをクランプ**

```cpp
float extraBack = (std::min)(distance * 0.25f, 6.0f);
float extraHeight = (std::min)(distance * 0.12f, 4.0f);
targetPos.y += extraHeight;
targetPos.z -= extraBack;
```

- [ ] **Step 3: 注視角も少しだけボス方向へ振る**

```cpp
targetRot = {
    0.88f - (std::min)(distance * 0.01f, 0.10f),
    0.0f,
    0.0f
};
```

- [ ] **Step 4: ビルドで確認**

Run: `& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' project\CG2_00_01.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`
Expected: `0 エラー`

### Task 3: ボス導入演出を5段へ拡張

**Files:**
- Modify: `project/game/enemy/BossManager.h`
- Modify: `project/game/enemy/BossManager.cpp`
- Modify: `project/game/scene/GamePlayScene.cpp`
- Test: `project/game/scene/GamePlayScene.cpp`

- [ ] **Step 1: IntroCameraState を拡張**

```cpp
enum class IntroCameraState {
    None,
    SkyLook,
    BossReveal,
    BossDropFollow,
    BossLandImpact,
    ToBattle
};
```

- [ ] **Step 2: StartBossIntro を新しい開始状態へ変更**

```cpp
void BossManager::StartBossIntro() {
    isBossIntroPlaying_ = true;
    bossIntroCameraState_ = IntroCameraState::SkyLook;
    bossIntroTimer_ = 35;
    bossCardRainTimer_ = bossCardRainInterval_;
}
```

- [ ] **Step 3: GamePlayScene の導入カメラ分岐を段階化**

```cpp
// SkyLook
targetPos = { playerPos_.x, playerPos_.y + 6.0f, playerPos_.z - 4.0f };
targetRot = { 1.20f, 0.0f, 0.0f };

// BossReveal
targetPos = { bossPos.x, bossPos.y - 4.0f, bossPos.z - 14.0f };
targetRot = { 0.95f, 0.0f, 0.0f };

// BossDropFollow
targetPos = { bossPos.x, bossPos.y + 7.0f, bossPos.z - 11.0f };
targetRot = { 0.55f, 0.0f, 0.0f };

// BossLandImpact
targetPos = { bossPos.x, bossPos.y + 5.0f, bossPos.z - 8.5f };
targetRot = { 0.42f, 0.0f, 0.0f };
```

- [ ] **Step 4: 着地インパクトに短いワンアクションを入れる**

```cpp
float impactT = 1.0f - static_cast<float>(bossIntroTimer) / 18.0f;
float punch = std::sinf(impactT * 3.14159f);
targetPos.y -= punch * 1.2f;
targetPos.z += punch * 1.4f;
targetRot.x -= punch * 0.10f;
```

- [ ] **Step 5: 戦闘カメラ復帰へ滑らかに接続**

```cpp
if (bossIntroTimer <= 0) {
    bossManager_->SetBossIntroCameraState(BossManager::IntroCameraState::ToBattle);
    bossManager_->SetBossIntroTimer(32);
}
```

- [ ] **Step 6: ビルドで確認**

Run: `& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' project\CG2_00_01.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`
Expected: `0 エラー`

### Task 4: 最終確認

**Files:**
- Modify: `project/game/scene/GamePlayScene.cpp`
- Modify: `project/game/enemy/Enemy.cpp`
- Test: `project/CG2_00_01.vcxproj`

- [ ] **Step 1: 導入中停止仕様が壊れていないか確認**

```cpp
const bool isBossIntroPlaying = bossManager_ ? bossManager_->IsBossIntroPlaying() : false;
if (enemyManager_ && !isBossIntroPlaying) {
    enemyManager_->Update(player, &cardPickupManager_, mapManager_.get(), boss, targetPos);
}
```

- [ ] **Step 2: 差分を見直す**

Run: `git diff -- project/game/enemy/Enemy.h project/game/enemy/Enemy.cpp project/game/enemy/EnemyManager.cpp project/game/enemy/BossManager.h project/game/enemy/BossManager.cpp project/game/scene/GamePlayScene.cpp`
Expected: ボス部屋AI、戦闘カメラ、導入演出の差分のみ

- [ ] **Step 3: 最終ビルド**

Run: `& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' project\CG2_00_01.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`
Expected: `ビルドに成功しました。`
