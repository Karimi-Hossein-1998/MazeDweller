// Player.hpp - the player's persistent ExP progression and combat stats.
//
// ExP levels the player up: going from level m to m+1 costs (m+1)*100 ExP.
// HitPoints and Damage are derived from the level, ready for enemies later.
#pragma once

#include <algorithm>

class Player
{
	public:
		int Level() const { return level; }
		int ExP() const { return exP; }
		int ExPToNext() const { return (level + 1) * 100; }

		int HitPoints() const { return hitPoints; }
		int MaxHitPoints() const { return level * 10 + 10; }
		int Damage() const { return level * 2 + 1; }

		void GainExP(int amount)
		{
			exP += amount;
			while (exP >= ExPToNext())
			{
				exP -= ExPToNext();
				const int oldMax = MaxHitPoints();
				++level;
				hitPoints += (MaxHitPoints() - oldMax);  // heal the newly-gained HP
			}
		}

		// Used once enemies can hit the player.
		void TakeDamage(int amount)
		{
			hitPoints = std::max(0, hitPoints - amount);
		}

		// Restore to full HP (used on retry).
		void HealToFull()
		{
			hitPoints = MaxHitPoints();
		}

		// Restore progression from a save (full HP).
		void SetProgression(int lvl, int exp)
		{
			level = lvl;
			exP = exp;
			hitPoints = MaxHitPoints();
		}

		void Reset()
		{
			level = 0;
			exP = 0;
			hitPoints = MaxHitPoints();
		}

	private:
		int level = 0;
		int exP = 0;
		int hitPoints = 10;  // == MaxHitPoints() at level 0
};
