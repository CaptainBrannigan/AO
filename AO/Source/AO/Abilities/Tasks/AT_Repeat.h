// Copyright Jordan Cain. All Rights Reserved.
#pragma once

#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "Abilities/SequenceTaskInterface.h"
#include "Data/GlobalLog.h"
#include "Data/DataMapInterface.h"
#include "Purpose/DataChunks/FloatChunk.h"
#include "Data/DataTags.h"
#include "AT_Repeat.generated.h"


/// Task data (Data/DataTags.h), previously UActionPerformanceDesired and UTimeBetweenActions
/// Data.Task.Repeat.Count		FFloatChunk, number of actions
/// Data.Task.Repeat.Interval	FFloatChunk, seconds between actions

UCLASS(BlueprintType, Blueprintable, EditInlineNew)
class UAT_Repeat : public UAbilityTask_Repeat, public ISequenceTaskInterface, public IDataMapInterface
{
	GENERATED_BODY()
public:

	UAT_Repeat(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
		dataMap.Set(FFloatChunk(TAG_Data_Task_Repeat_Count));
		dataMap.Set(FFloatChunk(TAG_Data_Task_Repeat_Interval));
	}

	/** Start a task that repeats an action or set of actions. */
	//UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	//static UAbilityTask_Repeat* RepeatAction(UGameplayAbility* OwningAbility, float TimeBetweenActions, int32 TotalActionCount);

	FSequenceTaskFinished taskFinished;

	void EstablishTaskFinishedCallback(FScriptDelegate callback) override
	{
		taskFinished.Add(callback);
	}

	bool PreActivationInitialization(const FActionSequenceEntry& entryData, class UAI_SequentialActions& source) override 
	{
		///ActionPerformancesDesired = entryData.maxInputCount;
		ActionPerformancesDesired = DataChunk<FFloatChunk>(TAG_Data_Task_Repeat_Count) ? DataChunk<FFloatChunk>(TAG_Data_Task_Repeat_Count)->GetValue() : 0;
		///TimeBetweenActions = entryData.periodOfInput;
		TimeBetweenActions = DataChunk<FFloatChunk>(TAG_Data_Task_Repeat_Interval) ? DataChunk<FFloatChunk>(TAG_Data_Task_Repeat_Interval)->GetValue() : 0.0f;
		ActionCounter = 0;

		FScriptDelegate finishDel;
		finishDel.BindUFunction(this, "EndInputTask");
		OnFinished.Add(finishDel);

		return OnFinished.IsBound();
	}

	bool PostActivationAdjustment(const FActionSequenceEntry& entryData, class UAI_SequentialActions& source) override
	{
		return true;
	}

	UFUNCTION()/// Necessary for FScriptDelegate
	void EndInputTask(int32 actionNumber)
	{
		EndTask();
		taskFinished.Broadcast();
	}

	/// Used so that we may dictate lifetimes of tasks that may not have an explicit end, such as a face direction task, if we want it to last as long as the current action is active
	ETaskDurationType GetTaskDurationType() override { return taskDuration; }

	UPROPERTY(EditAnywhere)
		ETaskDurationType taskDuration = ETaskDurationType::EndOnTaskFinished;

	FGameplayTagContainer  GetContextualTagsOfTask() final { return contextualTagForTask; }
	UPROPERTY(EditAnywhere)
		FGameplayTagContainer contextualTagForTask;

	///Allows evaluations to retrieve chunks of data from any type of object implementing interface
	const FDataMap& DataMap() const final { return dataMap; }


	UPROPERTY(EditAnywhere)
		FDataMap dataMap;

	/// Force implementers to provide an editable datamap
	FDataMap& MutableDataMap() final { return dataMap; }
};
