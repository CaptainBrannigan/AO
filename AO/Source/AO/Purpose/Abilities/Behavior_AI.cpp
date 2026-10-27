// Copyright Jordan Cain. All Rights Reserved.


#include "Behavior_AI.h"
#include "Tasks/AITask.h"

TObjectPtr<UGA_PurposeBase> UBehavior_AI::Initialize()
{
	Super::Initialize();

	OwnerController = Cast<AAIController>(GetControllerFromActorInfo());

	return this;
}



