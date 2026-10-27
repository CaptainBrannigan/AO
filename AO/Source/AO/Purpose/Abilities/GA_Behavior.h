// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GA_PurposeBase.h"
#include "GA_Behavior.generated.h"

UCLASS()
/// Behavior abilitys are abilitys which establish the output to a GA_Action
class AO_API UGA_Behavior : public UGA_PurposeBase
{
	GENERATED_BODY()
public:

	UGA_Behavior(const FObjectInitializer& ObjectInitializer);

	/// Actually activate ability, do not call this directly 
	///virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	bool IsActorFacingTarget( AActor& ref, const FVector targetLocation );

	void AbilityFinished(const EAbilityPurposeFeedback reasonAbilityEnded) override;

	bool bTickingAbility = false;
};
