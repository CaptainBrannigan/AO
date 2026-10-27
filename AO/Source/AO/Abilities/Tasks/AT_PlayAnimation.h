// Copyright Jordan Cain, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Abilities/SequenceTaskInterface.h"
#include "Data/DataMapInterface.h"
#include "GameplayTaskResource.h"
#include "AbilitySystemGlobals.h"
#include "AT_PlayAnimation.generated.h"

UCLASS()
class UTask_Resource_Animation_FullBody : public UGameplayTaskResource { GENERATED_BODY() };
UCLASS()
class UTask_Resource_Animation_UpperBody : public UGameplayTaskResource { GENERATED_BODY() };
UCLASS()
class UTask_Resource_Animation_LowerBody : public UGameplayTaskResource { GENERATED_BODY() };
UCLASS()
class UTask_Resource_Animation_Head : public UGameplayTaskResource { GENERATED_BODY() };
UCLASS()
class UTask_Resource_Animation_Arms : public UGameplayTaskResource { GENERATED_BODY() };

UCLASS(BlueprintType, Blueprintable, EditInlineNew)
/// <summary>
/// 
/// </summary>
class UAT_PlayAnimation : public UAbilityTask_PlayMontageAndWait, public IDataMapInterface, public ISequenceTaskInterface
{
	GENERATED_BODY()
public:

	UAT_PlayAnimation(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
		//AddData(CreateDefaultSubobject<ULocationToFace>("LocationToFace"));
		bTickingTask = false;
	}

	void EstablishTaskFinishedCallback(FScriptDelegate callback) override
	{
		OnInterrupted.Add(callback);
		OnCancelled.Add(callback);
		OnCompleted.Add(callback);
	}

	bool PreActivationInitialization(const FActionSequenceEntry& entryData, class UAI_SequentialActions& source) override
	{
		/*if (!HasData(ULocationToFace::StaticClass()))
		{
			return false;
		}*/

		UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Rate(playRate);

		MontageToPlay = montageForTask;
		Rate = playRate;
		StartSection = montageStartSection;
		AnimRootMotionTranslationScale = montageRootMotionTranslationScale;
		bStopWhenAbilityEnds = bStopAnimOnAbilityEnd;
		StartTimeSeconds = montageStartTimeSeconds;

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

	FGameplayTagContainer  GetContextualTagsOfTask() final { return contextualTagForTask; }
	UPROPERTY(EditAnywhere)
		FGameplayTagContainer contextualTagForTask;

protected:

	void Activate() override;

	/** Tick function for this task, if bTickingTask == true */
	void TickTask(float DeltaTime) override;

	UPROPERTY(EditAnywhere)
		TObjectPtr<UAnimMontage> montageForTask = nullptr;

	UPROPERTY(EditAnywhere)
		float playRate = 1.0f;

	UPROPERTY(EditAnywhere)
		FName montageStartSection = "None";

	UPROPERTY(EditAnywhere)
		float montageRootMotionTranslationScale = 1.0f;

	UPROPERTY(EditAnywhere)
		float montageStartTimeSeconds = 0.0f;

	UPROPERTY(EditAnywhere)
		bool bStopAnimOnAbilityEnd = true;

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

private:

};

