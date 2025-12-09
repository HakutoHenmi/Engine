#include "GameScene.h"
#include "Collision.h"
#include "imgui.h"
#include <Windows.h>
#include <Xinput.h>
#include <cmath>
#include <random>
#pragma comment(lib, "xinput9_1_0.lib")

using namespace DirectX;

namespace {

// Windows の ShowCursor は内部カウンタ制御なので、
// 目的の状態になるまで回して強制的に合わせる
void ForceCursorVisible(bool visible) {
	if (visible) {
		// カウンタが 0 以上になるまで TRUE を投げる
		while (ShowCursor(TRUE) < 0) {
			// 何もしない
		}
	} else {
		// カウンタが -1 以下になるまで FALSE を投げる
		while (ShowCursor(FALSE) >= 0) {
			// 何もしない
		}
	}
}

} // namespace

namespace Engine {

// ==== カメラの前方向ベクトルを取るヘルパ ====
// cam.View() の逆行列から「前向き（+Z）」を取り出す
static Vector3 GetCameraForward(const Camera& cam) {
	using namespace DirectX;

	XMMATRIX view = cam.View();
	XMMATRIX inv = XMMatrixInverse(nullptr, view); // view の逆＝カメラのワールド変換

	XMFLOAT3 f;
	f.x = inv.r[2].m128_f32[0]; // 前方向 (row 2)
	f.y = inv.r[2].m128_f32[1];
	f.z = inv.r[2].m128_f32[2];

	XMVECTOR v = XMLoadFloat3(&f);
	v = XMVector3Normalize(v);
	XMStoreFloat3(&f, v);

	return Vector3{f.x, f.y, f.z};
}

void GameScene::Initialize(WindowDX* dx) {
	dx_ = dx;

	sprite_ = Engine::SpriteRenderer::Instance();

	end_ = false;
	next_.clear();
	waitRelease_ = true;
	prevPressed_ = false;
	gridVisible_ = true;

	// ===============================
	// 1. Renderer / Input
	// ===============================
	renderer_.Initialize(*dx_);
	input_.Initialize(dx_->GetHInstance(), dx_->GetHwnd());

	// ===============================
	// 2. Camera
	// ===============================
	const float aspect = 1280.0f / 720.0f;
	camPlay_.Initialize();
	camDebug_.Initialize();
	camPlay_.SetProjection(XMConvertToRadians(60.0f), aspect, 0.1f, 500.0f);
	camDebug_.SetProjection(XMConvertToRadians(60.0f), aspect, 0.1f, 500.0f);
	camPlay_.SetPosition({0.0f, 10.0f, -20.0f});
	camPlay_.LookAt({0.0f, 0.0f, 0.0f}, {0, 1, 0});
	camDebug_.SetPosition({0.0f, 20.0f, -30.0f});
	camDebug_.LookAt({0.0f, 0.0f, 0.0f}, {0, 1, 0});
	activeCam_ = &camPlay_;

	// ===============================
	// 3. モデル / プレイヤー
	// ===============================
	player_.Initialize(renderer_, dx_->Dev(), dx_->List());

	// --- 剣モデルを読み込み・プレイヤーにセット ---
	int sword = renderer_.LoadModel(dx_->Dev(), dx_->List(), "Resources/weapons/sword.obj");
	player_.SetSwordModel(sword);

	// パーティクル
	sparks_.Initialize(renderer_, *dx_, 6000); // 2000枚まで
	sparks_.SetPosition({0, 1.0f, 0});
	auto& p = sparks_.Params();
	p.maxOnce = 256;
	p.useAdditive = true;
	p.initColor = {0.3f, 0.2f, 0.1f, 0.5f};
	p.initScaleMin = {0.05f, 0.05f, 0.05f};
	p.initScaleMax = {0.10f, 0.10f, 0.10f};
	p.lifeMin = 0.30f;
	p.lifeMax = 0.55f;
	p.initVelMin = {-1.6f, 2.8f, -1.6f};
	p.initVelMax = {+1.6f, 5.0f, +1.6f};
	p.spawnPosJitter = {0.40f, 0.60f, 0.40f};

	// UIの初期化

	// テストスプライト
	if (sprite_) {
		testSprite_.Initialize(sprite_, L"Resources/UI/uvChecker.png");
		testSprite_.SetPosition(50.0f, 50.0f);  // 左上あたり
		testSprite_.SetSize(200.0f, 100.0f);    // 幅200, 高さ100
		testSprite_.SetColor(1.0f, 1.0f, 1.0f); // 白
	}

	// --- ボス生成 ---
	boss_ = std::make_unique<Game::Boss>();
	boss_->Initialize(renderer_, *dx_);

	// ★ボスのルート位置（ステージ中央など）
	Engine::Vector3 bossRoot{0.0f, 0.0f, 0.0f};

	// ★この XZ でのボクセル地形の高さ(Y)を取得
	float groundY = renderer_.TerrainHeightAt(bossRoot.x, bossRoot.z);

	// 必要なら +0.1f とかで少し浮かせてもOK
	bossRoot.y = groundY; // または groundY + 0.1f;

	// ルート位置（支点）を設定
	boss_->SetPosition(bossRoot);

	// 地形を凹ませるコールバック
	boss_->SetTerrainHitCallback([this](const Game::TerrainHitInfo& info) { ApplyBossHitToTerrain_(info); });

	// ★XZ 位置からボクセル地形の高さを返すコールバック
	boss_->SetTerrainHeightCallback([this](const Engine::Vector3& pos) { return renderer_.TerrainHeightAt(pos.x, pos.z); });

	// 巨大地面をロード
	outerGroundHandle_ = renderer_.LoadModel(dx_->Dev(), dx_->List(), "Resources/Plane/Plane.obj");

	// Transform 設定
	outerGroundTf_.scale = {6000.0f, 6000.0f, 6000.0f};
	outerGroundTf_.rotate = {XMConvertToRadians(-90.0f), 0.0f, 0.0f};
	outerGroundTf_.translate = {0.0f, -5.0f, 0.0f};

	// ---- 水面の生成 ----
	Engine::WaterSurfaceDesc wd;
	wd.sizeX = 1000.0f; // 島を囲むくらいに調整
	wd.sizeZ = 1000.0f;
	wd.tessX = 200;
	wd.tessZ = 200;
	wd.height = -2.0f; // 海面の高さ（好みで調整）

	water_ = std::make_unique<Engine::WaterSurface>();
	water_->Initialize(*dx_, wd);

	// ===============================
	// 5. Grid 初期化（Stage 依存なし版）
	// ===============================
	{
		// ボクセル地形のサイズに合わせて、適当なグリッドを引く
		const float pitchX = 1.0f; // X 方向の格子間隔
		const float pitchZ = 1.0f; // Z 方向の格子間隔
		const int cols = 64;       // X 方向の本数
		const int rows = 64;       // Z 方向の本数

		const float minX = -0.5f * cols * pitchX;
		const float minZ = -0.5f * rows * pitchZ;

		renderer_.InitGrid(*dx_, (std::max)(cols, rows), pitchX, 0.0f, minX, minZ);
	}

	camPos_ = DirectX::XMFLOAT3{0.0f, 10.0f, -20.0f};
	camPlay_.SetPosition(camPos_);
	firstMouse_ = true;

	// ==== FPS 計測初期化 ====
	fpsLastTime_ = std::chrono::steady_clock::now();
	fpsFrameCount_ = 0;
	fps_ = 0.0f;

	// ==== カーソル固定 ＋ 非表示 ====
	cursorFree_ = false;
	ForceCursorVisible(cursorFree_);

	// 画面領域を取得してゲームウィンドウ内部に固定
	ClipCursor(NULL);
	RECT rect;
	GetClientRect(dx_->GetHwnd(), &rect);
	ClientToScreen(dx_->GetHwnd(), (POINT*)&rect.left);
	ClientToScreen(dx_->GetHwnd(), (POINT*)&rect.right);
	ClipCursor(&rect);
}

void GameScene::Update() {
	input_.Update();
	DrawImGui_();

	// ==== F3 押し中だけカーソル解放 ====
	{
		bool F3Now = (GetAsyncKeyState(VK_F3) & 0x8000) != 0;

		if (F3Now && !cursorFree_) {
			// ゲームモード → フリーモードに入る
			cursorFree_ = true;

			// 範囲ロック解除（画面全体でOKなら NULL）
			ClipCursor(NULL);
		} else if (!F3Now && cursorFree_) {
			// フリーモード → ゲームモードに戻る
			cursorFree_ = false;

			// 再度ウィンドウ内にロック
			RECT rect;
			GetClientRect(dx_->GetHwnd(), &rect);
			ClientToScreen(dx_->GetHwnd(), (POINT*)&rect.left);
			ClientToScreen(dx_->GetHwnd(), (POINT*)&rect.right);
			ClipCursor(&rect);

			// 戻した瞬間に一旦中心に置いとく
			POINT center;
			center.x = (rect.right + rect.left) / 2;
			center.y = (rect.bottom + rect.top) / 2;
			SetCursorPos(center.x, center.y);
		}
	}

	// カーソル表示状態を強制セット
	ForceCursorVisible(cursorFree_);

	// ==== ESC でアプリ終了 ====
	if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
		// 終了前に必ず元の状態に戻す
		ForceCursorVisible(true); // カーソルを確実に表示状態に
		ClipCursor(NULL);         // マウスの移動制限を解除

		PostQuitMessage(0); // メインループに WM_QUIT を送る
		return;             // 以降の更新はスキップ
	}

