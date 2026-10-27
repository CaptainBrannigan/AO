// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GA_Behavior.h"
#include "Purpose/PurposeAbilityComponent.h"
#include "AIController.h"
#include "Gameframework/Character.h"
#include "Behavior_AI.generated.h"

/**
 *
 */
UCLASS(Abstract, EditInlineNew)
/// <summary>
/// The AI version of Ability_Behavior is where logical behaviors for AI performing actions is established
/// It controls the inputs of the tasks autonomously in as a substitute for player input (who uses a Player version of Ability_Behavior)
/// </summary>
class AO_API UBehavior_AI : public UGA_Behavior
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = "Behavior Defaults")
	/// This ability priority only affects choosing between a new and currently executing ability by UPurposeAbilityComponent
	/// Design: Abilities UPurposeAbilityComponent Purpose; AbilityPriority could potentially be accounted for during scoring
	EAbilityPriority AbilityPriority = EAbilityPriority::Lowest;

public:

	/*UBehavior_AI(const FObjectInitializer& ObjectInitializer)
		:Super(ObjectInitializer)
	{
		bIsPausable = false;
	}*/

	/** Called to trigger the actual ability once the delegates have been set up
	 *	Note that the default implementation does nothing and you don't have to call it */
	///void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) final;

	///void TickAbility(float DeltaTime) final;

	virtual TObjectPtr<UGA_PurposeBase> Initialize() override;

	TObjectPtr<UPurposeAbilityComponent> Owner() { return Cast<UPurposeAbilityComponent>(GetAbilitySystemComponentFromActorInfo()); }/// Cast is safe as Vessel is default Pawn

	TObjectPtr<AAIController> Controller()
	{
		if (IsValid(OwnerController)) { return OwnerController; }
		else
		{
			return OwnerController = Cast<AAIController>(GetControllerFromActorInfo());
		}
	}

protected:

	UPROPERTY()
	TObjectPtr<AAIController> OwnerController = nullptr;

	/*void OnGameplayAbilityDeactivated(UGameplayAbility& Ability) override
	{
		Super::OnGameplayAbilityDeactivated(Ability);

		Global::Log(DATADEBUG, BEHAVIOR, *this, "OnGameplayAbilityDeactivated", TEXT(""));
	}*/
};
