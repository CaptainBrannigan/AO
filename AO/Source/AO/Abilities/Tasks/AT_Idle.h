// Copyright Jordan Cain, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Abilities/SequenceTaskInterface.h"
#include "Data/DataMapInterface.h"
#include "GameplayTaskResource.h"
#include "Data/GlobalLog.h"
#include "AT_Idle.generated.h"

UCLASS()
class UTask_Resource_Logic : public UGameplayTaskResource { GENERATED_BODY() };

UCLASS(BlueprintType, Blueprintable, EditInlineNew)
/// <summary>
/// 
/// </summary>
class UAT_Idle : public UAbilityTask, public IDataMapInterface, public ISequenceTaskInterface
{
	GENERATED_BODY()
public:

	UAT_Idle(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
		//AddData(CreateDefaultSubobject<ULocationToFace>("LocationToFace"));
		//bTickingTask = true;
	}

	void EstablishTaskFinishedCallback(FScriptDelegate callback) override
	{
		taskFinished.Add(callback);
	}

	bool PreActivationInitialization(const FActionSequenceEntry& entryData, class UAI_SequentialActions& source) override
	{
		/*if (!HasData(ULocationToFace::StaticClass()))
		{
			return false;
		}*/

		return true;
	}

	bool PostActivationAdjustment(const FActionSequenceEntry& entryData, class UAI_SequentialActions& source) override
	{
		return true;
	}

	/// Used so that we may dictate lifetimes of tasks that may not have an explicit end, such as a face direction task, if we want it to last as long as the current action is active
	ETaskDurationType GetTaskDurationType() override { return taskDuration; }

	UPROPERTY(EditAnywhere)
		ETaskDurationType taskDuration = ETaskDurationType::EndOnTaskFinished;

	UPROPERTY(EditAnywhere)
		float idleTime = 1.0f;

	FGameplayTagContainer  GetContextualTagsOfTask() final { return contextualTagForTask; }
	UPROPERTY(EditAnywhere)
		FGameplayTagContainer contextualTagForTask;

protected:

	void Activate() override;

	/** Tick function for this task, if bTickingTask == true */
	void TickTask(float DeltaTime) override;

	void FinishIdle()
	{
		Global::Log(CALLTRACETRIVIAL, TASK, "AT_Idle", "FinishIdle", TEXT(""));
		EndTask();
		taskFinished.Broadcast();
	}

	FSequenceTaskFinished taskFinished;

	///Allows evaluations to retrieve chunks of data from any type of object implementing interface
	const FDataMap& DataMap() const final { return dataMap; }


	UPROPERTY(EditAnywhere)
		FDataMap dataMap;

	/// Force implementers to provide an editable datamap
	FDataMap& MutableDataMap() final { return dataMap; }

private:

};

