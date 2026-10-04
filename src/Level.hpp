// Level.hpp - a level loaded from (or saved to) an ASCII text file.
//
// File format (thin-wall grid):
//   // comment (optional)
//   width <W>
//   height <H>
//   <blank>
//   <(2*H+1) rows of (2*W+1) chars>
//
// Symbols:
//   '#' wall · ' ' tunnel · 'O' player start · 'W' exit (next level)
//   (future: 'X' normal enemy, 'P' pursuing enemy)
#pragma once

#include <algorithm>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include "Enemies.hpp"
#include "Maze.hpp"
#include "Orbs.hpp"

namespace Symbols
{
	constexpr char Wall   = '#';
	constexpr char Tunnel = ' ';
	constexpr char Player = 'O';
	constexpr char Exit   = 'W';
}

class Level
{
	public:
		// Generate a random level (Wilson's) with a default start/exit.
		void Generate(int width, int height)
		{
			maze.Resize(width, height);
			maze.GenerateWilsonMaze();
			playerStartX = width / 2;
			playerStartY = height / 2;
			exitX = width - 1;
			exitY = height - 1;
			enemies.Clear();
			orbs.Clear();
		}

		// Deterministic level: size (l+1)*5, seeded by the level number.
		void GenerateLevel(int levelNumber)
		{
			const int size = (levelNumber + 1) * 3;
			maze.Resize(size, size);
			maze.GenerateWilsonMaze((uint64_t)levelNumber + 1);
			playerStartX = 0;
			playerStartY = 0;
			exitX = size - 1;
			exitY = size - 1;
			enemies.Clear();
			const int orbCount = std::max(1, (levelNumber + 1) * (levelNumber + 1) / 2);
			orbs.Scatter(maze, playerStartX, playerStartY, exitX, exitY, (uint64_t)levelNumber + 1000, orbCount);
			SpawnEnemies(levelNumber);
		}

		bool LoadFromFile(const char* path)
		{
			std::ifstream in(path);
			if (!in) return false;

			int w = 0;
			int h = 0;
			std::vector<std::string> grid;
			std::string line;
			while (std::getline(in, line))
			{
				// Strip '//' comments.
				const std::size_t c = line.find("//");
				if (c != std::string::npos) line.erase(c);

				// Trim surrounding whitespace.
				const std::size_t first = line.find_first_not_of(" \t\r\n");
				if (first == std::string::npos) continue;
				const std::size_t last = line.find_last_not_of(" \t\r\n");
				line = line.substr(first, last - first + 1);

				// Metadata "width <n>" / "height <n>".
				std::istringstream iss(line);
				std::string key;
				if (iss >> key && (key == "width" || key == "height"))
				{
					int value = 0;
					iss >> value;
					if (key == "width") w = value;
					else                h = value;
				}
				else
				{
					grid.push_back(line);  // a grid row
				}
			}

			if (w <= 0 || h <= 0 || (int)grid.size() != 2 * h + 1) return false;
			for (const std::string& row : grid)
				if ((int)row.size() < 2 * w + 1) return false;

			maze.Resize(w, h);
			playerStartX = w / 2;
			playerStartY = h / 2;
			exitX = w - 1;
			exitY = h - 1;
			enemies.Clear();
			orbs.Clear();

			for (int y = 0; y < h; ++y)
			{
				for (int x = 0; x < w; ++x)
				{
					maze.SetPassage(x, y, Directions::North, grid[2 * y][2 * x + 1] != Symbols::Wall);
					maze.SetPassage(x, y, Directions::East,  grid[2 * y + 1][2 * x + 2] != Symbols::Wall);
					maze.SetPassage(x, y, Directions::South, grid[2 * y + 2][2 * x + 1] != Symbols::Wall);
					maze.SetPassage(x, y, Directions::West,  grid[2 * y + 1][2 * x] != Symbols::Wall);

					const char c = grid[2 * y + 1][2 * x + 1];
					if (c == Symbols::Player) { playerStartX = x; playerStartY = y; }
					else if (c == Symbols::Exit) { exitX = x; exitY = y; }
					// future: 'X' -> enemies.Add(x, y, Enemies::Kind::Normal), 'P' -> Pursuing
				}
			}
			return true;
		}

