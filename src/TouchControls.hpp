// TouchControls.hpp - on-screen touch controls for mobile (Android).
//
// A circular movepad on the right (movement), a shoot circle on the left,
// and a pause/menu button in the top-right corner.
#pragma once

#include <cmath>
#include <vector>
#include <SDL3/SDL.h>
#include "circle.hpp"

class TouchControls
{
	public:
		void Update(const std::vector<SDL_FPoint>& fingers, int screenW, int screenH)
		{
			moveX = 0.0f;
			moveY = 0.0f;
			shooting = false;
			menuPressed = false;
			knobX = MovePadX(screenW);
			knobY = MovePadY(screenH);

			for (const SDL_FPoint& f : fingers)
			{
				const float fx = f.x * screenW;
				const float fy = f.y * screenH;

				// Movepad (right): direction/magnitude from the pad center.
				const float mx = MovePadX(screenW);
				const float my = MovePadY(screenH);
				const float mdx = fx - mx;
				const float mdy = fy - my;
				const float md = std::sqrt(mdx * mdx + mdy * mdy);
				if (md < MoveRadius)
				{
					const float dead = MoveRadius * 0.15f;  // small dead zone
					if (md > dead)
					{
						const float mag = std::min(1.0f, (md - dead) / (MoveRadius - dead));
						moveX = (mdx / md) * mag;
						moveY = (mdy / md) * mag;
					}
					knobX = fx;
					knobY = fy;
				}

				// Shoot (left): hold to fire.
				const float sx = ShootX(screenW);
				const float sy = ShootY(screenH);
				const float sdx = fx - sx;
				const float sdy = fy - sy;
				if (sdx * sdx + sdy * sdy < ShootRadius * ShootRadius)
					shooting = true;

				// Menu button (top-right): tap to pause.
				const float ux = MenuX(screenW);
				const float uy = MenuY(screenH);
				const float udx = fx - ux;
				const float udy = fy - uy;
				if (udx * udx + udy * udy < MenuRadius * MenuRadius)
					menuPressed = true;
			}
		}

		float MoveX() const { return moveX; }
		float MoveY() const { return moveY; }
		bool Shooting() const { return shooting; }
		bool MenuPressed() const { return menuPressed; }

		void Render(SDL_Renderer* renderer, int screenW, int screenH) const
		{
			SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
			// Movepad (right): translucent base + opaque central knob.
			RenderFilledCircle(renderer, MovePadX(screenW), MovePadY(screenH), MoveRadius, 40, ColorF(1.0f, 1.0f, 1.0f, 0.50f));
			RenderFilledCircle(renderer, knobX, knobY, MoveRadius * 0.5f, 32, ColorF(1.0f, 1.0f, 1.0f, 0.90f));
			// Shoot (left): translucent red circle.
			RenderFilledCircle(renderer, ShootX(screenW), ShootY(screenH), ShootRadius, 40, ColorF(1.0f, 0.35f, 0.35f, 0.50f));
			// Menu button (top-right): translucent circle with a pause glyph.
			RenderFilledCircle(renderer, MenuX(screenW), MenuY(screenH), MenuRadius, 32, ColorF(1.0f, 1.0f, 1.0f, 0.55f));
			SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
			const float barW = MenuRadius * 0.22f;
			const float barH = MenuRadius * 0.9f;
			const float gap  = MenuRadius * 0.30f;
			SDL_FRect b1 = { MenuX(screenW) - gap - barW, MenuY(screenH) - barH * 0.5f, barW, barH };
			SDL_RenderFillRect(renderer, &b1);
			SDL_FRect b2 = { MenuX(screenW) + gap, MenuY(screenH) - barH * 0.5f, barW, barH };
			SDL_RenderFillRect(renderer, &b2);
			SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
		}

	private:
		static float MovePadX(int screenW) { return screenW * 0.75f; }
		static float MovePadY(int screenH) { return screenH * 0.68f; }
		static float ShootX(int screenW) { return screenW * 0.18f; }
		static float ShootY(int screenH) { return screenH * 0.72f; }
		static float MenuX(int screenW) { return screenW * 0.90f; }
		static float MenuY(int screenH) { return screenH * 0.10f; }

		static constexpr float MoveRadius = 200.0f;
		static constexpr float ShootRadius = 70.0f;
		static constexpr float MenuRadius = 40.0f;

		float moveX = 0.0f;
		float moveY = 0.0f;
		float knobX = 0.0f;
		float knobY = 0.0f;
		bool  shooting = false;
		bool  menuPressed = false;
};
