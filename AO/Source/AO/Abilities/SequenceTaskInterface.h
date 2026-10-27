// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "Data/Data.h"
#include "Data/DataMapInterface.h"
#include "SequenceTaskInterface.generated.h"


UENUM(BlueprintType)
enum class EAIFocusType : uint8
{
	Default = 0
	, Move = 1
	, Gameplay = 2
	, LastFocusPriority = 3
};

UENUM(BlueprintType)
enum class ETaskDurationType : uint8
{
	EndOnTaskFinished/// This indicates the task is considered complete for the action when the task ends itself
	, EndOnActionFinished/// This indicates the task will continue to perform for as long as the action is active, and is ignored when action is checking for completion
};


USTRUCT(BlueprintType)
struct FTaskAndDataPair
{
	GENERATED_BODY();
public:

	UPROPERTY(EditAnywhere, DisplayName = "Description of Data Adjustment")
		FText description;

	UPROPERTY(EditAnywhere)
	ESubject subjectToRetrieveFrom = ESubject::Context;

	UPROPERTY(EditAnywhere)
	TSubclassOf<UDataChunk> dataToRetrieve = nullptr;

	//UPROPERTY(EditAnywhere)
	//int indexOfTaskToModify = -1;
	//int optionalTaskIndexToSetDataOn = -1;
	//UPROPERTY(EditAnywhere)
	//TSubclassOf<UGameplayTask> taskToSetDataOn = nullptr;
	UPROPERTY(EditAnywhere)
	TSubclassOf<UDataChunk> dataToModify = nullptr;

};

USTRUCT()
struct FContextAdjustmentFromAction
{
	GENERATED_BODY()

		UPROPERTY(EditAnywhere)
		/// A specifier indicating when to make an adjustment
		FText description;

	UPROPERTY(EditAnywhere, DisplayName = "Target of Adjusted Data")
		/// 
		ESubject subjectToAdjust = ESubject::None;

	UPROPERTY(Instanced, EditAnywhere, meta = (ShowInnerProperties))
		/// These DataChunks will either be adjusted or created 
		/// Adjustments will be determined by enum selection provided by DataChunk
		TObjectPtr<UDataChunk> dataAdjustment = nullptr;

};

USTRUCT(BlueprintType)
struct FActionEntry
{
	GENERATED_BODY();
public:

	UPROPERTY(EditAnywhere, DisplayName = "Description of Task")
		FText description;

	UPROPERTY(Instanced, EditAnywhere)
	TObjectPtr<class UGameplayTask> task = nullptr;

	UPROPERTY(EditAnywhere, DisplayName = "Resource to claim for Task")
	TSubclassOf<class UGameplayTaskResource> requiredResource = nullptr;

	UPROPERTY(EditAnywhere, meta = (TitleProperty=description))
	/// <summary>
	///  The idea is that we can share data between tasks to adjust how they perform
	/// </summary>
	TArray<FTaskAndDataPair> dataAdjustmentsForThisTask;
};

USTRUCT(BlueprintType)
struct FBehaviorTagAdjustment
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere)
		/// 
		FText description;

	UPROPERTY(Instanced, EditAnywhere, meta = (ShowInnerProperties))
		/// Adjust the behavior tags of this behavior to better reflect that behavior's impact
		TObjectPtr<class UContextualTags> behaviorAdjustment = nullptr;
};


USTRUCT(BlueprintType)
struct FActionSequenceEntry
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = "Behavior Data", DisplayName = "Description of Sequence Action")
	FText description;
	
	UPROPERTY(EditAnywhere, Category = "Behavior Data", DisplayName = "Task to Perform", meta = (TitleProperty=description))
	TArray<FActionEntry > tasksForAction;
	
	UPROPERTY(EditAnywhere, meta = (TitleProperty = description))
		/// <summary>
		///  Similar to purpose selection adjustments, actions may also adjust context data
		/// </summary>
		TArray<FContextAdjustmentFromAction> contextAdjustmentByAction;
};

/// For generic tasks which don't need to pass back results
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSequenceTaskFinished);

UINTERFACE(BlueprintType)
class USequenceTaskInterface : public UInterface
{
	GENERATED_BODY()

};

/// <summary>
///DataMapInteface is a means to enforce a data structure across any objects that can contain UDataChunks
///It also serves to abstract any objects implementing the interface to a base object pointer with only functionality critical to the data system
/// </summary>
class ISequenceTaskInterface
{
	GENERATED_BODY()
public:

	virtual void EstablishTaskFinishedCallback(FScriptDelegate callback) = 0;

	/// A good time to perform any modifications of data based on copied data from other tasks
	virtual bool PreActivationInitialization(const FActionSequenceEntry& entryData, class UAI_SequentialActions& source) = 0;

	/// This is the time to setup data, based on the task, that can be useful for other tasks
	/// Such as how after activating the move to task establishes data chunks for the path length and time length of movement
	virtual bool PostActivationAdjustment(const FActionSequenceEntry& entryData, class UAI_SequentialActions& source) = 0;

	/// Used so that we may dictate lifetimes of tasks that may not have an explicit end, such as a face direction task, if we want it to last as long as the current action is active
	virtual ETaskDurationType GetTaskDurationType() = 0;

	virtual FGameplayTagContainer  GetContextualTagsOfTask() = 0;

	///// Persistent contextual tags will exist throughout the lifetime of the behavior
	//virtual FGameplayTagContainer GetPersistentContextualTags() = 0;
	///// NonPersistent contextual tags only exist while the task is active
	//virtual FGameplayTagContainer GetNonPersistentContextualTags() = 0;
};
