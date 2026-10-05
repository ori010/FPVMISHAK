#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "FPVImpactSoundComponent.generated.h"

UENUM(BlueprintType)
enum class EFPVImpactSoundType : uint8
{
	/** Big blast: crack, roaring body, low thump and rumble (~3 s). */
	Explosion,
	/** Drone crash: plastic/carbon crunch and motors winding down (~1 s). */
	Crash
};

/**
 * Procedural one-shot impact sound (explosion or crash), synthesized in code so no audio files are needed.
 * Call Play() to trigger it; the component stops by itself when the sound has finished.
 */
UCLASS(ClassGroup = (FPV), meta = (BlueprintSpawnableComponent))
class FPVGAME_API UFPVImpactSoundComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	UFPVImpactSoundComponent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Impact Sound")
	EFPVImpactSoundType SoundType = EFPVImpactSoundType::Explosion;

	/** Loudness (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Impact Sound", meta = (ClampMin = "0", ClampMax = "1"))
	float Loudness = 1.f;

	/** Distance (cm) over which the sound fades out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Impact Sound", meta = (ClampMin = "100"))
	float FalloffDistance = 30000.f;

	UFUNCTION(BlueprintCallable, Category = "FPV Impact Sound")
	void Play();

protected:
	virtual ISoundGeneratorPtr CreateSoundGenerator(const FSoundGeneratorInitParams& InParams) override;
};