	// ==== FPS 計測 ====
	fpsFrameCount_++;
	auto now = std::chrono::steady_clock::now();
	float sec = std::chrono::duration<float>(now - fpsLastTime_).count();
	// 0.5秒ごとくらいに更新（チラつき防止）
	if (sec >= 0.5f) {
		fps_ = fpsFrameCount_ / sec;
		fpsFrameCount_ = 0;
		fpsLastTime_ = now;
	}

	// === F1：カメラ切替 ===
	bool nowF1 = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
	if (nowF1 && !prevF1_) {
		useDebugCam_ = !useDebugCam_;
		activeCam_ = useDebugCam_ ? &camDebug_ : &camPlay_;
	}
	prevF1_ = nowF1;

	// === F2：グリッド切替 ===
	if (input_.Trigger(DIK_F2)) {
		gridVisible_ = !gridVisible_;
	}

	// ==== マウス中央固定 ====
	if (!cursorFree_) {
		POINT center;
		RECT rc;
		GetClientRect(dx_->GetHwnd(), &rc);
		center.x = (rc.right - rc.left) / 2;
		center.y = (rc.bottom - rc.top) / 2;
		ClientToScreen(dx_->GetHwnd(), &center);
		SetCursorPos(center.x, center.y);

		// === プレイヤー更新 ===
		Vector3 prevPos = player_.GetPos(); // 移動前

		// ★★ ヒットストップ中はプレイヤー更新を止める ★★
		if (hitStopTimer_ <= 0.0f) {
			player_.Update(*activeCam_, input_);
		}
		Vector3 newPos = player_.GetPos(); // 移動後

		// --- ボス更新（プレイヤー位置を渡す）---
		Engine::Vector3 playerPos = player_.GetPos();

		// ★ 生 dt（固定）
		const float baseDt = 1.0f / 60.0f;

		// ★ ヒットストップタイマー更新
		if (hitStopTimer_ > 0.0f) {
			hitStopTimer_ -= baseDt;
			if (hitStopTimer_ < 0.0f) {
				hitStopTimer_ = 0.0f;
			}
		}

		// ★ ゲーム用 dt（ヒットストップを反映）
		float gameDt = baseDt;
		if (hitStopTimer_ > 0.0f) {
			// 完全停止したいなら → gameDt = 0.0f;
			gameDt *= hitStopScale_; // 0.05f くらいだと「ほぼ止まって見えるスロー」
		}

		debugBaseDt_ = baseDt;
		debugGameDt_ = gameDt;

		if (boss_) {
			boss_->Update(gameDt, playerPos);
		}

		if (water_) {
			water_->Update(gameDt);
		}

		// 位置反映（ステージ壁コリジョンは一旦無し）
		player_.SetPos(newPos);

		// === マウス左クリック：攻撃 / 右クリック：緊急回避 ===

		// 現在のマウスボタン状態
		bool mouseLeftNow = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
		bool mouseRightNow = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

		// 立ち上がり検出（「押した瞬間」）
		static bool prevMouseLeft = false;
		static bool prevMouseRight = false;

		bool leftPressed = (mouseLeftNow && !prevMouseLeft);    // 攻撃開始
		bool rightPressed = (mouseRightNow && !prevMouseRight); // 緊急回避開始

		prevMouseLeft = mouseLeftNow;
		prevMouseRight = mouseRightNow;

		// --- 左クリック：攻撃（ここではエフェクト＋後でPlayer側と連携） ---
		if (leftPressed) {
			// 攻撃のエフェクト：プレイヤー前方に火花を出す
			Engine::Vector3 pos = player_.GetPos();
			const float cy = std::cos(camYaw_), sy = std::sin(camYaw_);
			pos.x += cy * 0.8f;
			pos.z += sy * 0.8f;
			pos.y += 0.6f;
			sparks_.SetPosition(pos);

			// 一発ドバッと
			sparks_.Burst(512);

			// TODO: 実際の攻撃は Player 側で処理するなら、
			// ここで「攻撃開始フラグ」を Player に伝えるようにしてください。
		}

		// --- 右クリック：緊急回避（ロール・ステップなど） ---
		if (rightPressed) {
			// 緊急回避用のエフェクト（少し弱めに）
			Engine::Vector3 pos = player_.GetPos();
			pos.y += 4.0f;
			sparks_.SetPosition(pos);
			sparks_.Burst(256);

			// TODO: 実際の回避ロジックは Player 側で処理するなら、
			// ここで Player に「回避入力」を伝えるようにしてください。
		}

		// === TPS遅延カメラ（相対マウス入力 / 画面端でも止まらない） ===
		if (!useDebugCam_) {
			using namespace DirectX;

			// --- ズーム（ホイール） ---
			float wheel = input_.GetMouseWheelDelta();
			if (wheel != 0.0f) {
				camDist_ -= wheel * 1.0f; // 上で近づく
				camDist_ = std::clamp(camDist_, 3.0f, 20.0f);
			}

			// ★ 実際に使う距離（クリックで寄せるけど、攻撃中は固定）
			float useDist = camDist_;

			// --- マウス相対移動（攻撃中でも常にカメラ回転） ---
			const float dx = input_.GetMouseDeltaX();
			const float dy = input_.GetMouseDeltaY();

			const float mouseSensitivity = 0.002f;

			camYaw_ -= dx * mouseSensitivity;
			camPitch_ += dy * mouseSensitivity;

			// カメラの上下回転制限
			camPitch_ = std::clamp(camPitch_, -1.2f, 1.2f);

			// --- 追従対象座標 ---
			// const Vector3 playerPos = player_.GetPos();

			// 目標カメラ位置
			const float cx = playerPos.x - std::cos(camYaw_) * std::cos(camPitch_) * useDist;
			const float cz = playerPos.z - std::sin(camYaw_) * std::cos(camPitch_) * useDist;
			const float cy = playerPos.y + std::sin(camPitch_) * useDist + camHeight_;
			XMFLOAT3 targetCamPos{cx, cy, cz};

			// ================================
			// カメラ衝突：一旦「壁との衝突」は無しで、地形との接触だけ見る
			// ================================
			const float camRadius = 0.30f; // カメラ球半径

			Engine::Vector3 eye{playerPos.x, playerPos.y + 1.0f, playerPos.z};
			Engine::Vector3 desired{targetCamPos.x, targetCamPos.y, targetCamPos.z};

			// とりあえず壁コリジョンなしで、そのまま希望位置へ
			Engine::Vector3 allowed = desired;

			// ===== ボクセル地形との衝突（地面にめり込まないように） =====
			{
				// この XZ での地形の高さを取得
				float terrainY = renderer_.TerrainHeightAt(allowed.x, allowed.z);

				// カメラ球の「底」が地面より下に行かないようにクランプ
				if (allowed.y - camRadius < terrainY + 0.05f) {
					allowed.y = terrainY + 0.05f + camRadius;
				}
			}

			// --- 遅延追従（衝突解決後の位置へ） ---
			XMFLOAT3 noClipTarget{allowed.x, allowed.y, allowed.z};
			const float followSpeed = 0.15f;
			camPos_.x += (noClipTarget.x - camPos_.x) * followSpeed;
			camPos_.y += (noClipTarget.y - camPos_.y) * followSpeed;
			camPos_.z += (noClipTarget.z - camPos_.z) * followSpeed;

			// 反映と注視
			camPlay_.SetPosition(camPos_);
			XMFLOAT3 lookAt{playerPos.x, playerPos.y + 1.0f, playerPos.z};
			camPlay_.LookAt(lookAt, XMFLOAT3{0, 1, 0});
		}

		// === シーン遷移（Gキーで切替） ===
		if (input_.Trigger(DIK_G)) {
			end_ = true;
			next_ = "Result";
		}

		sparks_.Update(1.0f / 60.0f);
	}
}

