// Copyright Jordan Cain. All Rights Reserved.
#pragma once

#include "Tasks/AITask_RunEQS.h"
#include "Data/DataMapInterface.h"
#include "Abilities/SequenceTaskInterface.h"
#include "DataChunks/ActorVector.h"
#include "DataChunks/ActorReference.h"
#include "DataAssets/QueryContextAsset.h"
#include "AT_RunEQSQuery.generated.h"

UCLASS(MinimalAPI)
/// Used to store the result or result's location
class UResult : public UActorVector { GENERATED_BODY() };

UCLASS(BlueprintType, Blueprintable, EditInlineNew)
class UAT_RunEQSQuery : public UAITask_RunEQS, public IDataMapInterface, public ISequenceTaskInterface
{
	GENERATED_BODY()

public:
	UAT_RunEQSQuery(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "AI|Tasks", meta = (DefaultToSelf = "Controller", BlueprintInternalUseOnly = "TRUE"))
	static UAT_RunEQSQuery* RunEQSQuery(AAIController* Controller, UEnvQuery* QueryTemplate, TEnumAsByte<EEnvQueryRunMode::Type> inRunMode);

	void EstablishTaskFinishedCallback(FScriptDelegate callback) override
	{
		taskFinished.Add(callback);
	}

	UPROPERTY(EditAnywhere)
		TObjectPtr<UQueryContextAsset> query;

	UPROPERTY(EditAnywhere)
		TEnumAsByte<EEnvQueryRunMode::Type> runMode = EEnvQueryRunMode::AllMatching;

	bool PreActivationInitialization(const FActionSequenceEntry& entryData, class UAI_SequentialActions& source) override;

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

	void ResultFound(TSharedPtr<FEnvQueryResult> Result);

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
};
