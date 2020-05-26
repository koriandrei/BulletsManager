#include <iostream>

#include "Common.h"

#include "Graphics.h"
#include "BulletManager.h"

#include <chrono>

int main(int, char**)
{
	SDLAll SDL;

	if (!SDL.bWasSdlInitialized)
	{
		return 1;
	}

	Vector2 StartInput;
	Vector2 EndInput;

	bool bShouldRun = true;
	unsigned char TickId = 0;

	constexpr int targetFrameRate = 60;

	constexpr std::chrono::milliseconds targetDeltaTime(1000 / targetFrameRate);

	std::chrono::high_resolution_clock clock;

	WorldState graphicsState;

	while (bShouldRun)
	{
		const auto tickStartTime = clock.now();

		++TickId;

		while (true)
		{
			const InputResult Result = SDL.GetInput(StartInput, EndInput);

			if (Result == InputResult::None)
			{
				break;
			}

			const auto tickTimeAfterInput = clock.now();

			const bool bHasReceivedExitRequest = Result == InputResult::Exit;

			bShouldRun &= !bHasReceivedExitRequest;

			if (Result == InputResult::LaunchBullet)
			{
				std::cout << "Input " << StartInput.X << ":" << StartInput.Y << " to " << EndInput.X << ":" << EndInput.Y << std::endl;
				graphicsState.walls.push_back({ StartInput, EndInput });

				Vector2 hitLocation;
				if (BulletManager::TryGetCollisionPoint(BulletManager::WallDefinition(Vector2{ 0, 0 }, Vector2{ 1000, 1000 }), BulletManager::BulletDefinition(StartInput, EndInput - StartInput, 0, 1), hitLocation))
				{
					graphicsState.bullets.push_back({ hitLocation, {0,0} });
				}
				else
				{
					std::cout << "No hit" << std::endl;
				}
			}
		}

		const auto tickTimeAfterHandling = clock.now();

		SDL.Render(graphicsState);

		const auto tickTimeAfterRender = clock.now();

		const auto tickTimeAtTickEnd = clock.now();

		const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(tickTimeAtTickEnd - tickStartTime);

		const auto extraTickTime = targetDeltaTime - elapsedMs;

		if (extraTickTime.count() > 0)
		{
			std::cout << "ticked for " << elapsedMs.count() << " sleep for " << extraTickTime.count() << std::endl;
			SDL.Sleep(extraTickTime.count());
		}
	}

	return 0;
}
