#include "Drone/FPVCameraComponent.h"
#include "Engine/World.h"

UFPVCameraComponent::UFPVCameraComponent()
{
	// Wide FOV typical for FPV goggles feeds (real FPV lenses are ~120-150 deg diagonal with fisheye).
	FieldOfView = 120.f;
	bUsePawnControlRotation = false;
	bConstrainAspectRatio = false;
	bLockToHmd = false;

	// FPV video has a fast shutter: no smeared frames during fast flips.
	PostProcessSettings.bOverride_MotionBlurAmount = true;
	PostProcessSettings.MotionBlurAmount = 0.f;
}

void UFPVCameraComponent::OnRegister()
{
	Super::OnRegister();
	if (!bShowingImpactView)
	{
		SetRelativeRotation(FRotator(CameraUptiltDeg, 0.f, 0.f));
	}
}

#if WITH_EDITOR
void UFPVCameraComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	SetRelativeRotation(FRotator(CameraUptiltDeg, 0.f, 0.f));
}
#endif

void UFPVCameraComponent::SetCameraUptilt(float NewUptiltDeg)
{
	CameraUptiltDeg = FMath::Clamp(NewUptiltDeg, -10.f, 70.f);
	if (!bShowingImpactView)
	{
		SetRelativeRotation(FRotator(CameraUptiltDeg, 0.f, 0.f));
	}
}

void UFPVCameraComponent::ShowImpactView(const FVector& FocusPoint, const FVector& ApproachDirection, AActor* IgnoredActor)
{
	FVector Back = -ApproachDirection.GetSafeNormal2D();
	if (Back.IsNearlyZero())
	{
		Back = -GetForwardVector().GetSafeNormal2D();
	}
	if (Back.IsNearlyZero())
	{
		Back = -FVector::ForwardVector;
	}

	const FVector TraceStart = FocusPoint + FVector(0.f, 0.f, 50.f);
	FVector CameraLocation = FocusPoint + Back * ImpactViewDistance + FVector::UpVector * ImpactViewHeight;

	// Keep the observer camera out of walls/buildings.
	if (UWorld* World = GetWorld())
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(FPVImpactViewTrace), false, GetOwner());
		if (IgnoredActor)
		{
			Params.AddIgnoredActor(IgnoredActor);
		}

		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, TraceStart, CameraLocation, ECC_Camera, Params))
		{
			CameraLocation = Hit.Location + Hit.Normal * 30.f;
		}
	}

	DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	SetWorldLocationAndRotation(CameraLocation, (FocusPoint - CameraLocation).Rotation());
	bShowingImpactView = true;
}
