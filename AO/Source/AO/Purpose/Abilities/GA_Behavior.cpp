// Copyright Jordan Cain. All Rights Reserved.


#include "GA_Behavior.h"
#include "Data/DataMapInterface.h"
#include "Purpose/PurposeAbilityComponent.h"



UGA_Behavior::UGA_Behavior(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	//bIsPausable = false;
}


bool UGA_Behavior::IsActorFacingTarget( AActor& ref, const FVector targetLocation )
{
	///If dotProduct is 0, target is facing perpendicular to self 
	///if dotProduct is greater than 0, target is facing self
	return FVector::DotProduct( ref.GetActorForwardVector(), (targetLocation - ref.GetActorLocation()).GetSafeNormal() ) >= 0;
}

void UGA_Behavior::AbilityFinished(const EAbilityPurposeFeedback reasonAbilityEnded)
{
	if (FeedbackState() != EAbilityPurposeFeedback::None)
	{
		return;/// If abilityFeedbackState has already been set, AbilityFinished has already been called on this Ability and is not needed a second time
	}

	Super::AbilityFinished(reasonAbilityEnded);
	Global::Log(DATAESSENTIAL, TASK, AbilityNameForLog(), "AbilityFinished", TEXT("Feedback: %s."), *Global::EnumValueOnly<EAbilityPurposeFeedback>(FeedbackState()));

	bTickingAbility = false;

	Vessel()->AbilityHasFinished(reasonAbilityEnded);
	
	if (reasonAbilityEnded != EAbilityPurposeFeedback::FinishedUninterrupted)
	{
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
	}
	else
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

