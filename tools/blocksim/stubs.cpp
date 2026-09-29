// ゲーム本体の Player / BlockSystem をそのまま動かすための差し替え（描画・入力デバイスなし）
#include "engine/base/Input.h"
#include "game/stage/BlockRenderer.h"
#include "game/stage/BlockWorldShapes.h"
#include <cstring>

// ---- 入力：シミュレーションが毎フレーム押しているキーを決める ----
unsigned char g_keyNow[256] = {};
unsigned char g_keyPrev[256] = {};

Input* Input::GetInstance(){ static Input instance; return &instance; }
bool Input::Pushkey(BYTE key){ return g_keyNow[key] != 0; }
bool Input::Triggerkey(BYTE key){ return g_keyNow[key] != 0 && g_keyPrev[key] == 0; }

// ---- ブロックの描画・ワールドの箱は使わない（描画クラスは空の型で置き換える）----
class InstancedGroup {};
class Obj3d {};
BlockRenderer::BlockRenderer() = default;
BlockRenderer::~BlockRenderer() = default;
void BlockRenderer::Initialize(uint32_t){}
void BlockRenderer::Rebuild(const std::vector<PlacedBlock>&, const BlockGrid&){}
void BlockRenderer::Update(const std::vector<PlacedBlock>&, const std::vector<SplineRail>&){}
void BlockRenderer::Draw(const Camera*){}

bool BlockWorldShapes::MakeBox(const std::vector<SplineRail>&, const PlacedBlock&, Box&){ return false; }
void BlockWorldShapes::DrawWire(const Box&, const Vector4&, float){}
void BlockWorldShapes::Rebuild(const std::vector<PlacedBlock>&, const std::vector<SplineRail>&){}
bool BlockWorldShapes::SweepSphere(const Vector3&, const Vector3&, float, Vector3*) const{ return false; }
bool BlockWorldShapes::Raycast(const Vector3&, const Vector3&, Hit&) const{ return false; }
void BlockWorldShapes::DrawAll(const Vector4&) const{}
