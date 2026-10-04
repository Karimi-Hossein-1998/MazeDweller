// Projectiles.hpp - straight-line projectiles fired by the player or enemies.
//
// A projectile travels in a fixed direction and disappears once it exceeds its
// range or hits a wall. Collision (vs enemies or the player) is resolved by
// ResolveCollisions().
#pragma once

#include <cmath>
#include <cstddef>
#include <vector>
#include <SDL3/SDL.h>
#include "Enemies.hpp"
#include "Maze.hpp"
#include "MazeView.hpp"

class Projectiles
{
	public:
		enum class Owner : int { Player = 0, Enemy = 1 };

		void Spawn(float x, float y, float tx, float ty, int dmg, float range, Owner o)
		{
			float dx = tx - x;
			float dy = ty - y;
			const float len = std::sqrt(dx * dx + dy * dy);
			if (len < 1e-6f) { dx = 0.0f; dy = -1.0f; }
			else { dx /= len; dy /= len; }

			posX.push_back(x);
			posY.push_back(y);
			dirX.push_back(dx);
			dirY.push_back(dy);
			rangeLeft.push_back(range);
			damage.push_back(dmg);
			owner.push_back(static_cast<int>(o));
		}

		void Clear()
		{
			posX.clear();
			posY.clear();
			dirX.clear();
			dirY.clear();
			rangeLeft.clear();
			damage.clear();
			owner.clear();
		}

		std::size_t Count() const { return posX.size(); }

		void Remove(std::size_t i)
		{
			posX[i] = posX.back(); posX.pop_back();
			posY[i] = posY.back(); posY.pop_back();
			dirX[i] = dirX.back(); dirX.pop_back();
			dirY[i] = dirY.back(); dirY.pop_back();
			rangeLeft[i] = rangeLeft.back(); rangeLeft.pop_back();
			damage[i] = damage.back(); damage.pop_back();
			owner[i] = owner.back(); owner.pop_back();
		}

		// Move each projectile; drop those that exceed range or hit a wall.
		void Update(const MazeView& view, float dt)
		{
			const float step = Speed * dt;
			for (std::size_t i = posX.size(); i-- > 0; )
			{
				posX[i] += dirX[i] * step;
				posY[i] += dirY[i] * step;
				rangeLeft[i] -= step;
				if (rangeLeft[i] <= 0.0f || !view.IsWalkable(posX[i], posY[i]))
					Remove(i);
			}
		}

		// Resolve collisions. Returns the damage dealt to the player this frame,
		// and writes the ExP earned from killed enemies into `exPEarned`.
		int ResolveCollisions(Enemies& enemies, float playerX, float playerY, const MazeView& view, int& exPEarned)
		{
			int playerDamage = 0;
			exPEarned = 0;
			const float enemyRadius = view.TunnelWidth() * 0.4f;
			const float playerRadius = 16.0f;

			for (std::size_t i = posX.size(); i-- > 0; )
			{
				if (owner[i] == static_cast<int>(Owner::Player))
				{
					for (std::size_t j = 0; j < enemies.Count(); ++j)
					{
						float ex, ey;
						enemies.Center(j, view, ex, ey);
						const float dx = posX[i] - ex;
						const float dy = posY[i] - ey;
						if (dx * dx + dy * dy < enemyRadius * enemyRadius)
						{
							const Enemies::Kind killedKind = static_cast<Enemies::Kind>(enemies.kind[j]);
							if (enemies.Damage(j, damage[i]))
								exPEarned += Enemies::ExPReward(killedKind);
							Remove(i);
							break;
						}
					}
				}
				else
				{
					const float dx = posX[i] - playerX;
					const float dy = posY[i] - playerY;
					if (dx * dx + dy * dy < playerRadius * playerRadius)
					{
						playerDamage += damage[i];
						Remove(i);
					}
				}
			}
			return playerDamage;
		}

		void Render(SDL_Renderer* renderer, const MazeView& view) const
		{
			SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);  // white projectiles
			for (std::size_t i = 0; i < posX.size(); ++i)
			{
				const float size = 6.0f * view.Scale();
				const float sx = view.WorldToScreenX(posX[i]);
				const float sy = view.WorldToScreenY(posY[i]);
				SDL_FRect r = { sx - size * 0.5f, sy - size * 0.5f, size, size };
				SDL_RenderFillRect(renderer, &r);
			}
		}

	private:
		std::vector<float> posX;
		std::vector<float> posY;
		std::vector<float> dirX;
		std::vector<float> dirY;
		std::vector<float> rangeLeft;
		std::vector<int>   damage;
		std::vector<int>   owner;

		static constexpr float Speed = 600.0f;
};
