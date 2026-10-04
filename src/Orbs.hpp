// Orbs.hpp - ExP orbs (fading bluish circles) scattered in a level.
//
// Struct-of-arrays, like Enemies. Orbs are placed deterministically (seeded
// by the level) and each grants 1 ExP when the player's cell matches.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <random>
#include <vector>
#include <SDL3/SDL.h>
#include "Maze.hpp"
#include "MazeView.hpp"
#include "circle.hpp"

class Orbs
{
	public:
		// Scatter `count` orbs over the maze, avoiding the start and exit cells.
		void Scatter(const Maze& maze, int startX, int startY, int exitX, int exitY, uint64_t seed, int count)
		{
			cellX.clear();
			cellY.clear();
			collected.clear();

			std::vector<int> candidates;
			candidates.reserve(maze.Width() * maze.Height());
			for (int y = 0; y < maze.Height(); ++y)
				for (int x = 0; x < maze.Width(); ++x)
					if (!(x == startX && y == startY) && !(x == exitX && y == exitY))
						candidates.push_back(y * maze.Width() + x);

			std::mt19937_64 rng(seed);
			std::shuffle(candidates.begin(), candidates.end(), rng);

			const int n = std::min(count, (int)candidates.size());
			for (int i = 0; i < n; ++i)
			{
				cellX.push_back(candidates[i] % maze.Width());
				cellY.push_back(candidates[i] / maze.Width());
				collected.push_back(false);
			}
		}

		void Clear()
		{
			cellX.clear();
			cellY.clear();
			collected.clear();
		}

		// Collect any uncollected orbs at (x,y); returns how many were collected.
		int CollectAt(int x, int y)
		{
			int got = 0;
			for (std::size_t i = 0; i < cellX.size(); ++i)
			{
				if (!collected[i] && cellX[i] == x && cellY[i] == y)
				{
					collected[i] = true;
					++got;
				}
			}
			return got;
		}

		std::size_t Remaining() const
		{
			std::size_t n = 0;
			for (const bool c : collected)
				if (!c)
					++n;
			return n;
		}

		// Draw each uncollected orb as a layered glow: a bright core wrapped in
		// shells of progressively dimmer, darker blue.
		void Render(SDL_Renderer* renderer, const MazeView& view, float time) const
		{
			const int layers = 5;                                  // 1 bright core + 4 shells
			const float s = view.Scale();
			const float maxRadius = view.TunnelWidth() * 0.32f * s;
			const int segments = 24;

			SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
			for (std::size_t i = 0; i < cellX.size(); ++i)
			{
				if (collected[i])
					continue;
				const float wx = view.WorldToScreenX(cellX[i] * view.CellSize() + view.TunnelWidth() * 0.5f);
				const float wy = view.WorldToScreenY(cellY[i] * view.CellSize() + view.TunnelWidth() * 0.5f);

				// Whole-orb fade (pulse), de-synced per orb.
				const float pulse = 0.55f + 0.45f * std::sin(time * 2.5f + (float)i * 1.3f);

				// Outermost shell first (largest, dimmest), inward to the bright core.
				for (int layer = layers - 1; layer >= 0; --layer)
				{
					const float t = (float)layer / (float)(layers - 1);  // 1.0 outer -> 0.0 core
					const float radius = maxRadius * (0.30f + 0.70f * t);
					const float r = 0.15f + 0.75f * (1.0f - t);
					const float g = 0.35f + 0.60f * (1.0f - t);
					const float b = 0.60f + 0.40f * (1.0f - t);
					const float a = pulse * (0.55f + 0.45f * (1.0f - t));
					RenderFilledCircle(renderer, wx, wy, radius, segments, ColorF(r, g, b, a));
				}
			}
			SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
		}

	private:
		std::vector<int>  cellX;
		std::vector<int>  cellY;
		std::vector<bool> collected;
};
