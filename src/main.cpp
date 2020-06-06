#include <iostream>

#include "Common.h"

#include "Graphics.h"
#include "BulletManager.h"

#include <chrono>

#include <fstream>

#include "nlohmann/json.hpp"

static void from_json(const nlohmann::json& j, Vector2& outVector)
{
	outVector = Vector2{ j["x"].get<float>(), j["y"].get<float>() };
}

static void from_json(const nlohmann::json& j, BulletManager::WallDefinition& outWallDefinition)
{
	outWallDefinition = BulletManager::WallDefinition(j["start"].get<Vector2>(), j["end"].get<Vector2>());
}

static void from_json(const nlohmann::json& j, BulletManager::BulletDefinition& outBulletDefinition)
{
	outBulletDefinition = BulletManager::BulletDefinition(j["start"].get<Vector2>(), j["velocity"].get<Vector2>(), 0, 10);
}

template <class T>
static std::vector<T> loadFromJson(std::string jsonPath, std::vector<T> defaultValue)
{
	std::ifstream wallsSetupInputStream(jsonPath);

	nlohmann::json wallSetupJson;

	if (wallsSetupInputStream)
	{
		wallsSetupInputStream >> wallSetupJson;
	}

	if (wallSetupJson.is_array())
	{
		return wallSetupJson.get<std::vector<T>>();
	}

	return defaultValue;
}

int main(int, char**)
{
	GraphicsSystem SDL;

	if (!SDL.bWereGraphicsInitialized)
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

	GraphicsState graphicsState;

	const auto appStartTime = clock.now();

	
	auto tickTimeAtTickEnd = clock.now();

	constexpr auto maxSimulationTickDuration = std::chrono::seconds(1);

	const std::string wallSetupFilePath("walls.json");
	
	const std::string bulletSetupFilePath("bullets.json");

	const auto walls = loadFromJson<BulletManager::WallDefinition>(wallSetupFilePath, { BulletManager::WallDefinition({ 10, 100 }, { 100, 100 }) });

	const auto bullets = loadFromJson<BulletManager::BulletDefinition>(bulletSetupFilePath, { });

	BulletManager bulletManager(walls, bullets);

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

		const auto timeBeforeBulletManagerUpdate = clock.now();

		const auto actualDeltaTime = timeBeforeBulletManagerUpdate - appStartTime;

		const auto deltaTimeForSimulation = ( actualDeltaTime < maxSimulationTickDuration ? actualDeltaTime : maxSimulationTickDuration);
		
		const float deltaTimeSeconds = ((float)std::chrono::duration_cast<std::chrono::microseconds>(deltaTimeForSimulation).count()) / std::micro::den;

		const float timeDilation = 1.0f;

		bulletManager.Update(timeDilation * deltaTimeSeconds);
		
		const auto timeAfterCalculation = clock.now();

		std::cout << "Calculated for " << (std::chrono::duration_cast<std::chrono::milliseconds>(timeAfterCalculation - timeBeforeBulletManagerUpdate)).count() << std::endl;

		GraphicsState graphicsState1;

		bulletManager.GenerateState(graphicsState1);


		SDL.Render(graphicsState1);

		const auto tickTimeAfterRender = clock.now();

		tickTimeAtTickEnd = clock.now();

		const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(tickTimeAtTickEnd - tickStartTime);

		const auto extraTickTime = targetDeltaTime - elapsedMs;

		//if (extraTickTime.count() > 0)
		{
			std::cout << "ticked for " << elapsedMs.count() << " sleep for " << extraTickTime.count() << std::endl;
			if (extraTickTime.count() > 0)
			{
				SDL.Sleep(static_cast<int>(extraTickTime.count()));
			}
		}
	}

	return 0;
}
