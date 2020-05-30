#include "BulletManager.h"

#include <cmath>

#include <thread>

#include <iostream>

#include "Graphics.h"

namespace
{
	constexpr int wallsToGenerate = 600;
	constexpr int bulletsToGenerate = 10;
}

static std::vector<BulletManager::Wall> CreateWallDefinitions()
{
	std::vector<BulletManager::Wall> walls;


	for (int wallIndex = 0; wallIndex < wallsToGenerate; ++wallIndex)
	{
		const float index11 = static_cast<float>(wallIndex * 2);

		const float index12 = static_cast<float>(wallIndex % 10) * 10;

		const BulletManager::Wall wall{ BulletManager::WallDefinition(Vector2{index11, index12 + 10}, Vector2{index11, index12 + 20}) };

		walls.push_back(wall);
	}

	return walls;
}

static std::vector<BulletManager::Bullet> CreateBulletDefinitions()
{
	std::vector<BulletManager::Bullet> bullets;

	//const BulletManager::Bullet bullet{ BulletManager::BulletDefinition(Vector2{static_cast<float>(wallsToGenerate), 15}, Vector2{-1000, 0}, 0, 1000) };

	//bullets.push_back(bullet);

	for (int bulletIndex = 0; bulletIndex < bulletsToGenerate - 1; ++bulletIndex)
	{
		const float index12 = static_cast<float>(bulletIndex % 10) * 10;
		//const BulletManager::Bullet bullet{ BulletManager::BulletDefinition(Vector2{static_cast<float>(bulletIndex), 30}, Vector2{0, 10}, 0, 1000) };
		const BulletManager::Bullet bullet{ BulletManager::BulletDefinition(Vector2{static_cast<float>((wallsToGenerate/* + bulletIndex*/)), index12 + 15}, Vector2{static_cast<float>(10), 0}, 0, 1000) };

		bullets.push_back(bullet);
	}

	return bullets;
}



BulletManager BulletManager::CreateManager()
{
	static std::vector<Wall> walls = CreateWallDefinitions();

	static std::vector<Bullet> bullets = CreateBulletDefinitions();

	BulletManager manager;
	manager.walls = walls;
	manager.bullets = bullets;
	return manager;
}

