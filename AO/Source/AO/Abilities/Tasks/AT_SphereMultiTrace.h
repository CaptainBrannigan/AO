// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Data/DataMapInterface.h"
#include "Abilities/SequenceTaskInterface.h"
#include "AT_SphereMultiTrace.generated.h"

DECLARE_DELEGATE_OneParam(FHitResultsReturnDelegate, TArray<FHitResult>);

UCLASS()
/// <summary>
/// Given a set of parameters for the multi trace, will return the hit results via OnFinished
/// </summary>
class UAT_SphereMultiTrace : public UAbilityTask, public IDataMapInterface, public ISequenceTaskInterface
{
	GENERATED_BODY()

	FHitResultsReturnDelegate	OnFinished;

	virtual FString GetDebugString() const override;

	//UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	/// <summary>
	/// Sweeps a sphere along the given line and returns all hits encountered up to and including the first blocking hit.
	///	This trace finds the objects that RESPOND to the given TraceChannel
	/// </summary>
	/// <param name="OwningAbility"></param>
	/// <param name="WorldContextObject">The object providing the UWorld trace will be performed in.</param>
	/// <param name="Start">Start of the line on which to perform overlap spheres of Radius</param>
	/// <param name="End">End of the line on which to perform overlap spheres of Radius</param>
	/// <param name="Radius">Radius of the overlap sphere</param>
	/// <param name="TraceChannel"></param>
	/// <param name="bTraceComplex"></param>
	/// <param name="ActorsToIgnore"></param>
	/// <param name="DrawDebugType"></param>
	/// <param name="SortAscending">When true, will sort hit results by ascending distance, else by descending distance</param>
	/// <param name="bIgnoreSelf">If true, ignores WorldContextObject</param>
	/// <param name="TraceColor"></param>
	/// <param name="TraceHitColor"></param>
	/// <param name="DrawTime"></param>
	/// <returns>The task to be performed</returns>
	static UAT_SphereMultiTrace* QueryForHitResults(TWeakObjectPtr<UGameplayAbility> OwningAbility, TWeakObjectPtr<UObject> WorldContextObject, FVector Start, FVector End, float Radius, ETraceTypeQuery TraceChannel, bool bTraceComplex, TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, bool SortAscending = true, bool bIgnoreSelf = true, FLinearColor TraceColor = FLinearColor::Red, FLinearColor TraceHitColor = FLinearColor::Green, float DrawTime = 5.0f);

	virtual void Activate() override;

	void EstablishTaskFinishedCallback(FScriptDelegate callback) override
	{
		taskFinished.Add(callback);
	}

	bool PreActivationInitialization(const FActionSequenceEntry& entryData, class UAI_SequentialActions& source) override
	{
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

	TWeakObjectPtr<UObject> contextObj;
	FVector startPos;
	FVector endPos;
	float sphereRadius;
	ETraceTypeQuery traceChannel;
	bool traceComplex;
	TArray<AActor*> actorsToIgnore;
	EDrawDebugTrace::Type drawDebugType;
	bool sortAscending;
	bool ignoreSelf;
	FLinearColor traceColor;
	FLinearColor traceHitColor;
	float drawDebugTime;


	virtual void OnDestroy(bool AbilityIsEnding) override;
};
