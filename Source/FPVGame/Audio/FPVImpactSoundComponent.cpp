#include "Audio/FPVImpactSoundComponent.h"
#include "Sound/SoundGenerator.h"

namespace FPVImpactAudio
{
	/** One-shot synthesizer for explosion / crash sounds. Runs on the audio render thread. */
	class FGenerator : public ISoundGenerator
	{
	public:
		FGenerator(float InSampleRate, EFPVImpactSoundType InType, float InLoudness)
			: SampleRate(FMath::Max(InSampleRate, 8000.f))
			, Type(InType)
			, Loudness(InLoudness)
			, DurationSamples(int32(SampleRate * (InType == EFPVImpactSoundType::Explosion ? 3.5f : 1.1f)))
		{
		}

		virtual bool IsFinished() const override { return SampleIndex >= DurationSamples; }

		virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override
		{
			for (int32 Sample = 0; Sample < NumSamples; ++Sample, ++SampleIndex)
			{
				const float T = SampleIndex / SampleRate;
				const float Value = SampleIndex < DurationSamples
					? (Type == EFPVImpactSoundType::Explosion ? Explosion(T) : Crash(T))
					: 0.f;
				OutAudio[Sample] = FMath::Clamp(Loudness * FMath::Tanh(Value), -1.f, 1.f);
			}
			return NumSamples;
		}

	private:
		float Noise()
		{
			State ^= State << 13;
			State ^= State >> 17;
			State ^= State << 5;
			return (State & 0xFFFFFF) / float(0x7FFFFF) - 1.f;
		}

		/** One-pole low-pass with a cutoff in Hz. */
		static float LowPass(float& Memory, float Input, float CutoffHz, float Rate)
		{
			const float Alpha = 1.f - FMath::Exp(-2.f * PI * CutoffHz / Rate);
			Memory += (Input - Memory) * Alpha;
			return Memory;
		}

		float Explosion(float T)
		{
			const float White = Noise();

			// Initial crack.
			const float Crack = T < 0.04f ? White * (1.f - T / 0.04f) * 1.6f : 0.f;

			// Roaring body: noise whose brightness falls quickly.
			const float Cutoff = 180.f + 5000.f * FMath::Exp(-T * 7.f);
			const float Body = LowPass(BodyFilter, White, Cutoff, SampleRate) * 2.2f
				* FMath::Exp(-T * 1.6f) * (1.f - FMath::Exp(-T * 300.f));

			// Low thump: falling sine.
			const float ThumpFreq = 30.f + 35.f * FMath::Exp(-T * 6.f);
			ThumpPhase += ThumpFreq / SampleRate;
			const float Thump = FMath::Sin(2.f * PI * ThumpPhase) * 1.3f * FMath::Exp(-T * 4.f);

			// Long rumble: brown-ish noise.
			const float Rumble = LowPass(RumbleFilter, White, 90.f, SampleRate) * 3.f * FMath::Exp(-T * 0.9f) * FMath::Min(1.f, T * 8.f);

			return Crack + Body + Thump + Rumble;
		}

		float Crash(float T)
		{
			const float White = Noise();

			// Carbon/plastic crunch: a few noisy clicks in the first 150 ms.
			float Crunch = 0.f;
			const float ClickTimes[] = { 0.f, 0.035f, 0.07f, 0.12f };
			for (const float ClickTime : ClickTimes)
			{
				const float Local = T - ClickTime;
				if (Local >= 0.f && Local < 0.05f)
				{
					Crunch += White * FMath::Exp(-Local * 90.f);
				}
			}
			const float Bright = White - LowPass(CrunchFilter, White, 1500.f, SampleRate);

			// Short thud.
			ThumpPhase += 85.f / SampleRate;
			const float Thud = FMath::Sin(2.f * PI * ThumpPhase) * 0.8f * FMath::Exp(-T * 25.f);

			// Motors winding down: falling whine.
			const float WhineFreq = 180.f + 1300.f * FMath::Exp(-T * 3.5f);
			WhinePhase += WhineFreq / SampleRate;
			const float Whine = FMath::Sin(2.f * PI * WhinePhase) * 0.25f * FMath::Exp(-T * 2.5f);

			return Crunch * 1.2f + Bright * 0.6f * FMath::Exp(-T * 30.f) + Thud + Whine;
		}

		const float SampleRate;
		const EFPVImpactSoundType Type;
		const float Loudness;
		const int32 DurationSamples;
		int32 SampleIndex = 0;
		uint32 State = 0x9E3779B9u;
		float BodyFilter = 0.f;
		float RumbleFilter = 0.f;
		float CrunchFilter = 0.f;
		float ThumpPhase = 0.f;
		float WhinePhase = 0.f;
	};
}

UFPVImpactSoundComponent::UFPVImpactSoundComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 1;
	bAutoActivate = false;
	bAllowSpatialization = true;
	bOverrideAttenuation = true;
	AttenuationOverrides.bAttenuate = true;
	AttenuationOverrides.bSpatialize = true;
	AttenuationOverrides.FalloffDistance = FalloffDistance;
}

void UFPVImpactSoundComponent::Play()
{
	AttenuationOverrides.FalloffDistance = FalloffDistance;
	Stop();
	Start();
}

ISoundGeneratorPtr UFPVImpactSoundComponent::CreateSoundGenerator(const FSoundGeneratorInitParams& InParams)
{
	return MakeShared<FPVImpactAudio::FGenerator, ESPMode::ThreadSafe>(InParams.SampleRate, SoundType, Loudness);
}
