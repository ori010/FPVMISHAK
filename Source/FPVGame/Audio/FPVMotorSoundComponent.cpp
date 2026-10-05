#include "Audio/FPVMotorSoundComponent.h"
#include "Drone/FPVDroneMovementComponent.h"
#include "GameFramework/Actor.h"
#include "Sound/SoundGenerator.h"
#include <atomic>

/** Runs on the audio render thread. Parameters arrive through atomics written by the game thread. */
class FFPVMotorSoundGenerator : public ISoundGenerator
{
public:
	explicit FFPVMotorSoundGenerator(float InSampleRate)
		: SampleRate(FMath::Max(InSampleRate, 8000.f))
	{
		for (int32 Motor = 0; Motor < 4; ++Motor)
		{
			TargetRPM[Motor].store(0.f);
			// Tiny per-motor detune gives the characteristic beating of 4 motors.
			Detune[Motor] = 1.f + 0.006f * (Motor - 1.5f);
		}
		TargetLoad.store(0.f);
		TargetAirSpeed.store(0.f);
		TargetVolume.store(0.f);
	}

	void SetParameters(const float InRPM[4], float InLoad, float InAirSpeed, float InVolume)
	{
		for (int32 Motor = 0; Motor < 4; ++Motor)
		{
			TargetRPM[Motor].store(InRPM[Motor]);
		}
		TargetLoad.store(InLoad);
		TargetAirSpeed.store(InAirSpeed);
		TargetVolume.store(InVolume);
	}

	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override
	{
		float RPMTarget[4];
		for (int32 Motor = 0; Motor < 4; ++Motor)
		{
			RPMTarget[Motor] = TargetRPM[Motor].load();
		}
		const float LoadTarget = TargetLoad.load();
		const float AirTarget = TargetAirSpeed.load();
		const float VolumeTarget = TargetVolume.load();

		const float Smooth = 1.f - FMath::Exp(-1.f / (0.02f * SampleRate)); // ~20 ms parameter smoothing
		const float TwoPi = 2.f * PI;

		for (int32 Sample = 0; Sample < NumSamples; ++Sample)
		{
			Volume += (VolumeTarget - Volume) * Smooth;
			Load += (LoadTarget - Load) * Smooth;
			AirSpeed += (AirTarget - AirSpeed) * Smooth;

			float Mix = 0.f;
			for (int32 Motor = 0; Motor < 4; ++Motor)
			{
				RPM[Motor] += (RPMTarget[Motor] - RPM[Motor]) * Smooth;
				const float RevPerSec = RPM[Motor] / 60.f * Detune[Motor];

				// Blade-pass buzz (3 blades): a few harmonics of a saw-like wave.
				BladePhase[Motor] += RevPerSec * 3.f / SampleRate;
				BladePhase[Motor] -= FMath::FloorToFloat(BladePhase[Motor]);
				const float P = BladePhase[Motor] * TwoPi;
				const float Buzz = FMath::Sin(P) + 0.5f * FMath::Sin(2.f * P) + 0.28f * FMath::Sin(3.f * P) + 0.15f * FMath::Sin(4.f * P);

				// Electric whine (7 pole pairs).
				WhinePhase[Motor] += RevPerSec * 7.f / SampleRate;
				WhinePhase[Motor] -= FMath::FloorToFloat(WhinePhase[Motor]);
				const float Whine = FMath::Sin(WhinePhase[Motor] * TwoPi);

				const float Level = 0.25f + 0.75f * FMath::Clamp(RPM[Motor] / 30000.f, 0.f, 1.f);
				Mix += Level * (0.55f * Buzz + 0.12f * Whine);
			}
			Mix *= 0.25f;

			// Prop wash noise (grows with load) and wind noise (grows with airspeed).
			const float White = NextNoise();
			PropNoise += (White - PropNoise) * 0.35f;
			WindNoise += (White - WindNoise) * 0.05f;
			const float WindLevel = FMath::Clamp(AirSpeed / 40.f, 0.f, 1.f);
			Mix += PropNoise * (0.05f + 0.25f * Load) + WindNoise * 1.4f * WindLevel * WindLevel;

			OutAudio[Sample] = FMath::Clamp(Volume * FMath::Tanh(Mix * 1.4f), -1.f, 1.f);
		}
		return NumSamples;
	}

private:
	float NextNoise()
	{
		NoiseState ^= NoiseState << 13;
		NoiseState ^= NoiseState >> 17;
		NoiseState ^= NoiseState << 5;
		return (NoiseState & 0xFFFFFF) / float(0x7FFFFF) - 1.f;
	}

	const float SampleRate;
	std::atomic<float> TargetRPM[4];
	std::atomic<float> TargetLoad;
	std::atomic<float> TargetAirSpeed;
	std::atomic<float> TargetVolume;

	float RPM[4] = { 0.f, 0.f, 0.f, 0.f };
	float BladePhase[4] = { 0.f, 0.25f, 0.5f, 0.75f };
	float WhinePhase[4] = { 0.f, 0.f, 0.f, 0.f };
	float Detune[4];
	float Load = 0.f;
	float AirSpeed = 0.f;
	float Volume = 0.f;
	float PropNoise = 0.f;
	float WindNoise = 0.f;
	uint32 NoiseState = 0x12345678u;
};

UFPVMotorSoundComponent::UFPVMotorSoundComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 1;
	bAutoActivate = true;
	bAllowSpatialization = false; // the pilot "hears" the drone like in every FPV sim
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UFPVMotorSoundComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(true);
}

ISoundGeneratorPtr UFPVMotorSoundComponent::CreateSoundGenerator(const FSoundGeneratorInitParams& InParams)
{
	Generator = MakeShared<FFPVMotorSoundGenerator, ESPMode::ThreadSafe>(InParams.SampleRate);
	return Generator;
}

void UFPVMotorSoundComponent::SetMotorsRunning(bool bRunning)
{
	bMotorsRunning = bRunning;
}

void UFPVMotorSoundComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Generator.IsValid())
	{
		return;
	}

	const UFPVDroneMovementComponent* Movement = GetOwner() ? GetOwner()->FindComponentByClass<UFPVDroneMovementComponent>() : nullptr;
	const bool bRunning = bMotorsRunning && Movement && Movement->IsFlightEnabled();

	float MotorRPM[4] = { 0.f, 0.f, 0.f, 0.f };
	float Load = 0.f;
	float AirSpeed = 0.f;
	if (bRunning)
	{
		for (int32 Motor = 0; Motor < 4; ++Motor)
		{
			// Thrust ~ RPM^2, so RPM grows roughly with the square root of the thrust share.
			const float Output = Movement->GetMotorOutput(Motor);
			MotorRPM[Motor] = FMath::Lerp(IdleRPM, MaxRPM, FMath::Sqrt(FMath::Clamp(Output, 0.f, 1.f)));
		}
		Load = Movement->GetAverageMotorOutput();
		AirSpeed = Movement->Velocity.Size() * 0.01f;
	}

	Generator->SetParameters(MotorRPM, Load, AirSpeed, bRunning ? Loudness : 0.f);
}
