// Copyright Jordan Cain. All Rights Reserved.


#include "AT_Repeat_Input.h"
#include "Abilities/AI/AI_SequentialActions.h"

void UAT_Repeat_Input::PerformInput()
{
	Global::Log(FULLTRACE, BEHAVIOR, "UAT_Repeat_Input", "PerformInput", TEXT(""));
	if (!IsActive())/// As we are utilizing gameplay tasks and delegates, we need to account for callbacks when ability is ending
	{
		return;/// We don't EndTask or broadcast as that should have already been done
	}

	if (!owningAbility.IsValid())
	{
		EndTask();
		taskFinished.Broadcast();
	}

	Global::Log(CALLTRACETRIVIAL, BEHAVIOR, "UAT_Repeat_Input", "PerformInput", TEXT("Performing input %s for %s.")
		, *inputToPerform.ToString()
		, GetOwnerActor() ? *GetOwnerActor()->GetName() : TEXT("Invalid")
	);

	owningAbility->GameplayEvent(inputToPerform, FGameplayEventData());
}
