#pragma once
#include <SDL3/SDL.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_render.h>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

inline constexpr const double epsSqr=1.0e-10;

class Color;
class ColorF;

class Color
{
	private:
		unsigned short R;
		unsigned short G;
		unsigned short B;
		unsigned short A;
	public:
		// Constructor
		Color() = default;
		Color(const Color&) = default;
		Color(Color&&) noexcept = default;
		explicit Color(unsigned short r, unsigned short g, unsigned short b, unsigned short a) : R(r), G(g), B(b), A(a) {}
		// Assignment
		Color& operator=(const Color&) = default;
		Color& operator=(Color&&) noexcept = default;
		Color& operator=(const ColorF& cf);
		// Destructor
		~Color() = default;
		[[nodiscard]] unsigned short GetR() const noexcept {return R;}
		[[nodiscard]] unsigned short GetG() const noexcept {return G;}
		[[nodiscard]] unsigned short GetB() const noexcept {return B;}
		[[nodiscard]] unsigned short GetA() const noexcept {return A;}
		
};

class ColorF
{
	private:
		float R;
		float G;
		float B;
		float A;
	public:
		// Constructor
		ColorF() = default;
		ColorF(const ColorF&) = default;
		ColorF(ColorF&&) noexcept = default;
		explicit ColorF(float r, float g, float b, float a) : R(r), G(g), B(b), A(a) {}
		// Assignment
		ColorF& operator=(const ColorF&) = default;
		ColorF& operator=(ColorF&&) noexcept = default;
		ColorF& operator=(const Color& c);
		// Destructor
		~ColorF() = default;
		[[nodiscard]] float GetR() const noexcept {return R;}
		[[nodiscard]] float GetG() const noexcept {return G;}
		[[nodiscard]] float GetB() const noexcept {return B;}
		[[nodiscard]] float GetA() const noexcept {return A;}
};

inline Color& Color::operator=(const ColorF& cf)
{
	R = static_cast<unsigned short>(cf.GetR()*255);
	G = static_cast<unsigned short>(cf.GetG()*255);
	B = static_cast<unsigned short>(cf.GetB()*255);
	A = static_cast<unsigned short>(cf.GetA()*255);
	return *this;
}

inline ColorF& ColorF::operator=(const Color& c)
{
	R = static_cast<float>(c.GetR())/255.0f;
	G = static_cast<float>(c.GetG())/255.0f;
	B = static_cast<float>(c.GetB())/255.0f;
	A = static_cast<float>(c.GetA())/255.0f;
	return *this;
}


inline void RenderFilledCircle(SDL_Renderer* renderer, float cx, float cy, float radius, int segments, const ColorF& color)
{
	std::vector<SDL_Vertex> vertices;
	vertices.reserve(segments+1);
	vertices.push_back(SDL_Vertex{.position={cx,cy},.color={color.GetR(),color.GetG(),color.GetB(),color.GetA()},.tex_coord={cx,cy}});

	float vx{}, vy{}, theta{};
	const float step = static_cast<float>(2.0*std::numbers::pi/segments);
	for (size_t i{}; i<segments; ++i)
	{
		theta = step*i;
		vx = cx+cos(theta) * radius;
		vy = cy+sin(theta) * radius;
		vertices.push_back(SDL_Vertex{.position={vx,vy},.color={color.GetR(),color.GetG(),color.GetB(),color.GetA()},.tex_coord={vx,vy}});
	}
	std::vector<int> indices;
	indices.reserve(segments*3+1);
	for (size_t i{1}; i<segments; ++i)
	{
		indices.push_back(0); indices.push_back(i); indices.push_back(i+1);
	}
	indices.push_back(0); indices.push_back(segments); indices.push_back(1);

	SDL_RenderGeometry(renderer,nullptr,vertices.data(),static_cast<int>(vertices.size()),indices.data(),static_cast<int>(indices.size()));
}

inline void RenderCircle(SDL_Renderer* renderer, float cx, float cy, float radius, ColorF color, float dr=0.1, int segments=100)
{
	std::vector<SDL_Vertex> vertices;
	vertices.reserve(2*segments);
	float drHalf = dr*0.5;
	float innerRadius = radius - drHalf;
	float outerRadius = radius + drHalf;
	float vx{}, vy{}, theta{}, cosTheta{}, sinTheta{};
	const float step = static_cast<float>(2.0*std::numbers::pi/segments);
	for (size_t i{}; i<segments; ++i)
	{
		theta = step*i;
		cosTheta = cos(theta);
		sinTheta = sin(theta);
		vx = cx + cosTheta * innerRadius;
		vy = cy + sinTheta * innerRadius;
		vertices.push_back(SDL_Vertex{.position={vx,vy},.color={color.GetR(),color.GetG(),color.GetB(),color.GetA()},.tex_coord={0.0f,0.0f}});
		vx = cx + outerRadius * cosTheta;
		vy = cy + outerRadius * sinTheta;
		vertices.push_back(SDL_Vertex{.position={vx,vy},.color={color.GetR(),color.GetG(),color.GetB(),color.GetA()},.tex_coord={0.0f,0.0f}});
	}
	std::vector<int> indices;
	indices.reserve(segments*6);
	indices.push_back(2*segments-1); indices.push_back(0); indices.push_back(1);
	for (size_t i{1}; i<2*segments-1; ++i)
	{
		indices.push_back(i-1); indices.push_back(i); indices.push_back(i+1);
	}
	indices.push_back(2*segments-2); indices.push_back(2*segments-1); indices.push_back(0);

	SDL_RenderGeometry(renderer,nullptr,vertices.data(),static_cast<int>(vertices.size()),indices.data(),static_cast<int>(indices.size()));
}

