// Copyright Jordan Cain. All Rights Reserved.


#include "ActorReference.h"
#include "Purpose/PurposeAbilityComponent.h"

UDataChunk& UActorReference::NonOverloadInitialize()
{
	if (GetOuter() && GetOuter()->IsA<UPurposeAbilityComponent>())
	{
		Global::Log(DATATRIVIAL, LogData, "UActorReference", "NonOverloadInitialize", TEXT("Was able to initialize actor with avatar actor of outer."));
		actor = Cast<UPurposeAbilityComponent>(GetOuter())->GetAvatarActor();
	}
	return *this;
}
