#include "Core/FPVPlayerController.h"
#include "FPVGame.h"
#include "Core/FPVGameMode.h"
#include "Core/FPVGameState.h"
#include "Mission/FPVMissionManagerComponent.h"
#include "UI/FPVFlightHUDWidget.h"
#include "UI/FPVMissionResultWidget.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "TimerManager.h"

AFPVPlayerController::AFPVPlayerController()
{
	FlightHUDWidgetClass = UFPVFlightHUDWidget::StaticClass();
	MissionResultWidgetClass = UFPVMissionResultWidget::StaticClass();
}

void AFPVPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);

	if (FlightHUDWidgetClass)
	{
		FlightHUDWidget = CreateWidget<UFPVFlightHUDWidget>(this, FlightHUDWidgetClass);
		if (FlightHUDWidget)
		{
			FlightHUDWidget->AddToViewport(0);
		}
	}

	BindToMissionManager();
}

void AFPVPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(MissionResultTimer);
	Super::EndPlay(EndPlayReason);
}

void AFPVPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (GameMappingContext)
		{
			Subsystem->AddMappingContext(GameMappingContext, GameMappingPriority);
		}
		else
		{
			UE_LOG(LogFPVGame, Warning, TEXT("%s has no GameMappingContext assigned - restart key will not work. Assign IMC_Game in BP_FPVPlayerController."), *GetName());
		}
	}

	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (RestartMissionAction)
		{
			Input->BindAction(RestartMissionAction, ETriggerEvent::Started, this, &AFPVPlayerController::HandleRestartInput);
		}
	}
}

void AFPVPlayerController::HandleRestartInput(const FInputActionValue& Value)
{
	RestartMission();
}

void AFPVPlayerController::RestartMission()
{
	// Single player: the game mode exists locally. (Multiplayer would route this through a server RPC.)
	if (AFPVGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AFPVGameMode>() : nullptr)
	{
		GameMode->RestartMission();
	}
	else
	{
		UE_LOG(LogFPVGame, Warning, TEXT("RestartMission: the active game mode is not an AFPVGameMode."));
	}
}

void AFPVPlayerController::BindToMissionManager()
{
	const AFPVGameState* FPVGameState = GetWorld() ? GetWorld()->GetGameState<AFPVGameState>() : nullptr;
	UFPVMissionManagerComponent* Mission = FPVGameState ? FPVGameState->GetMissionManager() : nullptr;
	if (!Mission)
	{
		UE_LOG(LogFPVGame, Warning, TEXT("%s: no FPV mission manager found, mission UI disabled."), *GetName());
		return;
	}

	Mission->OnMissionStateChanged.AddUniqueDynamic(this, &AFPVPlayerController::HandleMissionStateChanged);
	if (Mission->GetMissionState() == EFPVMissionState::Completed || Mission->GetMissionState() == EFPVMissionState::Failed)
	{
		ShowMissionResultScreen();
	}
}

void AFPVPlayerController::HandleMissionStateChanged(EFPVMissionState NewState, EFPVMissionState OldState)
{
	GetWorldTimerManager().ClearTimer(MissionResultTimer);

	if (NewState == EFPVMissionState::Completed || NewState == EFPVMissionState::Failed)
	{
		const float Delay = NewState == EFPVMissionState::Completed ? MissionCompleteScreenDelay : MissionFailedScreenDelay;
		if (Delay > 0.f)
		{
			GetWorldTimerManager().SetTimer(MissionResultTimer, this, &AFPVPlayerController::ShowMissionResultScreen, Delay, false);
		}
		else
		{
			ShowMissionResultScreen();
		}
	}
	else
	{
		HideMissionResultScreen();
	}
}

void AFPVPlayerController::ShowMissionResultScreen()
{
	const AFPVGameState* FPVGameState = GetWorld() ? GetWorld()->GetGameState<AFPVGameState>() : nullptr;
	const UFPVMissionManagerComponent* Mission = FPVGameState ? FPVGameState->GetMissionManager() : nullptr;
	if (!IsLocalController() || !MissionResultWidgetClass || !Mission)
	{
		return;
	}

	if (!MissionResultWidget)
	{
		MissionResultWidget = CreateWidget<UFPVMissionResultWidget>(this, MissionResultWidgetClass);
	}
	if (!MissionResultWidget)
	{
		return;
	}

	MissionResultWidget->SetResult(Mission->GetMissionState(), Mission->GetResultReason());
	if (!MissionResultWidget->IsInViewport())
	{
		MissionResultWidget->AddToViewport(10);
	}

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	if (UWidget* FocusTarget = MissionResultWidget->GetRestartButton())
	{
		InputMode.SetWidgetToFocus(FocusTarget->TakeWidget());
	}
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
}

void AFPVPlayerController::HideMissionResultScreen()
{
	if (MissionResultWidget && MissionResultWidget->IsInViewport())
	{
		MissionResultWidget->RemoveFromParent();
	}

	if (IsLocalController())
	{
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}
}

bool AFPVPlayerController::IsMissionResultScreenVisible() const
{
	return MissionResultWidget && MissionResultWidget->IsInViewport();
}
