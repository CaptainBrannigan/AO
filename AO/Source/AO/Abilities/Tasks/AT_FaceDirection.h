// Copyright Jordan Cain, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Abilities/SequenceTaskInterface.h"
#include "Data/DataMapInterface.h"
#include "DataChunks/ActorVector.h"
#include "GameplayTaskResource.h"
#include "AT_FaceDirection.generated.h"

UCLASS(MinimalAPI)
/// 
class ULocationToFace : public UActorVector { GENERATED_BODY() };

UCLASS()
class UTask_Resource_Rotation : public UGameplayTaskResource { GENERATED_BODY() };

UCLASS(BlueprintType, Blueprintable, EditInlineNew)
/// <summary>
/// 
/// </summary>
class UAT_FaceDirection : public UAbilityTask, public IDataMapInterface, public ISequenceTaskInterface
{
	GENERATED_BODY()
public:

	UAT_FaceDirection(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
		AddData(CreateDefaultSubobject<ULocationToFace>("LocationToFace"));
		bTickingTask = true;
	}

	void EstablishTaskFinishedCallback(FScriptDelegate callback) override
	{
		taskFinished.Add(callback);
	}

	bool PreActivationInitialization(const FActionSequenceEntry& entryData, class UAI_SequentialActions& source) override
	{
		if (!HasData(ULocationToFace::StaticClass()))
		{
			return false;
		}

		return true;
	}

	bool PostActivationAdjustment(const FActionSequenceEntry& entryData, class UAI_SequentialActions& source) override
	{
		return true;
	}

	/// Used so that we may dictate lifetimes of tasks that may not have an explicit end, such as a face direction task, if we want it to last as long as the current action is active
	ETaskDurationType GetTaskDurationType() override { return taskDuration; }

	UPROPERTY(EditAnywhere)
	ETaskDurationType taskDuration = ETaskDurationType::EndOnActionFinished;

	UPROPERTY(EditAnywhere)
	float rotateToDirectionSpeed = 1.0f;

	UPROPERTY(EditAnywhere)
	float rotationToLookDirectionTolerance = 5.0f;

	FGameplayTagContainer  GetContextualTagsOfTask() final { return contextualTagForTask; }
	UPROPERTY(EditAnywhere)
		FGameplayTagContainer contextualTagForTask;

protected:

	void Activate() override;

	/** Tick function for this task, if bTickingTask == true */
	void TickTask(float DeltaTime) override;

	/*UFUNCTION(BlueprintImplementableEvent)
	void TickTask_BP(float DeltaTime);*/

	/** End and CleanUp the task - may be called by the task itself or by the task owner if the owner is ending.
	 *	IMPORTANT! Do NOT call directly! Call EndTask() or TaskOwnerEnded()
	 *	IMPORTANT! When overriding this function make sure to call Super::OnDestroy(bOwnerFinished) as the last thing,
	 *		since the function internally marks the task as "Pending Kill", and this may interfere with internal BP mechanics
	 */
	void OnDestroy(bool bInOwnerFinished);

	FSequenceTaskFinished taskFinished;

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

	UPROPERTY(BlueprintReadWrite)
		TObjectPtr<class AAIController> actorCache = nullptr;
	UPROPERTY(BlueprintReadWrite)
		TObjectPtr<class APawn> pawnCache = nullptr;
	UPROPERTY(BlueprintReadWrite)
		TObjectPtr<AActor> targetActorCache = nullptr;

private:

	FRotator rotationToFaceLocation;
};

