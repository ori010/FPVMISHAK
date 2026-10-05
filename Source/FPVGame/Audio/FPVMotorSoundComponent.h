#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "FPVMotorSoundComponent.generated.h"

class FFPVMotorSoundGenerator;

/**
 * Procedural FPV motor sound (no audio files needed). Each of the 4 motors produces the typical
 * "blade pass" buzz (RPM x 3 blades) plus the electric motor whine (RPM x 7 pole pairs), and prop and
 * wind noise grow with load and airspeed. RPM follows the flight model's motor outputs every frame.
 *
 * Attach to a pawn that has a UFPVDroneMovementComponent; it reads the motors automatically.
 */
UCLASS(ClassGroup = (FPV), meta = (BlueprintSpawnableComponent))
class FPVGAME_API UFPVMotorSoundComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	UFPVMotorSoundComponent(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Motor RPM at 0 output (idle). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Motor Sound", meta = (ClampMin = "0"))
	float IdleRPM = 4000.f;

	/** Motor RPM at full output (2207 1750KV motor on 6S with 5" props: ~30000). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Motor Sound", meta = (ClampMin = "1000"))
	float MaxRPM = 30000.f;

	/** Overall loudness (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPV Motor Sound", meta = (ClampMin = "0", ClampMax = "1"))
	float Loudness = 0.35f;

	/** Silences the motors (e.g. after a crash). */
	UFUNCTION(BlueprintCallable, Category = "FPV Motor Sound")
	void SetMotorsRunning(bool bRunning);

protected:
	virtual ISoundGeneratorPtr CreateSoundGenerator(const FSoundGeneratorInitParams& InParams) override;

	TSharedPtr<FFPVMotorSoundGenerator, ESPMode::ThreadSafe> Generator;
	bool bMotorsRunning = true;
};
