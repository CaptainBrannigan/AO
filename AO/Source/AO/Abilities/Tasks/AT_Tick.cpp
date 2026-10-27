///Copyright Jordan Cain. All Rights Reserved



#include "AT_Tick.h"

void UAT_Tick::Activate()
{
}

void UAT_Tick::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (customTickInterval == 0)
	{
		OnTick.ExecuteIfBound(DeltaTime);
	}
	else
	{
		if (tickTimer >= customTickInterval)
		{
			OnTick.ExecuteIfBound(DeltaTime);
			tickTimer = 0.0f;
		}
		else
		{
			tickTimer += DeltaTime;
		}
	}
}
