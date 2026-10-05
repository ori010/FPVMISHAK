#include "Core/FPVGameState.h"
#include "Mission/FPVMissionManagerComponent.h"

AFPVGameState::AFPVGameState()
{
	MissionManager = CreateDefaultSubobject<UFPVMissionManagerComponent>(TEXT("MissionManager"));
}
