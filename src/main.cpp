#include <iostream>
#include "SDL.h"

#include "SDL_video.h"

class SDLAll
{
public:
	SDLAll()
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

	~SDLAll()
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

	bool bWasSdlInitialized = false;
	SDL_Window* win = nullptr;
	SDL_Renderer* ren = nullptr;
};

struct Vector2
{
	int X;
	int Y;
};

enum class InputResult
{
	None,
	Exit,
	LaunchBullet,
};

InputResult HandleInput(const SDL_Event& Event, Vector2& start, Vector2& end)
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

int main(int, char**){
	SDLAll SDL;

	if (!SDL.bWasSdlInitialized)
	{
		return 1;
	}

	Vector2 StartInput;
	Vector2 EndInput;

	bool bShouldRun = true;
	unsigned char TickId = 0;
	while (bShouldRun)
	{
		SDL_Renderer* ren = SDL.ren;

		SDL_SetRenderDrawColor(ren, TickId, TickId, TickId, 255);
		
		++TickId;

		SDL_Event Event;

		if (SDL_PollEvent(&Event) != 0)
		{
			const InputResult Result = HandleInput(Event, StartInput, EndInput);

			bShouldRun = Result != InputResult::Exit;

			if (Result == InputResult::LaunchBullet)
			{
				std::cout << "Input " << StartInput.X << ":" << StartInput.Y << " to " << EndInput.X << ":" << EndInput.Y << std::endl;
			}
		}

		//First clear the renderer
		SDL_RenderClear(ren);

		SDL_RenderPresent(ren);

		//Take a quick break after all that hard work
		SDL_Delay(16);
	}

	return 0;
}
