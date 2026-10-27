// Copyright Jordan Cain. All Rights Reserved.
#pragma once

#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "Abilities/SequenceTaskInterface.h"
#include "Data/GlobalLog.h"
#include "Data/DataMapInterface.h"
#include "Purpose/Datachunks/FloatChunk.h"
#include "AT_Repeat.generated.h"


UCLASS(MinimalAPI)
class UActionPerformanceDesired : public UFloatChunk { GENERATED_BODY() };

UCLASS(MinimalAPI)
class UTimeBetweenActions : public UFloatChunk { GENERATED_BODY() };

UCLASS(BlueprintType, Blueprintable, EditInlineNew)
class UAT_Repeat : public UAbilityTask_Repeat, public ISequenceTaskInterface, public IDataMapInterface
{
	GENERATED_BODY()
public:

	UAT_Repeat(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
		AddData(CreateDefaultSubobject<UActionPerformanceDesired>("NumberOfActionsDesired"));
		AddData(CreateDefaultSubobject<UTimeBetweenActions>("TimeBetweenActions"));
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
		ActionPerformancesDesired = DataChunk<UActionPerformanceDesired>()->value;
		///TimeBetweenActions = entryData.periodOfInput;
		TimeBetweenActions = DataChunk<UTimeBetweenActions>()->value;
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
	const TArray<FDataMapEntry>& DataMap() final { return dataMap; }
	TArray<FDataMapEntry> DataMapCopy() final { return dataMap; }

	void AddData(UDataChunk* inData, bool overwriteValue = true) final { AddDataLocal(*inData); }
	void AppendData(const TArray<FDataMapEntry>& inDataMap, bool overwriteValue = true) final { AppendDataLocal(inDataMap); }
	void RemoveData(TSubclassOf<UDataChunk> inClass) final { RemoveDataLocal(inClass); }

	UPROPERTY(EditAnywhere)
		TArray<FDataMapEntry> dataMap;

	/// Force implementers to provide an editable datamap
	/// The only way to the change the DataMap is through the Server RPCs
	TArray<FDataMapEntry>& DataMapInternal() final { return dataMap; }
};
