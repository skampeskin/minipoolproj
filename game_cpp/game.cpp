
#include <cassert>
#include <cmath>
#include <array>

#include "../framework/scene.hpp"
#include "../framework/game.hpp"
#include "../framework/engine.hpp"


//-------------------------------------------------------
//	Basic Vector2 class
//-------------------------------------------------------

class Vector2
{
public:
	float x = 0.f;
	float y = 0.f;

	constexpr Vector2() = default;
	constexpr Vector2(float vx, float vy);
	constexpr Vector2(Vector2 const& other) = default;
	Vector2 operator+=(Vector2 const& other) {
		x += other.x;
		y += other.y;
		return *this;
	}
	Vector2 operator+(Vector2 const& other) const {
		Vector2 v = *this;
		return v += other;
	}
	Vector2 operator-=(Vector2 const& other) {
		x -= other.x;
		y -= other.y;
		return *this;
	}
	Vector2 operator-(Vector2 const& other) const {
		Vector2 v = *this;
		return v -= other;
	}
	Vector2 operator*=(float const& c) {
		x *= c;
		y *= c;
		return *this;
	}
	Vector2 operator*(float const& c) const {
		Vector2 v = *this;
		return v *= c;
	}
	operator bool() const {
		if (x == 0 && y == 0) {
			return false;
		}
		return true;
	}
};
float Abs(const Vector2& v) {
	return sqrt(v.x * v.x + v.y * v.y);
}


constexpr Vector2::Vector2(float vx, float vy) :
	x(vx),
	y(vy)
{
}


//-------------------------------------------------------
//	game parameters
//-------------------------------------------------------

namespace Params
{
	namespace System
	{
		constexpr int targetFPS = 60;
	}

	namespace Table
	{
		constexpr float width = 15.f;
		constexpr float height = 8.f;
		constexpr float pocketRadius = 0.4f;
		// corner pocket are moved a bit because balls don't fit otherwise
		static constexpr std::array< Vector2, 6 > pocketsPositions =
		{
			Vector2{ -0.5f * width + 0.1f, -0.5f * height + 0.1f },
			Vector2{ 0.f, -0.5f * height },
			Vector2{ 0.5f * width - 0.1f, -0.5f * height + 0.1f },
			Vector2{ -0.5f * width + 0.1f, 0.5f * height - 0.1f },
			Vector2{ 0.f, 0.5f * height },
			Vector2{ 0.5f * width - 0.1f, 0.5f * height - 0.1f}
		};

		static constexpr std::array< Vector2, 7 > ballsPositions =
		{
			// player ball
			Vector2(-0.3f * width, 0.f),
			// other balls
			Vector2(0.2f * width, 0.f),
			Vector2(0.25f * width, 0.05f * height),
			Vector2(0.25f * width, -0.05f * height),
			Vector2(0.3f * width, 0.1f * height),
			Vector2(0.3f * width, 0.f),
			Vector2(0.3f * width, -0.1f * height)
		};
	}

	namespace Ball
	{
		constexpr float radius = 0.3f;
		constexpr float friction = 0.01f;
	}

	namespace Shot
	{
		constexpr float chargeTime = 1.f;
	}
}


//-------------------------------------------------------
//	Table logic
//-------------------------------------------------------

class Table
{
public:
	Table() = default;
	Table(Table const&) = delete;

	void init();
	void deinit();
	void update(const std::array<Vector2, 7>&);
	void remove(int);


private:
	std::array< Scene::Mesh*, 6 > pockets = {};
	std::array< Scene::Mesh*, 7 > balls = {};
};


void Table::init()
{
	for (int i = 0; i < 6; i++)
	{
		assert(!pockets[i]);
		pockets[i] = Scene::createPocketMesh(Params::Table::pocketRadius);
		Scene::placeMesh(pockets[i], Params::Table::pocketsPositions[i].x, Params::Table::pocketsPositions[i].y, 0.f);
	}

	for (int i = 0; i < 7; i++)
	{
		assert(!balls[i]);
		balls[i] = Scene::createBallMesh(Params::Ball::radius);
		Scene::placeMesh(balls[i], Params::Table::ballsPositions[i].x, Params::Table::ballsPositions[i].y, 0.f);
	}
}


void Table::deinit()
{
	for (Scene::Mesh* mesh : pockets)
		Scene::destroyMesh(mesh);
	for (Scene::Mesh* mesh : balls)
		if (mesh) {
			Scene::destroyMesh(mesh);
		}
	pockets = {};
	balls = {};
}


void Table::update(const std::array<Vector2, 7>& balls_positions) {
	for (int i = 0; i < 7; i++)
	{
		if (balls[i]) {
			Scene::placeMesh(balls[i], balls_positions[i].x, balls_positions[i].y, 0.f);
		}
	}
}


void Table::remove(int i) {
	if (!balls[i]) {
		return;
	}
	Scene::destroyMesh(balls[i]);
	balls[i] = nullptr;
}


//-------------------------------------------------------
//	game public interface
//-------------------------------------------------------

namespace Game
{
	Table table;

	bool isChargingShot = false;
	float shotChargeProgress = 0.f;
	std::array<Vector2, 7> cur_ball_positions;
	std::array<Vector2, 7> cur_ball_speeds;
	std::array<bool, 7> scored;
	std::array<std::array<int, 7>, 7> last_collision;


