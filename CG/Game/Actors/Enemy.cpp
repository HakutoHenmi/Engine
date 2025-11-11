//#include "Enemy.h"
//#include "Camera.h"
//#include "LaserManager.h"
//#include <Windows.h>
//#include <algorithm>
//#include <cmath>
//
//namespace Engine {
//
//// -------------------------------------------------------------
//// 初期化
//// -------------------------------------------------------------
//void Enemy::Initialize(Renderer& renderer, ID3D12Device* device, ID3D12GraphicsCommandList* cmd) {
//	// モデルパスはプロジェクトの資産に合わせる（マージでズレやすい）
//	modelHandle_ = renderer.LoadModel(device, cmd, "Resources/cube_enemy/cube.obj");
//	markerHandle_ = renderer.LoadModel(device, cmd, "Resources/cube/cube.obj");
//	bulletModelHandle_ = renderer.LoadModel(device, cmd, "Resources/cubeEnemy_Bullet/cube.obj");
//
//	transform_.translate = {-6.0f, 0.5f, 0.0f};
//	transform_.scale = {0.5f, 0.5f, 0.5f};
//	transform_.rotate = {0.0f, 0.0f, 0.0f};
//
//	state_ = State::Patrol;
//	stateTimer_ = 0.0f;
//	moveDir_ = {1, 0, 0};
//	hp_ = 3.0f;
//	isAlive_ = true;
//	hit_ = false;
//}
//
//// -------------------------------------------------------------
//// 方向を向く（スムーズ回転）
//// -------------------------------------------------------------
//void Enemy::FaceTo(const Vector3& dir, float lerp) {
//	if (dir.x == 0 && dir.z == 0)
//		return;
//	float targetYaw = std::atan2f(dir.x, dir.z);
//	float diff = targetYaw - transform_.rotate.y;
//	while (diff > DirectX::XM_PI)
//		diff -= DirectX::XM_2PI;
//	while (diff < -DirectX::XM_PI)
//		diff += DirectX::XM_2PI;
//	transform_.rotate.y += diff * lerp;
//}
//
//// -------------------------------------------------------------
//// レーザー接触チェック（簡易）
//// -------------------------------------------------------------
//void Enemy::CheckLaserHit(const LaserManager& laserManager) {
//	// 敵中心の球とレーザー線分群の最短距離で接触判定
//	hit_ = laserManager.HitSphere(transform_.translate, radius_);
//}
//
//// -------------------------------------------------------------
//// 更新
//// -------------------------------------------------------------
//void Enemy::Update(float dt, Player& player, const std::vector<AABB>& walls, Camera& cam, LaserManager& lasers) {
//	if (!isAlive_)
//		return;
//
//	const Vector3 playerPos = player.GetTransform().translate;
//
//	// 敵本体がプレイヤーレーザーに触れたらダメージ
//	if (lasers.HitSphere(transform_.translate, radius_)) {
//		Damage(1);
//		if (!isAlive_)
//			return;
//	}
//
//	stateTimer_ += dt;
//
//	switch (state_) {
//	case State::Patrol: {
//		// プレイヤー方向へゆっくり移動
//		Vector3 toP{playerPos.x - transform_.translate.x, 0.0f, playerPos.z - transform_.translate.z};
//		float len = std::sqrt(toP.x * toP.x + toP.z * toP.z);
//		moveDir_ = (len > 1e-6f) ? Vector3{toP.x / len, 0.0f, toP.z / len} : Vector3{0, 0, 0};
//
//		transform_.translate.x += moveDir_.x * (moveSpeed_ * dt);
//		transform_.translate.z += moveDir_.z * (moveSpeed_ * dt);
//
//		FaceTo(moveDir_);
//
//		if (stateTimer_ > 1.2f) { // 一定間隔で攻撃フェーズへ
//			state_ = State::PreAttack;
//			stateTimer_ = 0.0f;
//		}
//	} break;
//
//	case State::PreAttack: {
//		// 停止＋マーカー・向きはプレイヤーに合わせる
//		Vector3 toP{playerPos.x - transform_.translate.x, 0.0f, playerPos.z - transform_.translate.z};
//		FaceTo(toP);
//		if (stateTimer_ > preTime_) {
//			state_ = State::Shoot;
//			stateTimer_ = 0.0f;
//		}
//	} break;
//
//	case State::Shoot: {
//		// 現在のプレイヤー方向に発射
//		SpawnBulletToward(playerPos);
//		state_ = State::Cooldown;
//		stateTimer_ = 0.0f;
//	} break;
//
//	case State::Cooldown: {
//		// クールダウン（向きのみ追従）
//		Vector3 toP{playerPos.x - transform_.translate.x, 0.0f, playerPos.z - transform_.translate.z};
//		FaceTo(toP);
//		if (stateTimer_ > cdTime_) {
//			state_ = State::Patrol;
//			stateTimer_ = 0.0f;
//		}
//	} break;
//	}
//
//	// ===== 弾更新 =====
//	for (auto& b : bullets_) {
//		if (!b.IsAlive())
//			continue;
//
//		// === レーザー・カメラ（シェイク）を考慮した弾更新 ===
//		b.Update(walls, lasers, cam, dt);
//
//		const Vector3 bp = b.GetTransform().translate;
//
//		// === プレイヤー命中 ===
//		const float pr = 0.6f;
//		Vector3 d{bp.x - playerPos.x, bp.y - playerPos.y, bp.z - playerPos.z};
//		float dd = d.x * d.x + d.y * d.y + d.z * d.z;
//		if (dd <= (b.Radius() + pr) * (b.Radius() + pr)) {
//			OutputDebugStringA("[EnemyBullet] Hit Player!\n");
//			player.Damage(1);
//			b.Kill();
//			continue;
//		}
//
//		// === レーザー命中（弾が消える・カメラが揺れる） ===
//		if (lasers.HitSphere(bp, b.Radius(), cam)) {
//			OutputDebugStringA("[EnemyBullet] Hit Player Laser -> bullet disappears.\n");
//			b.Kill();
//			continue;
//		}
//	}
//}
//
//// -------------------------------------------------------------
//// 描画（本体＋予備動作マーカー）
//// -------------------------------------------------------------
//void Enemy::Draw(Renderer& renderer, const Camera& cam, ID3D12GraphicsCommandList* cmd) {
//	if (!isAlive_)
//		return;
//
//	// 当たっている間だけ少し赤み（hit_ は GameScene 側などで更新してもOK）
//	Vector4 color = hit_ ? Vector4{1, 0.4f, 0.4f, 1} : Vector4{1, 1, 1, 1};
//	renderer.UpdateModelCBWithColor(modelHandle_, cam, transform_, color);
//	renderer.DrawModel(modelHandle_, cmd);
//
//	// 予備動作マーカー（頭上）
//	if (state_ == State::PreAttack) {
//		Transform m = transform_;
//		m.translate.y += 0.9f;
//		m.scale = {0.25f, 0.1f, 0.25f};
//		renderer.UpdateModelCBWithColor(markerHandle_, cam, m, {1, 0.1f, 0.1f, 1});
//		renderer.DrawModel(markerHandle_, cmd);
//	}
//}
//
//// -------------------------------------------------------------
//// 弾の描画
//// -------------------------------------------------------------
//void Enemy::DrawBullets(Renderer& renderer, const Camera& cam, ID3D12GraphicsCommandList* cmd) {
//	UINT slot = 0;
//	for (auto& b : bullets_) {
//		if (!b.IsAlive())
//			continue;
//		b.SetModelHandle(bulletModelHandle_);
//		b.Draw(renderer, cam, cmd, slot++);
//	}
//}
//
//// -------------------------------------------------------------
//// ダメージ
//// -------------------------------------------------------------
//void Enemy::Damage(int dmg) {
//	if (!isAlive_)
//		return;
//	hp_ -= float(dmg);
//	if (hp_ <= 0.0f) {
//		isAlive_ = false;
//		OutputDebugStringA("[Enemy] Destroyed!\n");
//	}
//}
//
//// -------------------------------------------------------------
//// AABB（球の外接立方体）
//// -------------------------------------------------------------
//AABB Enemy::GetAABB() const {
//	AABB box;
//	const Vector3& t = transform_.translate;
//	float r = radius_;
//	box.min = {t.x - r, t.y - r, t.z - r};
//	box.max = {t.x + r, t.y + r, t.z + r};
//	return box;
//}
//
//// -------------------------------------------------------------
//// 弾のスポーン（マズルオフセット / 少し上から）
//// -------------------------------------------------------------
//void Enemy::SpawnBulletToward(const Vector3& target) {
//	// 空き弾を再利用、なければ新規
//	EnemyBullet* use = nullptr;
//	for (auto& b : bullets_) {
//		if (!b.IsAlive()) {
//			use = &b;
//			break;
//		}
//	}
//	if (!use) {
//		bullets_.push_back({});
//		use = &bullets_.back();
//	}
//
//	// 方向（XZ）
//	Vector3 dir{target.x - transform_.translate.x, 0.0f, target.z - transform_.translate.z};
//	float len = std::sqrt(dir.x * dir.x + dir.z * dir.z);
//	if (len > 1e-6f) {
//		dir.x /= len;
//		dir.z /= len;
//	} else {
//		dir = {1, 0, 0};
//	}
//
//	// マズルオフセット：敵半径 + 弾半径 + 余裕
//	const float bulletRadius = 0.25f;
//	const float muzzleOffset = radius_ + bulletRadius + 0.1f;
//
//	Vector3 start = transform_.translate;
//	start.x += dir.x * muzzleOffset;
//	start.z += dir.z * muzzleOffset;
//	start.y += 0.25f; // 少し上げる（床接触回避）
//
//	use->SetModelHandle(bulletModelHandle_);
//	use->Spawn(start, dir, bulletSpeed_, bulletLife_);
//}
//
//} // namespace Engine
