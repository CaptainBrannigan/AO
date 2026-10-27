// Copyright Jordan Cain. All Rights Reserved.


#include "AT_Idle.h"
#include "EngineUtils.h"

void UAT_Idle::Activate()
{
	Super::Activate();

	/// With the background threads for the Purpose System running, we need AI and their AManager's
	FTimerDelegate idleTimer;
	FTimerHandle idleTimerHandle;
	idleTimer.BindUObject(this, &UAT_Idle::FinishIdle);

	GetOwnerActor()->GetWorldTimerManager().SetTimer(idleTimerHandle, idleTimer, idleTime, false);
}

void UAT_Idle::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);
}
