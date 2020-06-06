#include "BulletManager.h"

#include <cmath>

#include <thread>

#include <iostream>

#include <functional>

#include "Graphics.h"

#include "ParallelUtils.h"

BulletManager::BulletManager(const std::vector<WallDefinition>& inWallDefinitions, const std::vector<BulletDefinition>& inBulletDefinitions)
{
	walls.reserve(inWallDefinitions.size());
	for (const WallDefinition& wallDefinition : inWallDefinitions)
	{
		walls.push_back({ wallDefinition });
	}

	bullets.reserve(inBulletDefinitions.size());
	for (const BulletDefinition& bulletDefinition : inBulletDefinitions)
	{
		bullets.push_back({ bulletDefinition });
	}

	constexpr static int defaultThreadsToUse = 4;

	threadsToUse = std::thread::hardware_concurrency();

	if (threadsToUse == 0)
	{
		threadsToUse = defaultThreadsToUse;
	};

	threadPool = std::make_unique<ThreadPool>(threadsToUse);
}

BulletManager::~BulletManager() = default;


void BulletManager::AddBullet(const Vector2& position, const Vector2& velocity, float time, float lifetime)
{
	std::unique_lock<std::mutex> bulletAdditionLock(bulletAdditionMutex);

	bullets.push_back(Bullet{ BulletDefinition(position, velocity, time, lifetime) });
}

void BulletManager::GenerateState(GraphicsState& outGraphicsState) const
{
	for (const auto& bullet : bullets)
	{
		if (bullet.definition.startTime < currentTime && currentTime < (bullet.definition.startTime + bullet.definition.lifetime))
		{
			outGraphicsState.bullets.push_back({ bullet.definition.startingPosition + bullet.definition.velocity * (currentTime - bullet.definition.startTime), bullet.definition.velocity });
		}
	}

	for (const auto& wall : walls)
	{
		if (wall.timeDestroyed >= 0)
		{
			continue;
		}

		outGraphicsState.walls.push_back({ wall.definition.start, wall.definition.end });
	}
}

struct WallDestructionData
{
	int bulletIndex = -1;
	float time = std::numeric_limits<float>::max();
};

struct BulletHitData
{
	int wallIndex = -1;
	float time = std::numeric_limits<float>::max();
};

struct BulletManager::FilterStage
{
	struct Setup
	{
		Setup(int startWallIndex,

			int endWallIndex,

			float startTime,
			float endTime,

			const std::vector<Wall>& walls,

			const std::vector<Bullet>& bullets) : startWallIndex(startWallIndex), endWallIndex(endWallIndex), startTime(startTime), endTime(endTime), walls(walls), bullets(bullets)
		{}



		int startWallIndex;

		int endWallIndex;

		float startTime;
		float endTime;

		const std::vector<Wall>& walls;

		const std::vector<Bullet>& bullets;
	};

	FilterStage(const Setup& setup) : setup(setup),
		calculatedWalls(setup.endWallIndex - setup.startWallIndex)
	{

	}

	void DoWork()
	{
		for (int bulletIndex = 0; bulletIndex < setup.bullets.size(); ++bulletIndex)
		{
			//std::cout << "Starting bullet " << bulletIndex << std::endl;
			const Bullet& bullet = setup.bullets[bulletIndex];

			for (int wallIndex = setup.startWallIndex; wallIndex < setup.endWallIndex; ++wallIndex)
			{
				const Wall& wall = setup.walls[wallIndex];

				if (!CanCollide(wall, bullet, setup.startTime, setup.endTime))
				{
					continue;
				}

				float timeToHit;
				if (TryGetTimeDestroyed(wall.definition, bullet.definition, timeToHit) && timeToHit < setup.endTime)
				{
					const int calculatedWallIndex = wallIndex - setup.startWallIndex;

					WallDestructionData& data = calculatedWalls[calculatedWallIndex];

					if (timeToHit < data.time)
					{
						bWereAnyCollisionHitsFound = true;
						data.time = timeToHit;
						data.bulletIndex = bulletIndex;
					}
				}
			}
		}
	
		//printf("Done work\r\n");
	}

	Setup setup;

	std::vector<WallDestructionData> calculatedWalls;

	bool bWereAnyCollisionHitsFound = false;
};

struct BulletManager::ApplyBulletStage
{
	struct Setup
	{
		Setup(
			int startBulletIndex,
		int endBulletIndex,

		const std::vector<BulletHitData>& bulletsVsWall,

		std::vector<Bullet>& bullets,

		std::vector<Wall>& walls):startBulletIndex(startBulletIndex), endBulletIndex(endBulletIndex), bulletsVsWall(bulletsVsWall), bullets(bullets), walls(walls)
		{
		}

		int startBulletIndex;
		int endBulletIndex;

		const std::vector<BulletHitData>& bulletsVsWall;

		std::vector<Bullet>& bullets;

		std::vector<Wall>& walls;
	};

	ApplyBulletStage(const Setup& setup) : setup(setup)
	{

	}

