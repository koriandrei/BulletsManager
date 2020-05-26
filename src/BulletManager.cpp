#include "BulletManager.h"

#include <cmath>

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


