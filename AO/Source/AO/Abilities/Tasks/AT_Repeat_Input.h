 // Copyright Jordan Cain. All Rights Reserved.
#pragma once

#include "AT_Repeat.h"
#include "AT_Repeat_Input.generated.h"

UCLASS(BlueprintType, Blueprintable, EditInlineNew)
class UAT_Repeat_Input : public UAT_Repeat
{
	GENERATED_BODY()
public:

	UAT_Repeat_Input(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
		//AddData(CreateDefaultSubobject<UActionPerformanceDesired>("NumberOfActionsDesired"));
		//AddData(CreateDefaultSubobject<UTimeBetweenActions>("TimeBetweenActions"));
	}

	/** Start a task that repeats an action or set of actions. */
	//UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	//static UAbilityTask_Repeat* RepeatAction(UGameplayAbility* OwningAbility, float TimeBetweenActions, int32 TotalActionCount);

	bool PreActivationInitialization(const FActionSequenceEntry& entryData, class UAI_SequentialActions& source) override
	{
		bool bCanPerform = Super::PreActivationInitialization(entryData, source);

		if (!bCanPerform) { return bCanPerform; }

		owningAbility = &source;

		FScriptDelegate inputDel;
		inputDel.BindUFunction(this, "PerformInput");

		OnPerformAction.Add(inputDel);
		
		bCanPerform = OnPerformAction.IsBound();

		return bCanPerform;
	}
	bool PostActivationAdjustment(const FActionSequenceEntry& entryData, class UAI_SequentialActions& source) override
	{
		bool bCanPerform = Super::PostActivationAdjustment(entryData, source);
		return bCanPerform;
	}

	UPROPERTY(EditAnywhere, DisplayName = "Input To Perform")
		/// Input tag that a player would utilize to perform an ability
		FGameplayTag inputToPerform;

protected:

	UFUNCTION()/// Required for FScriptDelegate
	void PerformInput();

private:

	TWeakObjectPtr<class UAI_SequentialActions> owningAbility = nullptr;
};
