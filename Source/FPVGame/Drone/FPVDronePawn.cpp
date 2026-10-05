#include "Drone/FPVDronePawn.h"
#include "FPVGame.h"
#include "Audio/FPVImpactSoundComponent.h"
#include "Audio/FPVMotorSoundComponent.h"
#include "Drone/FPVCameraComponent.h"
#include "Drone/FPVDroneMovementComponent.h"
#include "Effects/FPVExplosionActor.h"
#include "Targets/FPVTargetActor.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AFPVDronePawn::AFPVDronePawn()
{
	PrimaryActorTick.bCanEverTick = false;

	// The flight model owns the rotation; never copy the controller's rotation onto the drone.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(15.f);
	CollisionSphere->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	CollisionSphere->SetCanEverAffectNavigation(false);
	CollisionSphere->SetShouldUpdatePhysicsVolume(true);
	RootComponent = CollisionSphere;

	// Placeholder 5-inch quad: flat frame + 4 prop discs. Hidden from the pilot's own camera.
	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(CollisionSphere);

	FrameMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrameMesh"));
	FrameMesh->SetupAttachment(VisualRoot);
	FrameMesh->SetStaticMesh(CubeMesh.Object);
	FrameMesh->SetRelativeScale3D(FVector(0.20f, 0.10f, 0.04f));
	FrameMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FrameMesh->SetOwnerNoSee(true);

	const FVector PropOffsets[] = { FVector(9.f, 9.f, 3.f), FVector(9.f, -9.f, 3.f), FVector(-9.f, 9.f, 3.f), FVector(-9.f, -9.f, 3.f) };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(PropOffsets); ++Index)
	{
		UStaticMeshComponent* Prop = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("PropMesh%d"), Index));
		Prop->SetupAttachment(VisualRoot);
		Prop->SetStaticMesh(CylinderMesh.Object);
		Prop->SetRelativeLocation(PropOffsets[Index]);
		Prop->SetRelativeScale3D(FVector(0.12f, 0.12f, 0.01f));
		Prop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Prop->SetOwnerNoSee(true);
		PropMeshes.Add(Prop);
	}

	CameraMount = CreateDefaultSubobject<USceneComponent>(TEXT("CameraMount"));
	CameraMount->SetupAttachment(CollisionSphere);
	CameraMount->SetRelativeLocation(FVector(10.f, 0.f, 3.f));

	FPVCamera = CreateDefaultSubobject<UFPVCameraComponent>(TEXT("FPVCamera"));
	FPVCamera->SetupAttachment(CameraMount);

	DroneMovement = CreateDefaultSubobject<UFPVDroneMovementComponent>(TEXT("DroneMovement"));
	DroneMovement->UpdatedComponent = CollisionSphere;

	MotorSound = CreateDefaultSubobject<UFPVMotorSoundComponent>(TEXT("MotorSound"));
	MotorSound->SetupAttachment(CollisionSphere);

	CrashSound = CreateDefaultSubobject<UFPVImpactSoundComponent>(TEXT("CrashSound"));
	CrashSound->SetupAttachment(CollisionSphere);
	CrashSound->SoundType = EFPVImpactSoundType::Crash;
	CrashSound->bAllowSpatialization = false;

	ExplosionClass = AFPVExplosionActor::StaticClass();
}

void AFPVDronePawn::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	DroneMovement->OnImpact.AddUniqueDynamic(this, &AFPVDronePawn::HandleMovementImpact);
}

UPawnMovementComponent* AFPVDronePawn::GetMovementComponent() const
{
	return DroneMovement;
}

void AFPVDronePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DroneMappingContext)
			{
				Subsystem->AddMappingContext(DroneMappingContext, DroneMappingPriority);
			}
			else
			{
				UE_LOG(LogFPVGame, Warning, TEXT("%s has no DroneMappingContext assigned - flight controls will not work. Assign IMC_Drone in BP_FPVDrone."), *GetName());
			}
		}
	}

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogFPVGame, Error, TEXT("%s expected an EnhancedInputComponent. Check Project Settings > Input > Default Input Component Class."), *GetName());
		return;
	}

	// Axis actions: "Triggered" fires every frame while the input is held, "Completed" when released (value 0).
	if (ThrottleAction)
	{
		Input->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &AFPVDronePawn::HandleThrottleInput);
	}
	if (ThrottleStickAction)
	{
		Input->BindAction(ThrottleStickAction, ETriggerEvent::Triggered, this, &AFPVDronePawn::HandleThrottleStickInput);
		Input->BindAction(ThrottleStickAction, ETriggerEvent::Completed, this, &AFPVDronePawn::HandleThrottleStickInput);
	}
	if (ThrottleTriggerAction)
	{
		Input->BindAction(ThrottleTriggerAction, ETriggerEvent::Triggered, this, &AFPVDronePawn::HandleThrottleTriggerInput);
		Input->BindAction(ThrottleTriggerAction, ETriggerEvent::Completed, this, &AFPVDronePawn::HandleThrottleTriggerInput);
	}
	if (PitchAction)
	{
		Input->BindAction(PitchAction, ETriggerEvent::Triggered, this, &AFPVDronePawn::HandlePitchInput);
		Input->BindAction(PitchAction, ETriggerEvent::Completed, this, &AFPVDronePawn::HandlePitchInput);
	}
	if (RollAction)
	{
		Input->BindAction(RollAction, ETriggerEvent::Triggered, this, &AFPVDronePawn::HandleRollInput);
		Input->BindAction(RollAction, ETriggerEvent::Completed, this, &AFPVDronePawn::HandleRollInput);
	}
	if (YawAction)
	{
		Input->BindAction(YawAction, ETriggerEvent::Triggered, this, &AFPVDronePawn::HandleYawInput);
		Input->BindAction(YawAction, ETriggerEvent::Completed, this, &AFPVDronePawn::HandleYawInput);
	}
	if (ToggleFlightModeAction)
	{
		Input->BindAction(ToggleFlightModeAction, ETriggerEvent::Started, this, &AFPVDronePawn::HandleToggleFlightMode);
	}
}

void AFPVDronePawn::UnPossessed()
{
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DroneMappingContext)
			{
				Subsystem->RemoveMappingContext(DroneMappingContext);
			}
		}
	}

	if (DroneMovement)
	{
		DroneMovement->SetControlInput(FFPVDroneControlInput());
	}

	Super::UnPossessed();
}

void AFPVDronePawn::HandleThrottleInput(const FInputActionValue& Value)
{
	if (!bDroneDestroyed)
	{
		DroneMovement->AddThrottle(Value.Get<float>() * ThrottleChangeRate * GetWorld()->GetDeltaSeconds());
	}
}

void AFPVDronePawn::HandleThrottleStickInput(const FInputActionValue& Value)
{
	if (GamepadThrottleMode == EFPVGamepadThrottleMode::Stick)
	{
		ApplyAbsoluteThrottle(0.5f * (FMath::Clamp(Value.Get<float>(), -1.f, 1.f) + 1.f));
	}
}

void AFPVDronePawn::HandleThrottleTriggerInput(const FInputActionValue& Value)
{
	if (GamepadThrottleMode == EFPVGamepadThrottleMode::Trigger)
	{
		ApplyAbsoluteThrottle(FMath::Clamp(Value.Get<float>(), 0.f, 1.f));
	}
}

void AFPVDronePawn::ApplyAbsoluteThrottle(float Throttle01)
{
	if (bDroneDestroyed)
	{
		return;
	}

	if (!bAbsoluteThrottleArmed)
	{
		if (bRequireThrottleLowToArm && Throttle01 > ThrottleArmThreshold)
		{
			bWaitingForThrottleArm = true;
			return;
		}
		bAbsoluteThrottleArmed = true;
		bWaitingForThrottleArm = false;
		UE_LOG(LogFPVGame, Log, TEXT("Throttle armed"));
	}

	DroneMovement->SetThrottle(Throttle01);
}

void AFPVDronePawn::HandlePitchInput(const FInputActionValue& Value)
{
	DroneMovement->SetPitchInput(bDroneDestroyed ? 0.f : Value.Get<float>());
}

