#pragma once

struct Vector2
{
	float X;
	float Y;

	static const Vector2 Zero;

	bool Equals(const Vector2& other, float threshold = 0.0001) const
	{
		const Vector2 difference = (*this) - other;

		return difference.GetSquareMagnitude() <= (threshold * threshold);
	}

	float GetSquareMagnitude() const
	{
		return X * X + Y * Y;
	}

	float GetMagnitude() const;

	Vector2 operator-() const
	{
		return { -X, -Y };
	}

	Vector2 operator+(const Vector2& other) const
	{
		return { X + other.X, Y + other.Y };
	}

	Vector2 operator-(const Vector2& other) const
	{
		return (*this) + (-other);
	}

	Vector2 operator*(float multiplier) const
	{
		return { X * multiplier, Y * multiplier };
	}
};

enum class InputResult
{
	None,
	Exit,
	LaunchBullet,
};
