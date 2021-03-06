#pragma once

#include "Common.h"

#include <vector>

#include <mutex>

class BulletManager
{
public:
	struct WallDefinition
	{
		WallDefinition() : WallDefinition(Vector2::Zero, Vector2::Zero) {}

		WallDefinition(const Vector2& inStart, const Vector2& inEnd) : start(inStart), end(inEnd), change(inEnd - inStart), freeTerm(inStart.X* inEnd.Y - inStart.Y * inEnd.X)
		{
		}

		Vector2 start;
		Vector2 end;

		Vector2 change;

		float freeTerm;
	};

	struct BulletDefinition
	{
		BulletDefinition() : BulletDefinition(Vector2::Zero, Vector2::Zero, 0, 0)
		{

		}

		BulletDefinition(const Vector2& inStartingPosition, const Vector2& inVelocity, float inStartTime, float inLifetime) : startingPosition(inStartingPosition), velocity(inVelocity), startTime(inStartTime), lifetime(inLifetime)
		{
		}

		Vector2 startingPosition;
		Vector2 velocity;
		float startTime;
		float lifetime;
	};

	BulletManager(const std::vector<WallDefinition>& inWallDefinitions, const std::vector<BulletDefinition>& inBulletDefinitions);

	~BulletManager();

	void Update(float time);

	void AddBullet(const Vector2& position, const Vector2& velocity, float time, float lifetime);

	void GenerateState(struct GraphicsState& outGraphicsState) const;

	struct Wall
	{
		WallDefinition definition;

		float timeDestroyed = -1;
	};

	struct Bullet
	{
		BulletDefinition definition;
	};

	struct FilterStage;

	struct ApplyBulletStage;

	static bool TryGetTimeDestroyed(WallDefinition wall, BulletDefinition bullet, float& outTime);

	static bool TryGetCollisionPoint(WallDefinition wall, BulletDefinition bullet, Vector2& outCollisionPoint);

	static Vector2 EvaluateBulletLocation(BulletDefinition bullet, float time);

private:
	std::mutex bulletAdditionMutex;

	static bool TryGetCollinearBulletCollisionTime(WallDefinition wall, BulletDefinition bullet, float& outTime);

	static bool CanCollide(const Wall& wall, const Bullet& bullet, float startingTime, float targetTime);

	float currentTime = 0;

	int threadsToUse = -1;

	std::vector<Wall> walls;

	std::vector<Bullet> bullets;

	std::unique_ptr<class ThreadPool> threadPool;
};