	void init()
	{
		Engine::setTargetFPS(Params::System::targetFPS);
		Scene::setupBackground(Params::Table::width, Params::Table::height);
		table.init();
		cur_ball_positions = Params::Table::ballsPositions;
		cur_ball_speeds.fill(Vector2(0, 0));
		scored.fill(false);
		for (auto& arr : last_collision) {
			arr.fill(3);
		}

	}


	void deinit()
	{
		table.deinit();
	}

	void collide_two_balls(int i, int j) {
		if (i >= j || scored[j] || scored[i]) {
			return;
		}
		last_collision[i][j] = std::min(last_collision[i][j] + 1, 10); // to prevent overflow after one year of no collisions
		Vector2 v = cur_ball_positions[i] - cur_ball_positions[j];
		if (Abs(v) > 2 * Params::Ball::radius) {
			return;
		}
		if (last_collision[i][j] < 2) { // this prevents balls from "colliding" again after already going in different derections
			return;
		}
		// firstly, we change the axes to make collision horisontal
		float c = v.x / Abs(v); // cos
		float s = v.y / Abs(v);  // sin
		float x1 = cur_ball_speeds[i].x * c + cur_ball_speeds[i].y * s;
		float y1 = -cur_ball_speeds[i].x * s + cur_ball_speeds[i].y * c;
		float x2 = cur_ball_speeds[j].x * c + cur_ball_speeds[j].y * s;
		float y2 = -cur_ball_speeds[j].x * s + cur_ball_speeds[j].y * c;
		// after the collision Y velocities stay the same because forces are horisontal
		// X velocities are swapped because masses are the same
		std::swap(x1, x2);
		// change the axes back
		cur_ball_speeds[i].x = x1 * c - y1 * s;
		cur_ball_speeds[i].y = x1 * s + y1 * c;
		cur_ball_speeds[j].x = x2 * c - y2 * s;
		cur_ball_speeds[j].y = x2 * s + y2 * c;
		last_collision[i][j] = 0;
	}

	void check_collisions() {
		for (int i = 0; i < 7; ++i) {
			// check wall colisions
			if (scored[i]) {
				continue;
			}
			if (cur_ball_positions[i].x + Params::Ball::radius > 0.5f * Params::Table::width) {
				cur_ball_speeds[i].x *= -1;
			}
			if (cur_ball_positions[i].x < Params::Ball::radius - 0.5f * Params::Table::width) {
				cur_ball_speeds[i].x *= -1;
			}
			if (cur_ball_positions[i].y + Params::Ball::radius > 0.5f * Params::Table::height) {
				cur_ball_speeds[i].y *= -1;
			}
			if (cur_ball_positions[i].y < Params::Ball::radius - 0.5f * Params::Table::height) {
				cur_ball_speeds[i].y *= -1;
			}
			for (const auto& pocket : Params::Table::pocketsPositions) {
				if (Abs(pocket - cur_ball_positions[i]) < Params::Table::pocketRadius) {

					scored[i] = true;
					cur_ball_speeds[i] = Vector2(0, 0);
					table.remove(i);
				}
			}
			for (int j = i + 1; j < 7; ++j) {
				collide_two_balls(i, j);
			}
		}
	}
	void apply_friction() {
		for (int i = 0; i < 7; ++i) {
			if (Abs(cur_ball_speeds[i]) < Params::Ball::friction) {
				cur_ball_speeds[i] = Vector2(0, 0);
			}
			else {
				cur_ball_speeds[i] -= cur_ball_speeds[i] * (Params::Ball::friction / Abs(cur_ball_speeds[i]));
			}
		}
	}

	void update(float dt)
	{
		if (scored[0]) {  // no more moves
			deinit();
			init();
			return;
		}
		bool game_finished = true;
		for (int i = 0; i < 7; i++)
		{
			if (!scored[i]) {
				game_finished = false;
			}
		}
		if (game_finished) {  // game won
			deinit();
			init();
			return;
		}
		if (isChargingShot)
			shotChargeProgress = std::min(shotChargeProgress + dt / Params::Shot::chargeTime, 1.f);
		Scene::updateProgressBar(shotChargeProgress);
		check_collisions();
		for (int i = 0; i < 7; i++)
		{
			cur_ball_positions[i] += cur_ball_speeds[i] * dt;
		}
		apply_friction();
		table.update(cur_ball_positions);

	}



	void mouseButtonPressed(float x, float y)
	{
		for (int i = 0; i < 7; i++) // remove for easier testing
		{
			if (cur_ball_speeds[i]) {
				return;
			}
		}
		isChargingShot = true;
	}


	void mouseButtonReleased(float x, float y)
	{
		for (int i = 0; i < 7; i++) // remove for easier testing
		{
			if (cur_ball_speeds[i]) {
				return;
			}
		}
		Vector2 v = Vector2(x, y) - cur_ball_positions[0];
		isChargingShot = false;
		cur_ball_speeds[0] = v * (shotChargeProgress / Abs(v)) * 6.f;
		//cur_ball_speeds[0] = Vector2(1, 0) * shotChargeProgress * 10.f;  // balls should travell perfectly simmetrical but they don't because 
		shotChargeProgress = 0.f;
	}
}
