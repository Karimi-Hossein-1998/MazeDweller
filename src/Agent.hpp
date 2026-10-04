// Agent.hpp - an agent that moves smoothly through the maze tunnels.
//
// The agent has a continuous world-pixel position and moves with the arrow
// keys at a constant speed, sliding along walls (it can only travel through
// open passages). Collision is checked to sub-pixel precision.
#pragma once

#include <cmath>
#include <SDL3/SDL.h>
#include "Maze.hpp"
#include "MazeView.hpp"

class Agent
{
	public:
		Agent(int startCellX, int startCellY, const MazeView& view, SDL_Color color = { 0, 200, 200, 255 })
			: view(view), color(color)
		{
			posX = startCellX * view.CellSize() + view.TunnelWidth() * 0.5f;
			posY = startCellY * view.CellSize() + view.TunnelWidth() * 0.5f;
		}

		// Continuous world-pixel position of the agent's center.
		float PosX() const { return posX; }
		float PosY() const { return posY; }
		int CellX() const { return (int)std::floor(posX / view.CellSize()); }
		int CellY() const { return (int)std::floor(posY / view.CellSize()); }

		// Move the agent to a cell's tunnel center (e.g. the level start).
		void Reset(int startCellX, int startCellY)
		{
			posX = startCellX * view.CellSize() + view.TunnelWidth() * 0.5f;
			posY = startCellY * view.CellSize() + view.TunnelWidth() * 0.5f;
		}

		// Move in direction (dx,dy); normalized internally, slides along walls.
		void Move(float dx, float dy, const Maze& maze, float dt)
		{
			const float length = std::sqrt(dx * dx + dy * dy);
			if (length < 1e-6f)
				return;
			dx /= length;
			dy /= length;

			const float nx = posX + dx * speed * dt;
			if (!Collides(nx, posY, maze)) posX = nx;

			const float ny = posY + dy * speed * dt;
			if (!Collides(posX, ny, maze)) posY = ny;
		}

		// Move with the held arrow keys. Frame-rate independent (dt in seconds).
		void Update(const bool* keys, const Maze& maze, float dt)
		{
			float dx = 0.0f;
			float dy = 0.0f;
			if (keys[SDL_SCANCODE_UP])    dy -= 1.0f;
			if (keys[SDL_SCANCODE_DOWN])  dy += 1.0f;
			if (keys[SDL_SCANCODE_LEFT])  dx -= 1.0f;
			if (keys[SDL_SCANCODE_RIGHT]) dx += 1.0f;
			Move(dx, dy, maze, dt);
		}

		// Draw the agent as a small filled square.
		void Render(SDL_Renderer* renderer) const
		{
			const float size = (float)sideLength * view.Scale();
			SDL_FRect rect = { view.WorldToScreenX(posX) - size * 0.5f, view.WorldToScreenY(posY) - size * 0.5f, size, size };
			SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
			SDL_RenderFillRect(renderer, &rect);
		}

	private:
		// True if the agent's square (centered at px,py) overlaps any wall.
		bool Collides(float px, float py, const Maze& maze) const
		{
			const int cs = view.CellSize();
			const int t  = view.TunnelWidth();
			const float half = sideLength * 0.5f;
			const float x0 = px - half;
			const float x1 = px + half;
			const float y0 = py - half;
			const float y1 = py + half;

			// Outer boundary walls.
			if (x0 < 0.0f || y0 < 0.0f || x1 > (float)(maze.Width() * cs) || y1 > (float)(maze.Height() * cs))
				return true;

			const int cx0 = (int)std::floor(x0 / cs);
			const int cy0 = (int)std::floor(y0 / cs);
			const int cx1 = (int)std::floor(x1 / cs);
			const int cy1 = (int)std::floor(y1 / cs);

			for (int cy = cy0; cy <= cy1; ++cy)
			{
				for (int cx = cx0; cx <= cx1; ++cx)
				{
					if (!maze.InBound(cx, cy)) continue;
					const float bx = (float)(cx * cs);
					const float by = (float)(cy * cs);

					// East wall (solid unless there is an east passage).
					if (!maze.HasPassage(cx, cy, Directions::East))
						if (Overlap(x0, y0, x1, y1, bx + t, by, bx + cs, by + t)) return true;
					// South wall.
					if (!maze.HasPassage(cx, cy, Directions::South))
						if (Overlap(x0, y0, x1, y1, bx, by + t, bx + t, by + cs)) return true;
					// Corner (always solid).
					if (Overlap(x0, y0, x1, y1, bx + t, by + t, bx + cs, by + cs)) return true;
				}
			}
			return false;
		}

		static bool Overlap(float ax0, float ay0, float ax1, float ay1, float bx0, float by0, float bx1, float by1)
		{
			return ax0 < bx1 && ax1 > bx0 && ay0 < by1 && ay1 > by0;
		}

		const MazeView& view;
		float posX = 0.0f;
		float posY = 0.0f;
		float speed = 400.0f;
		const int sideLength = 40;
		SDL_Color color;
};
