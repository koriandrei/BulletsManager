#include "Common.h"

#include <cmath>

const Vector2 Vector2::Zero{ 0,0 };

float Vector2::GetMagnitude() const
{
	return std::sqrtf(GetSquareMagnitude());
}

