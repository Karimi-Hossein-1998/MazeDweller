// Enemies.hpp - all enemies in one class, stored as parallel vectors.
//
// Struct-of-arrays (SoA) layout. Enemy stats derive from the level L they
// appear in (1-indexed game level). Pursuing enemies stay dormant until they
// see the player, then chase smoothly: continuous pixel movement along the
// BFS path, one waypoint (cell) at a time.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>
#include <SDL3/SDL.h>
#include "Maze.hpp"
#include "MazeView.hpp"
#include "circle.hpp"

class Enemies
{
	public:
		enum class Kind : int { Normal = 0, Pursuing = 1, Elite = 2, Boss = 3 };

		std::vector<int>   cellX;   // current grid cell (for pathfinding)
		std::vector<int>   cellY;
		std::vector<float> posX;    // smooth pixel center (rendering/movement)
		std::vector<float> posY;
		std::vector<int>   kind;
		std::vector<int>   hp;
		std::vector<int>   maxHp;
		std::vector<int>   attack;
		std::vector<bool>  triggered;

		// `level` is the 1-indexed game level (the level the enemy appears in).
		void Spawn(Kind k, int x, int y, int level)
		{
			cellX.push_back(x);
			cellY.push_back(y);
			posX.push_back(0.0f);  // placeholder, filled by EnsurePositions()
			posY.push_back(0.0f);
			kind.push_back(static_cast<int>(k));
			const int h = HpFor(k, level);
			hp.push_back(h);
			maxHp.push_back(h);
			attack.push_back(AttackFor(k, level));
			triggered.push_back(false);
			positionsInit = false;
		}

		void Clear()
		{
			cellX.clear();
			cellY.clear();
			posX.clear();
			posY.clear();
			kind.clear();
			hp.clear();
			maxHp.clear();
			attack.clear();
			triggered.clear();
			positionsInit = false;
		}

		std::size_t Count() const { return cellX.size(); }

		int GetAttack(std::size_t i) const { return attack[i]; }

		// ExP granted for killing each kind.
		static int ExPReward(Kind k)
		{
			switch (k)
			{
				case Kind::Normal:
				case Kind::Pursuing: return 1;
				case Kind::Elite:    return 10;
				case Kind::Boss:     return 100;
			}
			return 0;
		}

		void Remove(std::size_t i)
		{
			cellX[i] = cellX.back(); cellX.pop_back();
			cellY[i] = cellY.back(); cellY.pop_back();
			posX[i] = posX.back(); posX.pop_back();
			posY[i] = posY.back(); posY.pop_back();
			kind[i] = kind.back(); kind.pop_back();
			hp[i] = hp.back(); hp.pop_back();
			maxHp[i] = maxHp.back(); maxHp.pop_back();
			attack[i] = attack.back(); attack.pop_back();
			triggered[i] = triggered.back(); triggered.pop_back();
		}

		// Deal `amount` damage; returns true if the enemy died (and was removed).
		bool Damage(std::size_t i, int amount)
		{
			hp[i] -= amount;
			if (hp[i] <= 0)
			{
				Remove(i);
				return true;
			}
			return false;
		}

		// Index of the enemy nearest (by world-pixel distance) to (px, py), or -1.
		int Nearest(float px, float py, const MazeView& view) const
		{
			(void)view;
			int best = -1;
			float bestDist = std::numeric_limits<float>::max();
			for (std::size_t i = 0; i < posX.size(); ++i)
			{
				const float dx = posX[i] - px;
				const float dy = posY[i] - py;
				const float d = dx * dx + dy * dy;
				if (d < bestDist) { bestDist = d; best = static_cast<int>(i); }
			}
			return best;
		}

		// World-pixel center of enemy i.
		void Center(std::size_t i, const MazeView& view, float& x, float& y) const
		{
			(void)view;
			x = posX[i];
			y = posY[i];
		}

		// Pursuing enemies trigger when they see the player, then chase smoothly.
		void Update(const Maze& maze, const MazeView& view, float playerX, float playerY, float dt)
		{
			EnsurePositions(view);

			const int pcx = static_cast<int>(std::floor(playerX / view.CellSize()));
			const int pcy = static_cast<int>(std::floor(playerY / view.CellSize()));
			const float step = EnemySpeed * dt;

			for (std::size_t i = 0; i < cellX.size(); ++i)
			{
				if (kind[i] != static_cast<int>(Kind::Pursuing))
					continue;
				if (!triggered[i])
				{
					if (!SeesPlayer(i, playerX, playerY, view))
						continue;
					triggered[i] = true;
				}

				// Move toward the next waypoint (next cell on the BFS path).
				const std::vector<int> path = maze.FindPath(cellX[i], cellY[i], pcx, pcy);
				if (path.size() < 2)
					continue;
				const int next = path[1];
				const int nx = next % maze.Width();
				const int ny = next / maze.Width();
				const float tx = nx * view.CellSize() + view.TunnelWidth() * 0.5f;
				const float ty = ny * view.CellSize() + view.TunnelWidth() * 0.5f;
				const float dx = tx - posX[i];
				const float dy = ty - posY[i];
				const float dist = std::sqrt(dx * dx + dy * dy);
				if (dist <= step)
				{
					posX[i] = tx;
					posY[i] = ty;
					cellX[i] = nx;
					cellY[i] = ny;
				}
				else
				{
					posX[i] += dx / dist * step;
					posY[i] += dy / dist * step;
				}
			}
		}

