#pragma once

#include "Common.h"

#include <vector>

class BulletManager
{
public:
	void Update(float time);

	void AddWall(const Vector2& start, const Vector2& end);

	void AddBullet(const Vector2& position, const Vector2& velocity, float time, float lifetime);

	struct WallDefinition
	{
		WallDefinition(const Vector2& inStart, const Vector2& inEnd) : start(inStart), end(inEnd), change(inStart - inEnd), freeTerm(inStart.X * inEnd.Y - inStart.Y * inEnd.X)
		{
		}

		const Vector2 start;
		const Vector2 end;

		const Vector2 change;

		const float freeTerm;
	};

	struct BulletDefinition
	{
		BulletDefinition(const Vector2& inStartingPosition, const Vector2& inVelocity, float inStartTime, float inLifetime) : startingPosition(inStartingPosition), velocity(inVelocity), startTime(inStartTime), lifetime(inLifetime)
		{
		}

		const Vector2 startingPosition;
		const Vector2 velocity;
		const float startTime;
		const float lifetime;
	};

	struct Wall
	{
		const WallDefinition definition;

		float timeDestroyed = 0;
	};

	struct Bullet
	{
		const BulletDefinition definition;
	};

	static bool TryGetTimeDestroyed(WallDefinition wall, BulletDefinition bullet, float& outTime);

	static bool TryGetCollisionPoint(WallDefinition wall, BulletDefinition bullet, Vector2& outCollisionPoint);

	static Vector2 EvaluateBulletLocation(BulletDefinition bullet, float time);

private:
	static bool TryGetCollinearBulletCollisionTime(WallDefinition wall, BulletDefinition bullet, float& outTime);

	std::vector<Wall> walls;

	std::vector<Bullet> bullets;
};