void AFPVDronePawn::HandleRollInput(const FInputActionValue& Value)
{
	DroneMovement->SetRollInput(bDroneDestroyed ? 0.f : Value.Get<float>());
}

void AFPVDronePawn::HandleYawInput(const FInputActionValue& Value)
{
	DroneMovement->SetYawInput(bDroneDestroyed ? 0.f : Value.Get<float>());
}

void AFPVDronePawn::HandleToggleFlightMode(const FInputActionValue& Value)
{
	if (!bDroneDestroyed)
	{
		DroneMovement->ToggleFlightMode();
		UE_LOG(LogFPVGame, Log, TEXT("Flight mode: %s"), *UEnum::GetDisplayValueAsText(DroneMovement->GetFlightMode()).ToString());
	}
}

bool AFPVDronePawn::ShouldDetonateOnImpact(const FHitResult& Hit) const
{
	const AFPVTargetActor* Target = Cast<AFPVTargetActor>(Hit.GetActor());
	return Target && !Target->IsTargetDestroyed();
}

void AFPVDronePawn::HandleMovementImpact(const FHitResult& Hit, float ImpactSpeed)
{
	if (bDroneDestroyed)
	{
		return;
	}

	if (ShouldDetonateOnImpact(Hit))
	{
		Detonate(Hit);
	}
	else if (bCanCrash && ImpactSpeed >= CrashImpactSpeed)
	{
		UE_LOG(LogFPVGame, Log, TEXT("Hard impact on %s at %.0f km/h"), *GetNameSafe(Hit.GetActor()), ImpactSpeed * 0.036f);
		Crash(NSLOCTEXT("FPVGame", "CrashReasonImpact", "Drone destroyed in a crash"));
	}
}

void AFPVDronePawn::FellOutOfWorld(const UDamageType& DamageType)
{
	// Don't let the engine delete the pawn: treat it as a crash so the mission fails cleanly.
	Crash(NSLOCTEXT("FPVGame", "CrashReasonOutOfBounds", "Drone flew out of the area"));
}

void AFPVDronePawn::Crash(const FText& Reason)
{
	if (bDroneDestroyed)
	{
		return;
	}
	bDroneDestroyed = true;
	bCrashed = true;

	UE_LOG(LogFPVGame, Log, TEXT("Drone crashed: %s"), *Reason.ToString());

	DroneMovement->SetFlightEnabled(false);
	MotorSound->SetMotorsRunning(false);
	CrashSound->Play();

	OnDroneCrashed.Broadcast(this, Reason);
	OnDroneDestroyed.Broadcast(this);
}

void AFPVDronePawn::Detonate(const FHitResult& Hit)
{
	if (bDroneDestroyed)
	{
		return;
	}
	bDroneDestroyed = true;

	const FVector ApproachDirection = DroneMovement->Velocity.IsNearlyZero() ? GetActorForwardVector() : DroneMovement->Velocity.GetSafeNormal();
	const FVector BlastLocation = Hit.bBlockingHit ? FVector(Hit.ImpactPoint) : GetActorLocation();
	AActor* HitActor = Hit.GetActor();

	UE_LOG(LogFPVGame, Log, TEXT("Drone detonated on %s at %s"), *GetNameSafe(HitActor), *BlastLocation.ToCompactString());

	// Disable the drone: no more flight, no collision, invisible, silent.
	DroneMovement->SetFlightEnabled(false);
	MotorSound->SetMotorsRunning(false);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualRoot->SetVisibility(false, true);

	if (UWorld* World = GetWorld())
	{
		if (ExplosionClass)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnParams.Instigator = this;
			World->SpawnActor<AFPVExplosionActor>(ExplosionClass, BlastLocation, (-ApproachDirection).Rotation(), SpawnParams);
		}
	}

	if (HitActor)
	{
		UGameplayStatics::ApplyPointDamage(HitActor, PayloadDamage, ApproachDirection, Hit, GetController(), this, UDamageType::StaticClass());
	}

	// Pull the camera back so the pilot can watch the explosion instead of staring into it.
	FPVCamera->ShowImpactView(BlastLocation, ApproachDirection, HitActor);

	ReceiveDetonated(Hit);
	OnDroneDestroyed.Broadcast(this);
}