	void DoWork()
	{
		for (int bulletIndex = setup.startBulletIndex; bulletIndex < setup.endBulletIndex; ++bulletIndex)
		{
			const BulletHitData& bulletData = setup.bulletsVsWall[bulletIndex];

			if (bulletData.wallIndex < 0)
			{
				continue;
			}

			Wall& wall = setup.walls[bulletData.wallIndex];

			wall.timeDestroyed = bulletData.time;

			Bullet& bullet = setup.bullets[bulletIndex];

			bullet.definition.startingPosition = EvaluateBulletLocation(bullet.definition, bulletData.time);

			const float timePassedSinceBulletStart = bulletData.time - bullet.definition.startTime;

			bullet.definition.lifetime -= timePassedSinceBulletStart;

			bullet.definition.startTime = bulletData.time;

			const Vector2 normal = wall.definition.change.GetNormal().Normalized();

			const Vector2 reflectedVelocity = bullet.definition.velocity - normal * (2 * Vector2::DotProduct(bullet.definition.velocity, normal));

			bullet.definition.velocity = reflectedVelocity;
		}
	}

	Setup setup;
};

template <class TContainer>
std::pair<int, int> GetInterval(const TContainer& container, int totalParts, int partIndex)
{
	const int containerSize = static_cast<int>(container.size());

	return std::make_pair((containerSize * partIndex) / totalParts, (containerSize * (partIndex + 1)) / totalParts);
}

void BulletManager::Update(const float deltaTime)
{
	const float time = currentTime + deltaTime;

	ThreadPool& pool = *threadPool;

	//std::vector<std::thread> workerThreads(threadsToUse);

	std::unique_lock<std::mutex> bulletsLock(bulletAdditionMutex);

	while (true)
	{
		std::vector<WallDestructionData> wallVsBullets(walls.size());

		std::vector<BulletHitData> bulletsVsWall(bullets.size());

		bool bWereAnyCollisionHitsFound = false;

		const int filterStagesCount = threadsToUse;

		auto filterStages = RunStage<FilterStage>([this, filterStagesCount, time](int filterStageIndex) {
			const auto interval = GetInterval(walls, filterStagesCount, filterStageIndex);

			const int startingWallIndex = interval.first;

			const int endWallIndex = interval.second;

			return FilterStage(FilterStage::Setup(startingWallIndex, endWallIndex, currentTime, time, walls, bullets)); }
			, filterStagesCount, pool);

		for (const auto& parallelStage : filterStages)
		{
			const FilterStage& stage = parallelStage;

			bWereAnyCollisionHitsFound |= stage.bWereAnyCollisionHitsFound;
			for (int calculatedWallIndex = 0; calculatedWallIndex < stage.calculatedWalls.size(); ++calculatedWallIndex)
			{
				const int actualWallIndex = calculatedWallIndex + stage.setup.startWallIndex;

				const WallDestructionData& calculatedData = stage.calculatedWalls[calculatedWallIndex];

				WallDestructionData& data = wallVsBullets[actualWallIndex];

				if (calculatedData.time < data.time)
				{
					if (calculatedData.bulletIndex > data.bulletIndex)
					{
						data = calculatedData;
					}
				}
			}
		}

		if (!bWereAnyCollisionHitsFound)
		{
			break;
		}

		for (int wallIndex = 0; wallIndex < walls.size(); ++wallIndex)
		{
			WallDestructionData& data = wallVsBullets[wallIndex];

			if (data.bulletIndex < 0)
			{
				continue;
			}

			BulletHitData& bulletData = bulletsVsWall[data.bulletIndex];

			if (data.time < bulletData.time)
			{
				bulletData.time = data.time;
				bulletData.wallIndex = wallIndex;
			}
		}

		const int applyBulletsStagesCount = threadsToUse;

		auto bulletStage = RunStage<ApplyBulletStage>([this, applyBulletsStagesCount, &bulletsVsWall](int stageIndex)->ApplyBulletStage {

			const auto interval = GetInterval(bullets, applyBulletsStagesCount, stageIndex);

			ApplyBulletStage Stage(ApplyBulletStage::Setup(interval.first, interval.second, bulletsVsWall, bullets, walls));
			return Stage;
			}, applyBulletsStagesCount, pool);
	}

	currentTime = time;
}