inline void RenderFilledEllipse(SDL_Renderer* renderer, float cx, float cy, float rx, float ry, float rotation, int segments, const ColorF& color)
{
	std::vector<SDL_Vertex> vertices;
	vertices.reserve(segments+1);
	vertices.push_back(SDL_Vertex{.position={cx,cy},.color={color.GetR(),color.GetG(),color.GetB(),color.GetA()},.tex_coord={cx,cy}});

	float vx{}, vx_{}, vy{}, vy_{}, theta{};
	const float step = static_cast<float>(2.0*std::numbers::pi/segments);
	const float sRotation = sin(rotation), cRotation = cos(rotation);
	for (size_t i{}; i<segments; ++i)
	{
		theta = step*i + rotation;
		vx_ = cos(theta) * rx;
		vy_ = sin(theta) * ry;
		vx = cx + vx_*cRotation - vy_*sRotation;
		vy = cy + vx_*sRotation + vy_*cRotation;
		vertices.push_back(SDL_Vertex{.position={vx,vy},.color={color.GetR(),color.GetG(),color.GetB(),color.GetA()},.tex_coord={vx,vy}});
	}
	std::vector<int> indices;
	indices.reserve(segments*3+1);
	for (size_t i{1}; i<segments; ++i)
	{
		indices.push_back(0); indices.push_back(i); indices.push_back(i+1);
	}
	indices.push_back(0); indices.push_back(segments); indices.push_back(1);

	SDL_RenderGeometry(renderer,nullptr,vertices.data(),static_cast<int>(vertices.size()),indices.data(),static_cast<int>(indices.size()));
}

inline void RenderEllipse(SDL_Renderer* renderer, float cx, float cy, float rx, float ry, float rotation, ColorF color, float dr=0.1, int segments=100)
{
	std::vector<SDL_Vertex> vertices;
	vertices.reserve(2*segments);
	float drHalf = dr*0.5;
	float innerRx = rx - drHalf;
	float outerRx = rx + drHalf;
	float innerRy = ry - drHalf;
	float outerRy = ry + drHalf;
	float vx{}, vx_{}, vy{}, vy_{}, theta{}, cosTheta{}, sinTheta{};
	const float step = static_cast<float>(2.0*std::numbers::pi/segments);
	const float sRotation = sin(rotation), cRotation = cos(rotation);
	for (size_t i{}; i<segments; ++i)
	{
		theta = step*i;
		cosTheta = cos(theta);
		sinTheta = sin(theta);
		vx_ = cosTheta * innerRx;
		vy_ = sinTheta * innerRy;
		vx = cx + vx_*cRotation - vy_*sRotation;
		vy = cy + vx_*sRotation + vy_*cRotation;
		vertices.push_back(SDL_Vertex{.position={vx,vy},.color={color.GetR(),color.GetG(),color.GetB(),color.GetA()},.tex_coord={0.0f,0.0f}});
		vx_ = outerRx * cosTheta;
		vy_ = outerRy * sinTheta;
		vx = cx + vx_*cRotation - vy_*sRotation;
		vy = cy + vx_*sRotation + vy_*cRotation;
		vertices.push_back(SDL_Vertex{.position={vx,vy},.color={color.GetR(),color.GetG(),color.GetB(),color.GetA()},.tex_coord={0.0f,0.0f}});
	}
	std::vector<int> indices;
	indices.reserve(segments*6);
	indices.push_back(2*segments-1); indices.push_back(0); indices.push_back(1);
	for (size_t i{1}; i<2*segments-1; ++i)
	{
		indices.push_back(i-1); indices.push_back(i); indices.push_back(i+1);
	}
	indices.push_back(2*segments-2); indices.push_back(2*segments-1); indices.push_back(0);

	SDL_RenderGeometry(renderer,nullptr,vertices.data(),static_cast<int>(vertices.size()),indices.data(),static_cast<int>(indices.size()));
}

inline void RenderCircleMidpoint(SDL_Renderer* renderer, float cx, float cy, float radius)
{
	float x{radius}, y{}, err{};
	// int cx = static_cast<int>(cxf), cy = static_cast<int>(cyf);
	while (x>=y)
	{
		SDL_RenderPoint(renderer,cx+x,cy+y);
		SDL_RenderPoint(renderer,cx+x,cy-y);
		SDL_RenderPoint(renderer,cx+y,cy+x);
		SDL_RenderPoint(renderer,cx+y,cy-x);
		SDL_RenderPoint(renderer,cx-x,cy+y);
		SDL_RenderPoint(renderer,cx-x,cy-y);
		SDL_RenderPoint(renderer,cx-y,cy+x);
		SDL_RenderPoint(renderer,cx-y,cy-x);

		if (err<=0.0)
		{
			y += 0.1;
			err += 2*y+1;
		}
		else
		{
			x -= 0.1;
			err -= 2*x+1;
		}
	}
}