		void Render(SDL_Renderer* renderer, const MazeView& view) const
		{
			const float s = view.Scale();
			const float radius = view.TunnelWidth() * 0.35f * s;
			const int segments = 24;

			for (std::size_t i = 0; i < posX.size(); ++i)
			{
				const float cx = view.WorldToScreenX(posX[i]);
				const float cy = view.WorldToScreenY(posY[i]);
				const Kind k = static_cast<Kind>(kind[i]);
				float topY = cy;

				if (k == Kind::Pursuing)
				{
					const float side = view.TunnelWidth() * 0.5f * s;
					SDL_FRect r = { cx - side * 0.5f, cy - side * 0.5f, side, side };
					SDL_SetRenderDrawColor(renderer, 255, 60, 60, 255);
					SDL_RenderFillRect(renderer, &r);
					topY = cy - side * 0.5f;
				}
				else
				{
					ColorF color(1.0f, 0.25f, 0.25f, 1.0f);                         // Normal: red
					if (k == Kind::Elite) color = ColorF(1.0f, 0.55f, 0.10f, 1.0f); // Elite: orange
					else if (k == Kind::Boss) color = ColorF(1.0f, 0.78f, 0.10f, 1.0f); // Boss: golden
					RenderFilledCircle(renderer, cx, cy, radius, segments, color);
					topY = cy - radius;
				}

				// HP bar above the enemy (green -> red as HP drops).
				if (maxHp[i] > 0)
				{
					const float frac = std::clamp((float)hp[i] / (float)maxHp[i], 0.0f, 1.0f);
					const float barW = view.TunnelWidth() * 0.7f * s;
					const float barH = 4.0f * s;
					const float bx = cx - barW * 0.5f;
					const float by = topY - barH - 3.0f * s;
					SDL_FRect bg = { bx, by, barW, barH };
					SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
					SDL_RenderFillRect(renderer, &bg);
					SDL_FRect fill = { bx, by, barW * frac, barH };
					SDL_SetRenderDrawColor(renderer, (Uint8)(255.0f * (1.0f - frac)), (Uint8)(255.0f * frac), 40, 255);
					SDL_RenderFillRect(renderer, &fill);
				}
			}
		}

	private:
		// Fill posX/posY from the grid cells once (first Update after spawning).
		void EnsurePositions(const MazeView& view)
		{
			if (positionsInit)
				return;
			const float half = view.TunnelWidth() * 0.5f;
			for (std::size_t i = 0; i < cellX.size(); ++i)
			{
				posX[i] = cellX[i] * view.CellSize() + half;
				posY[i] = cellY[i] * view.CellSize() + half;
			}
			positionsInit = true;
		}

		// True if enemy i can see the player (within range, clear line of sight).
		bool SeesPlayer(std::size_t i, float playerX, float playerY, const MazeView& view) const
		{
			const float ex = posX[i];
			const float ey = posY[i];
			const float dx = playerX - ex;
			const float dy = playerY - ey;
			const float dist = std::sqrt(dx * dx + dy * dy);
			if (dist > SightRangeCells * view.CellSize())
				return false;

			const int steps = std::max(1, static_cast<int>(dist / 4.0f));
			for (int s = 1; s < steps; ++s)
			{
				const float t = static_cast<float>(s) / steps;
				if (!view.IsWalkable(ex + dx * t, ey + dy * t))
					return false;
			}
			return true;
		}

		static int HpFor(Kind k, int level)
		{
			switch (k)
			{
				case Kind::Normal:
				case Kind::Pursuing: return 2 * level;
				case Kind::Elite:    return 10 * level;
				case Kind::Boss:     return 100 * level;
			}
			return 1;
		}

		static int AttackFor(Kind k, int level)
		{
			switch (k)
			{
				case Kind::Normal:
				case Kind::Pursuing: return level;
				case Kind::Elite:    return 2 * level;
				case Kind::Boss:     return 10 * level;
			}
			return 1;
		}

		static constexpr float EnemySpeed = 150.0f;
		static constexpr float SightRangeCells = 5.0f;
		bool positionsInit = false;
};