void GameScene::DrawImGui_() {
	// #ifdef _DEBUG
	//  ==== ImGui FPS カウンター（左上） ====
	ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always); // 画面左上固定
	ImGui::SetNextWindowBgAlpha(0.35f);                        // 半透明背景

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 6));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));        // 白文字
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.6f)); // 濃い背景

	ImGuiWindowFlags flags =
	    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;

	ImGui::Begin("FPSOverlay", nullptr, flags);

	ImGui::SetWindowFontScale(1.4f);
	ImGui::Text("FPS : %5.1f", fps_);
	ImGui::SetWindowFontScale(1.0f);

	ImGui::End();

	ImGui::PopStyleColor(2);
	ImGui::PopStyleVar();

	// ==== 操作説明（右上） ====
	{
		ImGuiIO& io = ImGui::GetIO();

		// 右上に固定（pivot を (1,0) にして右端基準にする）
		const ImVec2 margin(10.0f, 10.0f);
		ImVec2 pos(io.DisplaySize.x - margin.x, margin.y);
		ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(1.0f, 0.0f));
		ImGui::SetNextWindowBgAlpha(0.35f);

		ImGuiWindowFlags helpFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;

		ImGui::Begin("操作説明", nullptr, helpFlags);

		ImGui::Text("操作説明");
		ImGui::Separator();
		ImGui::Text("1. WASD で移動");
		ImGui::Text("2. マウスで視点操作");
		ImGui::Text("3. Space キーでジャンプ");
		ImGui::Text("4. 左クリックで剣を振る");
		ImGui::Text("5. 右クリックで緊急回避");
		ImGui::Text("6. マウスホイールでカメラ距離の調整");
		ImGui::Text("7. F2 でグリッド線の ON/OFF");
		ImGui::Text("8. ESC でゲーム終了 ");

		ImGui::End();
	}
	// #endif // _DEBUG
}

