#pragma once

#include "Common.h"

#include <vector>

struct WorldState
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

class SDLAll
{
public:
	SDLAll();

	~SDLAll();

	InputResult GetInput(Vector2& start, Vector2& end);

	void Render(const WorldState& worldState);

	void Sleep(int millisecondsToSleep);

	bool bWasSdlInitialized = false;
	struct SDL_Window* win = nullptr;
	struct SDL_Renderer* ren = nullptr;
};