		bool SaveToFile(const char* path) const
		{
			std::ofstream out(path);
			if (!out) return false;

			const int w = maze.Width();
			const int h = maze.Height();

			out << "width " << w << "\n";
			out << "height " << h << "\n";
			out << "\n";

			for (int gy = 0; gy <= 2 * h; ++gy)
			{
				for (int gx = 0; gx <= 2 * w; ++gx)
				{
					const bool gxOdd = (gx % 2 == 1);
					const bool gyOdd = (gy % 2 == 1);
					char c;
					if (gxOdd && gyOdd)
					{
						const int x = (gx - 1) / 2;
						const int y = (gy - 1) / 2;
						if (x == playerStartX && y == playerStartY) c = Symbols::Player;
						else if (x == exitX && y == exitY) c = Symbols::Exit;
						else c = Symbols::Tunnel;
					}
					else if (gxOdd && !gyOdd)
					{
						const int x = (gx - 1) / 2;
						const int above = gy / 2 - 1;
						const bool open = (above >= 0 && above < h && maze.HasPassage(x, above, Directions::South));
						c = open ? Symbols::Tunnel : Symbols::Wall;
					}
					else if (!gxOdd && gyOdd)
					{
						const int y = (gy - 1) / 2;
						const int left = gx / 2 - 1;
						const bool open = (left >= 0 && left < w && maze.HasPassage(left, y, Directions::East));
						c = open ? Symbols::Tunnel : Symbols::Wall;
					}
					else
					{
						c = Symbols::Wall;  // corners and boundary
					}
					out << c;
				}
				out << "\n";
			}
			return (bool)out;
		}

		const Maze& GetMaze() const { return maze; }
		int PlayerStartX() const { return playerStartX; }
		int PlayerStartY() const { return playerStartY; }
		int ExitX() const { return exitX; }
		int ExitY() const { return exitY; }
		Enemies& GetEnemies() { return enemies; }
		Orbs& GetOrbs() { return orbs; }

		void Update(const MazeView& view, float playerX, float playerY, float dt) { enemies.Update(maze, view, playerX, playerY, dt); }

	private:
		// Spawn the level's enemy population. Pursuing enemies appear every level;
		// Normal/Elite every 5th level (1-indexed), the Boss every 10th, guarding the W.
		void SpawnEnemies(int levelNumber)
		{
			const int L = levelNumber + 1;       // 1-indexed level
			const bool hasCombat = (L % 5 == 0); // Normal + Elite
			const bool hasBoss = (L % 10 == 0);  // Boss

			const int normalCount = hasCombat ? 5 * levelNumber + 1 : 0;
			const int pursuingCount = 5 * levelNumber + 1; // always present
			const int eliteCount = hasCombat ? 2 * levelNumber : 0;

			// Boss placement: a connected neighbor of the exit (guards the W).
			int bossX = -1;
			int bossY = -1;
			if (hasBoss)
			{
				for (int d = 0; d < 4; ++d)
				{
					const int nx = exitX + dX[d];
					const int ny = exitY + dY[d];
					if (maze.InBound(nx, ny) && maze.HasPassage(exitX, exitY, static_cast<Directions>(d)))
					{
						bossX = nx;
						bossY = ny;
						break;
					}
				}
			}

			std::vector<int> cells;
			cells.reserve(maze.Width() * maze.Height());
			for (int y = 0; y < maze.Height(); ++y)
				for (int x = 0; x < maze.Width(); ++x)
					if (!(x == playerStartX && y == playerStartY) && !(x == exitX && y == exitY) && !(x == bossX && y == bossY))
						cells.push_back(y * maze.Width() + x);

			std::mt19937_64 rng((uint64_t)levelNumber + 2000);
			std::shuffle(cells.begin(), cells.end(), rng);

			int idx = 0;
			auto take = [&]() -> int
			{
				if (idx >= (int)cells.size()) return -1;
				return cells[idx++];
			};

			for (int i = 0; i < normalCount; ++i) { const int c = take(); if (c >= 0) enemies.Spawn(Enemies::Kind::Normal, c % maze.Width(), c / maze.Width(), L); }
			for (int i = 0; i < pursuingCount; ++i) { const int c = take(); if (c >= 0) enemies.Spawn(Enemies::Kind::Pursuing, c % maze.Width(), c / maze.Width(), L); }
			for (int i = 0; i < eliteCount; ++i) { const int c = take(); if (c >= 0) enemies.Spawn(Enemies::Kind::Elite, c % maze.Width(), c / maze.Width(), L); }
			if (hasBoss && bossX >= 0)
				enemies.Spawn(Enemies::Kind::Boss, bossX, bossY, L);
		}

		Maze maze;
		int playerStartX = 0;
		int playerStartY = 0;
		int exitX = 0;
		int exitY = 0;
		Enemies enemies;
		Orbs orbs;
};
