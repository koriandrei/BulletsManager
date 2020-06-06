#pragma once

#include "Common.h"

#include <vector>

struct GraphicsState
{
	struct Wall
	{
		Vector2 start;
		Vector2 end;
	};

	struct Bullet
	{
		Vector2 location;
		Vector2 velocity;
	};

	std::vector<Wall> walls;
	std::vector<Bullet> bullets;
};

class GraphicsSystem
{
public:
	GraphicsSystem();

	~GraphicsSystem();

	InputResult GetInput(Vector2& start, Vector2& end);

	void Render(const GraphicsState& GraphicsState);

	void Sleep(int millisecondsToSleep);

	bool bWereGraphicsInitialized = false;
private:
	struct SDL_Window* win = nullptr;
	struct SDL_Renderer* ren = nullptr;
};
