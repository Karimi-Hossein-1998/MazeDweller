// MazeView.hpp - a camera that renders a portion of the maze on screen.
//
// The maze is drawn as tunnels (tunnelWidth pixels) separated by walls
// (wallThickness pixels). The view holds a camera offset and a uniform scale
// so it can show a window into a maze larger than the screen (camera follows
// the agent) or scale up a small maze to fill the screen.
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>
#include <SDL3/SDL.h>
#include "Maze.hpp"

class MazeView
{
	public:
		MazeView(const Maze& maze, int tunnelWidth, int wallThickness)
			: maze(maze), tunnelWidth(tunnelWidth), wallThickness(wallThickness) {}

		// ---- size accessors ----
		int CellSize() const { return tunnelWidth + wallThickness; }
		int TunnelWidth() const { return tunnelWidth; }
		int WallThickness() const { return wallThickness; }
		int MazeWidth() const { return maze.Width(); }
		int MazeHeight() const { return maze.Height(); }
		int WorldWidth() const { return maze.Width() * CellSize() + wallThickness; }
		int WorldHeight() const { return maze.Height() * CellSize() + wallThickness; }

		// ---- camera ----
		float CameraX() const { return cameraX; }
		float CameraY() const { return cameraY; }
		float Scale() const { return scale; }

		// Map a world-pixel coordinate to a screen coordinate (camera + scale).
		float WorldToScreenX(float worldX) const { return (worldX - cameraX) * scale; }
		float WorldToScreenY(float worldY) const { return (worldY - cameraY) * scale; }

		// Size of the visible region (call once, or whenever the window resizes).
		void SetViewport(int width, int height)
		{
			viewWidth = width;
			viewHeight = height;
			// Scale up small mazes to fill the viewport (uniform, keeps aspect).
			const float sx = WorldWidth()  > 0 ? (float)width  / (float)WorldWidth()  : 1.0f;
			const float sy = WorldHeight() > 0 ? (float)height / (float)WorldHeight() : 1.0f;
			scale = std::max(1.0f, std::min(sx, sy));
		}

		// Center the camera on a world-pixel position, clamped to the maze bounds.
		// Small mazes (that fit on screen) are centered; large mazes follow the agent.
		void CenterOn(float worldX, float worldY)
		{
			const float vw = (float)viewWidth  / scale;
			const float vh = (float)viewHeight / scale;
			if (WorldWidth() <= vw)
				cameraX = ((float)WorldWidth() - vw) * 0.5f;
			else
				cameraX = std::clamp(worldX - vw * 0.5f, 0.0f, (float)WorldWidth() - vw);
			if (WorldHeight() <= vh)
				cameraY = ((float)WorldHeight() - vh) * 0.5f;
			else
				cameraY = std::clamp(worldY - vh * 0.5f, 0.0f, (float)WorldHeight() - vh);
		}

		// Center the camera on a cell's tunnel center.
		void CenterOnCell(int cellX, int cellY)
		{
			CenterOn(cellX * CellSize() + tunnelWidth * 0.5f, cellY * CellSize() + tunnelWidth * 0.5f);
		}

		// World-pixel position of a cell's floor top-left corner.
		SDL_Point CellToWorld(int cellX, int cellY) const
		{
			return { cellX * CellSize(), cellY * CellSize() };
		}

		// Draw the visible cells (tunnels + open passages) in tunnelColor.
		// The background (walls) is whatever the screen was cleared to.
		void Render(SDL_Renderer* renderer, SDL_Color tunnelColor) const
		{
			const int cs = CellSize();
			const float tw = (float)tunnelWidth  * scale;
			const float wt = (float)wallThickness * scale;
			const int startX = std::max(0, (int)std::floor(cameraX / cs));
			const int endX = std::min(maze.Width() - 1, (int)std::floor((cameraX + viewWidth / scale) / cs));
			const int startY = std::max(0, (int)std::floor(cameraY / cs));
			const int endY = std::min(maze.Height() - 1, (int)std::floor((cameraY + viewHeight / scale) / cs));

			SDL_SetRenderDrawColor(renderer, tunnelColor.r, tunnelColor.g, tunnelColor.b, tunnelColor.a);

			for (int cy = startY; cy <= endY; ++cy)
			{
				for (int cx = startX; cx <= endX; ++cx)
				{
					const float wx = WorldToScreenX((float)(cx * cs));
					const float wy = WorldToScreenY((float)(cy * cs));

					SDL_FRect floor = { wx, wy, tw, tw };
					SDL_RenderFillRect(renderer, &floor);

					if (maze.HasPassage(cx, cy, Directions::East))
					{
						SDL_FRect east = { wx + tw, wy, wt, tw };
						SDL_RenderFillRect(renderer, &east);
					}
					if (maze.HasPassage(cx, cy, Directions::South))
					{
						SDL_FRect south = { wx, wy + tw, tw, wt };
						SDL_RenderFillRect(renderer, &south);
					}
				}
			}
		}

		// Highlight the cells along a path (for the 'h' debug overlay).
		void RenderPath(SDL_Renderer* renderer, const std::vector<int>& path, SDL_Color color) const
		{
			SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
			const float tw = (float)tunnelWidth * scale;
			for (const int cell : path)
			{
				const int cx = cell % maze.Width();
				const int cy = cell / maze.Width();
				SDL_FRect rect = { WorldToScreenX((float)(cx * CellSize())), WorldToScreenY((float)(cy * CellSize())), tw, tw };
				SDL_RenderFillRect(renderer, &rect);
			}
		}

		// Fill a single cell with a color (used for the golden exit).
		void RenderCell(SDL_Renderer* renderer, int cellX, int cellY, SDL_Color color) const
		{
			const float tw = (float)tunnelWidth * scale;
			SDL_FRect rect = { WorldToScreenX((float)(cellX * CellSize())), WorldToScreenY((float)(cellY * CellSize())), tw, tw };
			SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
			SDL_RenderFillRect(renderer, &rect);
		}

		// True if the world point (x, y) is inside a tunnel or passage (not a wall).
		bool IsWalkable(float x, float y) const
		{
			const int cs = CellSize();
			const int t = TunnelWidth();
			const int cx = (int)std::floor(x / cs);
			const int cy = (int)std::floor(y / cs);
			if (!maze.InBound(cx, cy)) return false;
			const float lx = x - cx * cs;
			const float ly = y - cy * cs;
			if (lx <= t && ly <= t) return true;                                   // tunnel
			if (lx > t && ly <= t) return maze.HasPassage(cx, cy, Directions::East);  // east wall
			if (lx <= t && ly > t) return maze.HasPassage(cx, cy, Directions::South); // south wall
			return false;                                                         // corner
		}

	private:
		const Maze& maze;
		int tunnelWidth;
		int wallThickness;
		float cameraX = 0.0f;
		float cameraY = 0.0f;
		float scale = 1.0f;
		int viewWidth = 0;
		int viewHeight = 0;
};
