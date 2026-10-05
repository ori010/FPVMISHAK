#include "Drone/FPVFlightMath.h"

float UFPVFlightMath::ComputeRateDegPerSec(float Stick, const FFPVAxisRates& Rates, EFPVRatesType RatesType)
{
	float Command = FMath::Clamp(Stick, -1.f, 1.f);
	const float CommandAbs = FMath::Abs(Command);
	float AngleRate = 0.f;

	if (RatesType == EFPVRatesType::Betaflight)
	{
		if (Rates.RcExpo > 0.f)
		{
			const float Expo = FMath::Clamp(Rates.RcExpo, 0.f, 1.f);
			Command = Command * CommandAbs * CommandAbs * CommandAbs * Expo + Command * (1.f - Expo);
		}

		float RcRate = Rates.RcRate;
		if (RcRate > 2.f)
		{
			RcRate += 14.54f * (RcRate - 2.f); // RC_RATE_INCREMENTAL
		}
		AngleRate = 200.f * RcRate * Command;

		if (Rates.SuperRate > 0.f)
		{
			const float SuperFactor = 1.f / FMath::Clamp(1.f - CommandAbs * Rates.SuperRate, 0.01f, 1.f);
			AngleRate *= SuperFactor;
		}
	}
	else
	{
		const float Expo = FMath::Clamp(Rates.ActualExpo, 0.f, 1.f);
		const float Command5 = Command * Command * Command * Command * Command;
		const float ExpoCurve = CommandAbs * (Command5 * Expo + Command * (1.f - Expo));
		const float StickMovement = FMath::Max(0.f, Rates.MaxRate - Rates.CenterSensitivity);
		AngleRate = Command * Rates.CenterSensitivity + StickMovement * ExpoCurve;
	}

	return FMath::Clamp(AngleRate, -MaxSetpointRate, MaxSetpointRate);
}

float UFPVFlightMath::ComputeMaxRateDegPerSec(const FFPVAxisRates& Rates, EFPVRatesType RatesType)
{
	return ComputeRateDegPerSec(1.f, Rates, RatesType);
}

float UFPVFlightMath::ApplyThrottleCurve(float Throttle, float Mid, float Expo)
{
	const float X = FMath::Clamp(Throttle, 0.f, 1.f);
	const float M = FMath::Clamp(Mid, 0.01f, 0.99f);
	const float E = FMath::Clamp(Expo, 0.f, 1.f);

	// Continuous form of Betaflight's lookupThrottleRC table.
	const float T = X - M;
	const float Span = T > 0.f ? 1.f - M : M;
	return FMath::Clamp(M + T * ((1.f - E) + E * (T * T) / (Span * Span)), 0.f, 1.f);
}
