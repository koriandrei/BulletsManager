#include "BulletManager.h"

#include <cmath>

#include "Graphics.h"

namespace
{
	constexpr int wallsToGenerate = 500;
	constexpr int bulletsToGenerate = 1000;
}

static std::vector<BulletManager::Wall> CreateWallDefinitions()
{
	std::vector<BulletManager::Wall> walls;


	for (int wallIndex = 0; wallIndex < wallsToGenerate; ++wallIndex)
	{
		const float index11 = static_cast<float>(wallIndex * 2);

		const BulletManager::Wall wall{ BulletManager::WallDefinition(Vector2{index11, 10}, Vector2{index11, 20}) };

		walls.push_back(wall);
	}

	return walls;
}

static std::vector<BulletManager::Bullet> CreateBulletDefinitions()
{
	std::vector<BulletManager::Bullet> bullets;

	const BulletManager::Bullet bullet{ BulletManager::BulletDefinition(Vector2{static_cast<float>(wallsToGenerate), 15}, Vector2{-1000, 0}, 0, 1000) };

	bullets.push_back(bullet);

	for (int bulletIndex = 0; bulletIndex < bulletsToGenerate - 1; ++bulletIndex)
	{
		const BulletManager::Bullet bullet{ BulletManager::BulletDefinition(Vector2{static_cast<float>(bulletIndex), 30}, Vector2{0, 10}, 0, 1000) };

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

void BulletManager::Update(const float time)
{

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

	while (true)
	{

		std::vector<WvsB> wallVsBullets(walls.size());

		std::vector<BvsW> bulletsVsWall(bullets.size());

		bool bWereAnyCollisionHitsFound = false;

		for (int bulletIndex = 0; bulletIndex < bullets.size(); ++bulletIndex)
		{
			const Bullet& bullet = bullets[bulletIndex];

			for (int wallIndex = 0; wallIndex < walls.size(); ++wallIndex)
			{
				const Wall& wall = walls[wallIndex];

				if (!CanCollide(wall, bullet, currentTime, time))
				{
					continue;
				}

				float timeToHit;
				if (TryGetTimeDestroyed(wall.definition, bullet.definition, timeToHit) && timeToHit < time)
				{
					WvsB& data = wallVsBullets[wallIndex];

					if (timeToHit < data.time)
					{
						bWereAnyCollisionHitsFound = true;
						data.time = timeToHit;
						data.bulletIndex = bulletIndex;
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

		for (int bulletIndex = 0; bulletIndex < bullets.size(); ++bulletIndex)
		{
			BvsW& bulletData = bulletsVsWall[bulletIndex];

			if (bulletData.wallIndex < 0)
			{
				continue;
			}

			Bullet& bullet = bullets[bulletIndex];

			bullet.definition.startingPosition = EvaluateBulletLocation(bullet.definition, bulletData.time);
			bullet.definition.startTime = bulletData.time;
			// TODO: rewrite normal bounce calculation
			bullet.definition.velocity.X = -bullet.definition.velocity.X;

			Wall& wall = walls[bulletData.wallIndex];

			wall.timeDestroyed = bulletData.time;
		}
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

	const float denominator = bullet.velocity.Y * wall.change.X - bullet.velocity.X * wall.change.Y;

	if (std::abs(denominator) < 0.0001f)
	{
		if (!TryGetCollinearBulletCollisionTime(wall, bullet, collisionTime))
		{
			return false;
		}
	}
	else
	{
		const float numerator = bullet.startingPosition.X * wall.change.Y - bullet.startingPosition.Y * wall.change.X + wall.freeTerm;

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
	
	const Vector2 fromWallToBullet = wall.definition.start - EvaluateBulletLocation(bullet.definition, startingTime);

	const float bulletMovementTime = targetTime - startingTime;

	const float bulletTravelDistanceSquare = bullet.definition.velocity.GetSquareMagnitude() * bulletMovementTime * bulletMovementTime;

	const float equationA = Vector2::DotProduct(wall.definition.change, wall.definition.change);

	const float equationB = Vector2::DotProduct(wall.definition.change, fromWallToBullet);

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
	const float movementTime = time - bullet.startTime;

	return bullet.startingPosition + bullet.velocity * movementTime;
}