void GameScene::ApplyBossHitToTerrain_(const Game::TerrainHitInfo& info) {
	char buf[256];
	std::snprintf(buf, sizeof(buf), "[BossHit] pos=(%.2f, %.2f, %.2f), r=%.2f, depth=%.2f\n", info.position.x, info.position.y, info.position.z, info.radius, info.depth);
	OutputDebugStringA(buf);

	// ★ボスの着弾位置を GPU ボクセル地形に凹みとして登録
	DirectX::XMFLOAT3 pos{info.position.x, info.position.y, info.position.z};
	renderer_.AddTerrainDent(pos, info.radius, info.depth);
}

void GameScene::Draw() {
	ID3D12GraphicsCommandList* cmd = dx_->List();

	// フレーム開始
	renderer_.BeginFrame(cmd);

	// ここで Compute
	renderer_.RebuildVoxelIfNeeded(dx_->List());
	renderer_.DrawVoxel(dx_->List(), *activeCam_);

	// 地面（Plane.obj）を描画
	if (outerGroundHandle_ >= 0) {
		renderer_.UpdateModelCBWithColor(outerGroundHandle_, *activeCam_, outerGroundTf_, {1, 1, 1, 1});
		renderer_.DrawModel(outerGroundHandle_, dx_->List());
	}

	// ---- 水面 ----
	if (water_) {
		water_->Draw(cmd, *activeCam_);
	}

	// 既存の描画
	if (gridVisible_) {
		renderer_.DrawGrid(*activeCam_, cmd);
	}

	player_.Draw(renderer_, *activeCam_, cmd);

	sparks_.Draw(cmd, *activeCam_);

	if (boss_) {
		boss_->Draw(renderer_, dx_->List(), *activeCam_);
	}

	renderer_.DrawSkybox(*activeCam_, cmd);

	testSprite_.Draw();

	renderer_.EndFrame(cmd);
}

} // namespace Engine
