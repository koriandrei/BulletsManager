#include "Graphics.h"

#include "SDL.h"

#include <iostream>


SDLAll::SDLAll()
{
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		return;
	}

	bWasSdlInitialized = true;

	win = SDL_CreateWindow("Hello World!", 100, 100, 640, 480, SDL_WINDOW_SHOWN);
	if (win == nullptr)
	{
		return;
	}

	ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (ren == nullptr)
	{
		return;
	}
}

SDLAll::~SDLAll()
{
	if (bWasSdlInitialized)
	{
		if (win != nullptr)
		{
			if (ren != nullptr)
			{
				SDL_DestroyRenderer(ren);
			}
			else
			{
				std::cout << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
			}

			SDL_DestroyWindow(win);
		}
		else
		{
			std::cout << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
		}

		SDL_Quit();
	}
	else
	{
		std::cout << "SDL_Init Error: " << SDL_GetError() << std::endl;
	}
}

static InputResult GetInputInternal(const SDL_Event& Event, Vector2& start, Vector2& end)
{
	const bool bIsLeftMouseButton = Event.button.button == SDL_BUTTON_LEFT;

	if (Event.type == SDL_EventType::SDL_KEYDOWN)
	{
		if (Event.key.keysym.scancode == SDL_Scancode::SDL_SCANCODE_ESCAPE)
		{
			return InputResult::Exit;
		}
	}

	if (bIsLeftMouseButton)
	{
		const bool bIsButtonDown = Event.type == SDL_EventType::SDL_MOUSEBUTTONDOWN;
		const bool bIsButtonUp = Event.type == SDL_EventType::SDL_MOUSEBUTTONUP;

		Vector2 Input{ Event.button.x, Event.button.y };


		if (bIsButtonDown)
		{
			start = Input;
		}

		if (bIsButtonUp)
		{
			end = Input;

			return InputResult::LaunchBullet;
		}
	}

	return InputResult::None;
}

InputResult SDLAll::GetInput(Vector2& start, Vector2& end)
{
	SDL_Event Event;

	while (SDL_PollEvent(&Event) != 0)
	{
		InputResult result = GetInputInternal(Event, start, end);

		if (result != InputResult::None)
		{
			return result;
		}
	}

	return InputResult::None;
}

void SDLAll::Render(const WorldState& worldState)
{
	SDL_SetRenderDrawColor(ren, 0,0,0,255);

	//First clear the renderer
	SDL_RenderClear(ren);

	SDL_SetRenderDrawColor(ren, 200, 0, 0, 255);

	for (const WorldState::Wall& wall : worldState.walls)
	{
		SDL_RenderDrawLine(ren, wall.start.X, wall.start.Y, wall.end.X, wall.end.Y);
	}

	SDL_SetRenderDrawColor(ren, 0, 255, 0, 255);

	for (const WorldState::Bullet& bullet : worldState.bullets)
	{
		const SDL_Rect bulletRect{bullet.location.X, bullet.location.Y, 12, 12};

		SDL_RenderDrawRect(ren, &bulletRect);
	}

	SDL_RenderPresent(ren);
}

void SDLAll::Sleep(int millisecondsToSleep)
{
	SDL_Delay(millisecondsToSleep);
}