bool BulletManager::TryGetTimeDestroyed(WallDefinition wall, BulletDefinition bullet, float& outTime)
{
	if (bullet.velocity.Equals(Vector2::Zero) || wall.change.Equals(Vector2::Zero))
	{
		return false;
	}

	float collisionTime;

	const float denominator = bullet.velocity.X * wall.change.Y - bullet.velocity.Y * wall.change.X ;

	if (std::abs(denominator) < 0.0001f)
	{
		if (!TryGetCollinearBulletCollisionTime(wall, bullet, collisionTime))
		{
			return false;
		}
	}
	else
	{
		const float numerator = wall.change.X * bullet.startingPosition.Y - wall.change.Y * bullet.startingPosition.X + wall.freeTerm;

		collisionTime = numerator / denominator;

		struct NormalizedWallLocationHelper
		{
			NormalizedWallLocationHelper(float Vector2::* memberToUse) : memberToUse(memberToUse){}

			float GetVectorComponent(const Vector2& vector) const
			{
				return vector.*memberToUse;
			}

			float GetWallLocation(const WallDefinition& wall, const BulletDefinition& bullet, float collisionTime)
			{
				return (GetVectorComponent(bullet.startingPosition) + GetVectorComponent(bullet.velocity) * collisionTime - GetVectorComponent(wall.start)) / GetVectorComponent(wall.change);
			}

			float Vector2::* memberToUse;
		};

		float Vector2::* const ComponentToUse = std::abs(wall.change.X) < 0.0001f ? &Vector2::Y : &Vector2::X;

		const float normalizedWallLocation = NormalizedWallLocationHelper(ComponentToUse).GetWallLocation(wall, bullet, collisionTime);

		if (normalizedWallLocation < 0 || normalizedWallLocation > 1)
		{
			// bullet didn't actually hit the wall
			return false;
		}
	}

	if (collisionTime >= 0 && collisionTime < bullet.lifetime)
	{
		outTime = collisionTime + bullet.startTime;

		return true;
	}

	return false;
}

bool BulletManager::TryGetCollinearBulletCollisionTime(WallDefinition wall, BulletDefinition bullet, float& outTime)
{
	// bullet might be parallel to the wall, or it might be on the same line as it
	const Vector2 bulletShift = wall.start - bullet.startingPosition;

	const float bulletStartTimeX = bulletShift.X / bullet.velocity.X;
	const float bulletStartTimeY = bulletShift.Y / bullet.velocity.Y;

	if (std::abs(bulletStartTimeX - bulletStartTimeY) >= 0.0001f)
	{
		// the bullet has to travel for a different time on two axis,
		// it won't intersect with the wall
		return false;
	}

	const float bulletEndTimeX = (wall.end.X - bullet.startingPosition.X) / bullet.velocity.X;

	float collisionTime;

	if (std::signbit(bulletStartTimeX) == std::signbit(bulletEndTimeX))
	{
		// the signs on time to both sides are the same; that means
		// the bullet approaches the wall from a side. We should pick the closest 
		// position which corresponds to the time that is the closest to zero
		collisionTime = std::copysign(std::fmax(std::abs(bulletStartTimeX), std::abs(bulletEndTimeX)), bulletStartTimeX);
	}
	else
	{
		// the signs on time to both sides are different, that means that the bullet would spawn inside of a wall
		// so it should collide right away
		collisionTime = 0;
	}

	outTime = collisionTime;
	return true;
}

bool BulletManager::CanCollide(const Wall& wall, const Bullet& bullet, float startingTime, float targetTime)
{
	if (wall.timeDestroyed >= 0)
	{
		return false;
	}

	if (targetTime < bullet.definition.startTime)
	{
		return false;
	}

	if (bullet.definition.startTime + bullet.definition.lifetime < startingTime)
	{
		return false;
	}

	const Vector2 bulletStartingLocation = EvaluateBulletLocation(bullet.definition, startingTime);
	
	const Vector2 fromWallToBullet = wall.definition.start - bulletStartingLocation;

	const float bulletMovementTime = targetTime - startingTime;

	const Vector2 maxMovedPosition = EvaluateBulletLocation(bullet.definition, targetTime);

	const float bulletTravelDistanceSquare = (maxMovedPosition - bulletStartingLocation).GetSquareMagnitude();

	if (fromWallToBullet.GetSquareMagnitude() < bulletTravelDistanceSquare)
	{
		return true;
	}

	const auto wallChange = wall.definition.change;

	const auto wallChangeNormalized = wallChange.Normalized();

	const Vector2 boundingSquareSide = wallChangeNormalized * bulletTravelDistanceSquare;


	const float equationA = Vector2::DotProduct(wallChange, wallChange);

	const float equationB = 2 * Vector2::DotProduct(wallChange, fromWallToBullet);

	const float equationC = Vector2::DotProduct(fromWallToBullet, fromWallToBullet) - bulletTravelDistanceSquare;

	const float discriminant = equationB * equationB - 4 * equationA * equationC;

	if (discriminant < 0)
	{
		return false;
	}

	const float discriminantRoot = std::sqrtf(discriminant);

	const float t1 = (-equationB - discriminantRoot) / (2 * equationA);
	const float t2 = (-equationB + discriminantRoot) / (2 * equationA);

	return t1 <= 1 && t2 >= 0;
}


bool BulletManager::TryGetCollisionPoint(WallDefinition wall, BulletDefinition bullet, Vector2& outCollisionPoint)
{
	float collisionTime;

	if (TryGetTimeDestroyed(wall, bullet, collisionTime))
	{
		outCollisionPoint = EvaluateBulletLocation(bullet, collisionTime);
		return true;
	}

	return false;
}

Vector2 BulletManager::EvaluateBulletLocation(BulletDefinition bullet, float time)
{
	const float movementTime = std::fmaxf(0, time - bullet.startTime);

	return bullet.startingPosition + bullet.velocity * movementTime;
}


