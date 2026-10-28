// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Purpose/Abilities/Behavior_AI.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "GameplayTagContainer.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Abilities/SequenceTaskInterface.h"
#include "DataChunks/ContextualTags.h"
#include "AI_SequentialActions.generated.h"

UCLASS(MinimalAPI)
/// This AI behavior 
class UAI_SequentialActions : public UBehavior_AI
{
	GENERATED_BODY()
public:

	/*UAI_EngageTarget(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
		ResourceOverlapPolicy = EAbilityResourceOverlapPolicy::StartOnTop;
		AddRequiredResource(UAIResource_Movement::StaticClass());
		AddClaimedResource(UAIResource_Movement::StaticClass());
	}*/
	FString AbilityNameForLog() final { return descriptionOfBehavior.ToString() + "(" + GetOwningActorFromActorInfo()->GetName() + ")"; }

	/** Called to trigger the actual ability once the delegates have been set up
	 *	Note that the default implementation does nothing and you don't have to call it */
	void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) final;

	void AttemptSequenceOfActions();

	void AttemptNextSequenceOfActions()
	{
		/// Any tasks specified to last for the duration of an action will not end themselves
		/// We have to explictly end them prior to next action
		for (int i = 0; i < taskDuplicatesForCurrentAction.Num(); ++i)
		{
			taskDuplicatesForCurrentAction[i]->EndTask();/// EndTask does not explictly call back to This::TaskFinished(), it has to be manually called via delegate in the task itself
			/// So this should not result in recursive calls to AttemptNextSequenceOfActions
		}

		taskDuplicatesForCurrentAction.Empty();/// Ensure the previous task pointers are all removed
		++actionIndex;
		AttemptSequenceOfActions();
	}

	void PerformTask();

	UFUNCTION()/// Has to be UFUNCTION() otherwise FScriptDelegate cannot bind to it
		void TaskFinished();

	/// Provides Tasks with ability to create GameplayEvents
	void GameplayEvent(FGameplayTag eventTag, FGameplayEventData eventData)
	{
		SendGameplayEvent(eventTag, eventData);
	}

	UPROPERTY(EditAnywhere, Category = "Behavior Data")
	FText descriptionOfBehavior;

	UPROPERTY(EditAnywhere, Category = "Behavior Data", DisplayName = "Sequential Actions", meta = (TitleProperty=description))
		TArray<FActionSequenceEntry> sequenceOfActions;

	FDataMap& ContextDataForBehavior() { return Vessel()->CurrentBehavior().contextData; }

protected:

	int actionIndex = 0;
	int actionAttemptsOnFail = 0;
	int maxFailAttempts = 2;

	UPROPERTY()
		///TArray<FActionEntry> taskDuplicatesForCurrentAction;
		TArray<TObjectPtr<UGameplayTask>> taskDuplicatesForCurrentAction;

	void AttemptToAppendTaskDataToContextData(const FDataMap& dataToAppend);
};