void BulletManager::GenerateState(WorldState& outGraphicsState) const
{
	for (const auto& bullet : bullets)
	{
		outGraphicsState.bullets.push_back({bullet.definition.startingPosition + bullet.definition.velocity * (currentTime - bullet.definition.startTime), bullet.definition.velocity});
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

struct WvsB
{
	int bulletIndex = -1;
	float time = std::numeric_limits<float>::max();
};

struct BvsW
{
	int wallIndex = -1;
	float time = std::numeric_limits<float>::max();
};

template <class StageLogic>
struct ParallelStage
{
	typedef StageLogic TStage;

	ParallelStage(const TStage& InStage, bool bInUseThread = false) : Stage(InStage), bUseThread(bInUseThread)
	{

	}

	void StartWork()
	{
		if (bUseThread)
		{
			StageThread = std::thread(&ParallelStage<StageLogic>::DoWork, this);
		}
		else
		{
			DoWork();
		}
	}

	void DoWork()
	{
		Stage.DoWork();
	}

	void FinishWork()
	{
		if (bUseThread)
		{
			StageThread.join();
			StageThread = std::thread();
		}
	}

	TStage Stage;
	bool bUseThread = false;
	std::thread StageThread;
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

					WvsB& data = calculatedWalls[calculatedWallIndex];

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

	std::vector<WvsB> calculatedWalls;

	bool bWereAnyCollisionHitsFound = false;
};

struct BulletManager::ApplyBulletStage
{
	struct Setup
	{
		Setup(
			int startBulletIndex,
		int endBulletIndex,

		const std::vector<BvsW>& bulletsVsWall,

		std::vector<Bullet>& bullets,

		std::vector<Wall>& walls):startBulletIndex(startBulletIndex), endBulletIndex(endBulletIndex), bulletsVsWall(bulletsVsWall), bullets(bullets), walls(walls)
		{
		}

		int startBulletIndex;
		int endBulletIndex;

		const std::vector<BvsW>& bulletsVsWall;

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
			const BvsW& bulletData = setup.bulletsVsWall[bulletIndex];

			if (bulletData.wallIndex < 0)
			{
				continue;
			}

			Bullet& bullet = setup.bullets[bulletIndex];

			bullet.definition.startingPosition = EvaluateBulletLocation(bullet.definition, bulletData.time);
			bullet.definition.startTime = bulletData.time;
			// TODO: rewrite normal bounce calculation
			bullet.definition.velocity.X = -bullet.definition.velocity.X;

			Wall& wall = setup.walls[bulletData.wallIndex];

			wall.timeDestroyed = bulletData.time;
		}
	}

	Setup setup;
};

template <class StageLogic>
std::vector<ParallelStage<StageLogic>> RunStage(std::function<StageLogic(int)> allocator, int stagesCount, bool bUseThreads = false)
{
	std::vector<ParallelStage<StageLogic>> stages;

	stages.reserve(stagesCount);

	for (int stageIndex = 0; stageIndex < stagesCount; ++stageIndex)
	{
		stages.push_back(ParallelStage<StageLogic>(allocator(stageIndex), bUseThreads));

		stages.rbegin()->StartWork();
	}

	for (int stageIndex = 0; stageIndex < stagesCount; ++stageIndex)
	{
		stages[stageIndex].FinishWork();
	}

	return stages;
}

void BulletManager::Update(const float deltaTime)
{
	const float time = currentTime + deltaTime;

	constexpr static int defaultThreadsToUse = 4;

	int threadsToUse = std::thread::hardware_concurrency();

	if (threadsToUse == 0)
	{
		threadsToUse = defaultThreadsToUse;
	}

	std::vector<std::thread> workerThreads(threadsToUse);

	while (true)
	{
		std::vector<WvsB> wallVsBullets(walls.size());

		std::vector<BvsW> bulletsVsWall(bullets.size());

		bool bWereAnyCollisionHitsFound = false;

		const int filterStagesCount = threadsToUse;

		auto filterStages = RunStage<FilterStage>([this, filterStagesCount, time](int filterStageIndex) {
			const int startingWallIndex = (walls.size() * filterStageIndex) / filterStagesCount;

			const int endWallIndex = (walls.size() * (filterStageIndex + 1)) / filterStagesCount;

			return FilterStage(FilterStage::Setup(startingWallIndex, endWallIndex, currentTime, time, walls, bullets)); }
			, filterStagesCount, true);

		for (const auto& parallelStage : filterStages)
		{
			const FilterStage& stage = parallelStage.Stage;

			bWereAnyCollisionHitsFound |= stage.bWereAnyCollisionHitsFound;
			for (int calculatedWallIndex = 0; calculatedWallIndex < stage.calculatedWalls.size(); ++calculatedWallIndex)
			{
				const int actualWallIndex = calculatedWallIndex + stage.setup.startWallIndex;

				const WvsB& calculatedData = stage.calculatedWalls[calculatedWallIndex];

				WvsB& data = wallVsBullets[actualWallIndex];

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
			WvsB& data = wallVsBullets[wallIndex];

			if (data.bulletIndex < 0)
			{
				continue;
			}

			BvsW& bulletData = bulletsVsWall[data.bulletIndex];

			if (data.time < bulletData.time)
			{
				bulletData.time = data.time;
				bulletData.wallIndex = wallIndex;
			}
		}

		const int applyBulletsStagesCount = threadsToUse;

		RunStage<ApplyBulletStage>([this, applyBulletsStagesCount, &bulletsVsWall](int stageIndex)->ApplyBulletStage {
			ApplyBulletStage Stage(ApplyBulletStage::Setup((bullets.size() * stageIndex) / applyBulletsStagesCount, (bullets.size() * (stageIndex + 1)) / applyBulletsStagesCount, bulletsVsWall, bullets, walls));
			return Stage;
			}, applyBulletsStagesCount, true);
	}

	currentTime = time;
}

void BulletManager::UpdateFromTo(std::vector<Wall>& walls, std::vector<Bullet>& bullets, const float timeFrom, const float timeTo) const
{

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


