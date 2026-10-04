#pragma once

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <queue>
#include <random>
#include <vector>

enum class Directions {North=0, East=1, South=2, West=3};

const int dX[4] = {0,1,0,-1};
const int dY[4] = {-1,0,1,0};

constexpr const char* theMazeGlyphs[16]  = {"\u00B7","\u2575","\u2576","\u2514",
											"\u2577","\u2502","\u250C","\u2523",
											"\u2574","\u2518","\u2500","\u2534",
											"\u2510","\u252B","\u252C","\u253C"
										};

class Maze
{
	private:
		int width;
		int height;
		int gridSize;

		std::vector<bool> inMaze;

		std::vector<bool> northPass;
		std::vector<bool> eastPass;
		std::vector<bool> southPass;
		std::vector<bool> westPass;
		
	public:
		Maze(int w=0, int h=0) : width(w), height(h), gridSize(w*h), inMaze(gridSize,false),
										northPass(gridSize,false), eastPass(gridSize,false), southPass(gridSize,false), westPass(gridSize,false) {}

		inline int Index(int x, int y) const {return y*width+x;}
		inline bool InBound(int x, int y) const {return (x>=0) && (x<width) && (y>=0) && (y<height);}

		inline int Width() const { return width; }
		inline int Height() const { return height; }
		inline bool HasPassage(int x, int y, Directions d) const
		{
			if (!InBound(x, y)) return false;
			int i = Index(x, y);
			switch (d)
			{
				case Directions::North: return northPass[i];
				case Directions::East:  return eastPass[i];
				case Directions::South: return southPass[i];
				case Directions::West:  return westPass[i];
				default: return false;
			}
		}

		inline void Resize(int w, int h)
		{
			width = w;
			height = h;
			gridSize = w * h;
			inMaze.assign(gridSize, false);
			northPass.assign(gridSize, false);
			eastPass.assign(gridSize, false);
			southPass.assign(gridSize, false);
			westPass.assign(gridSize, false);
		}

		inline void SetPassage(int x, int y, Directions d, bool open)
		{
			if (!InBound(x, y)) return;
			int i = Index(x, y);
			switch (d)
			{
				case Directions::North: northPass[i] = open; break;
				case Directions::East:  eastPass[i]  = open; break;
				case Directions::South: southPass[i] = open; break;
				case Directions::West:  westPass[i]  = open; break;
				default: break;
			}
		}

		// Shortest path from (startX,startY) to (goalX,goalY) via BFS.
		// Returns the cell indices along the path (empty if unreachable).
		std::vector<int> FindPath(int startX, int startY, int goalX, int goalY) const
		{
			if (!InBound(startX, startY) || !InBound(goalX, goalY)) return {};

			const int start = Index(startX, startY);
			const int goal  = Index(goalX, goalY);

			std::vector<int> parent(gridSize, -1);
			std::vector<bool> visited(gridSize, false);
			std::queue<int> frontier;
			frontier.push(start);
			visited[start] = true;

			while (!frontier.empty())
			{
				const int cur = frontier.front();
				frontier.pop();
				if (cur == goal) break;

				const int cx = cur % width;
				const int cy = cur / width;
				for (int d = 0; d < 4; ++d)
				{
					if (!HasPassage(cx, cy, static_cast<Directions>(d))) continue;
					const int nx = cx + dX[d];
					const int ny = cy + dY[d];
					if (!InBound(nx, ny)) continue;
					const int ni = Index(nx, ny);
					if (visited[ni]) continue;
					visited[ni] = true;
					parent[ni] = cur;
					frontier.push(ni);
				}
			}

			if (!visited[goal]) return {};

			std::vector<int> path;
			for (int at = goal; at != -1; at = parent[at])
				path.push_back(at);
			std::reverse(path.begin(), path.end());
			return path;
		}

		inline void GenerateWilsonMaze()
		{
			std::random_device rd{};
			GenerateWilsonMaze((uint64_t)rd());
		}

		inline void GenerateWilsonMaze(uint64_t seed)
		{
			std::mt19937_64 rng(seed);

			std::vector<int> unvisited(gridSize);
			std::iota(unvisited.begin(),unvisited.end(),0);
			std::vector<int> posInUnvisited(gridSize);
			std::iota(posInUnvisited.begin(),posInUnvisited.end(),0);

			std::uniform_int_distribution<int> uid(0,gridSize-1);

			int root = uid(rng);
			int rootPos = posInUnvisited[root];
			int lastCell = unvisited.back();
			unvisited[rootPos] = lastCell;
			unvisited.pop_back();
			posInUnvisited[lastCell] = rootPos;
			posInUnvisited[root] = -1;
			inMaze[root] = true;
			std::uniform_int_distribution<int> dirDist(0,3);
			

			std::vector<int> nextDir(gridSize,-1);
			while (!unvisited.empty())
			{
				std::uniform_int_distribution<int> dist(0,unvisited.size()-1);
				int newRoot = unvisited[dist(rng)];
				int current = newRoot;
				while(!inMaze[current])
				{
					int currentX = current%width;
					int currentY = current/width;
					int dir = -1;
					int nX  = -1;
					int nY  = -1;
					do
					{
						dir = dirDist(rng);
						nX  = currentX + dX[dir];
						nY  = currentY + dY[dir];
					} while (!InBound(nX,nY));

					nextDir[current] = dir;
					current = Index(nX,nY);
				}

				int currentTrace = newRoot;
				while (!inMaze[currentTrace])
				{
					inMaze[currentTrace] = true;
					int pos = posInUnvisited[currentTrace];
					int lastCell = unvisited.back();
					unvisited[pos] = lastCell;
					unvisited.pop_back();
					posInUnvisited[lastCell] = pos;
					posInUnvisited[currentTrace] = -1;

					int dir      = nextDir[currentTrace];
					int cX       = currentTrace%width;
					int cY       = currentTrace/width;
					int nX       = cX + dX[dir];
					int nY       = cY + dY[dir];
					int nextCell = Index(nX,nY);

					switch(static_cast<Directions>(dir))
					{
						case Directions::North:
							northPass[currentTrace] = true;
							southPass[nextCell]     = true;
							break;
						case Directions::East:
							eastPass[currentTrace] = true;
							westPass[nextCell]     = true;
							break;
						case Directions::South:
							southPass[currentTrace] = true;
							northPass[nextCell]     = true;
							break;
						case Directions::West:
							westPass[currentTrace] = true;
							eastPass[nextCell]     = true;
							break;
						default:
							break;
					}
					currentTrace = nextCell;
				}
			}
		}

		inline void print()
		{
			int index = -1;
			for (int y{0}; y<height; ++y)
			{
				for (int x{0}; x<width; ++x)
				{
					index = y*width+x;
					int bitMask = (northPass[index]?1:0) | (eastPass[index]?2:0) | (southPass[index]?4:0) | (westPass[index]?8:0);
					std::cout << theMazeGlyphs[bitMask];
				}
				std::cout << '\n';
			}
			std::cout << '\n';
		}
};




