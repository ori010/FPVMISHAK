#include "Core/FPVGameMode.h"
#include "FPVGame.h"
#include "Core/FPVGameState.h"
#include "Core/FPVPlayerController.h"
#include "Drone/FPVDronePawn.h"
#include "Effects/FPVExplosionActor.h"
#include "Mission/FPVMissionManagerComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

AFPVGameMode::AFPVGameMode()
{
	DefaultPawnClass = AFPVDronePawn::StaticClass();
	PlayerControllerClass = AFPVPlayerController::StaticClass();
	GameStateClass = AFPVGameState::StaticClass();
}

void AFPVGameMode::StartPlay()
{
	Super::StartPlay();

	if (bAutoStartMission)
	{
		if (UFPVMissionManagerComponent* Mission = GetMissionManager())
		{
			Mission->StartMission();
		}
		else
		{
			UE_LOG(LogFPVGame, Error, TEXT("No mission manager found. Is the GameState class set to AFPVGameState (or a child)?"));
		}
	}
}

void AFPVGameMode::SetPlayerDefaults(APawn* PlayerPawn)
{
	Super::SetPlayerDefaults(PlayerPawn);

	if (AFPVDronePawn* Drone = Cast<AFPVDronePawn>(PlayerPawn))
	{
		Drone->OnDroneCrashed.AddUniqueDynamic(this, &AFPVGameMode::HandleDroneCrashed);
	}
}

void AFPVGameMode::HandleDroneCrashed(AFPVDronePawn* Drone, const FText& Reason)
{
	if (UFPVMissionManagerComponent* Mission = GetMissionManager())
	{
		Mission->FailMission(Reason);
	}
}

UFPVMissionManagerComponent* AFPVGameMode::GetMissionManager() const
{
	const AFPVGameState* FPVGameState = GetGameState<AFPVGameState>();
	return FPVGameState ? FPVGameState->GetMissionManager() : nullptr;
}

void AFPVGameMode::RestartMission()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UE_LOG(LogFPVGame, Log, TEXT("Restarting mission"));

	UFPVMissionManagerComponent* Mission = GetMissionManager();
	if (Mission)
	{
		Mission->ResetMission();
	}

	// Remove leftovers of the previous attempt.
	for (TActorIterator<AFPVExplosionActor> It(World); It; ++It)
	{
		It->Destroy();
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}

		if (APawn* OldPawn = PC->GetPawn())
		{
			PC->UnPossess();
			OldPawn->Destroy();
		}
		RestartPlayer(PC);
	}

	if (Mission)
	{
		Mission->StartMission();
	}
}
