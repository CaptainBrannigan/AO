// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Containers/Queue.h"
#include <atomic>
#include "UObject/ObjectKey.h"
#include "Data/GlobalLog.h"
#include "Data/Data.h"
#include "Condition.h"
#include "UObject/Interface.h"
#include "Data/DataMapInterface.h"
#include "Misc/Timespan.h"
#include "PurposeEvaluationThread.generated.h"

AO_API DECLARE_LOG_CATEGORY_EXTERN(LogOccurrence, Log, All);

#pragma region PurposeSystem

class UPurposeAbilityComponent;

UENUM(BlueprintType)//"BlueprintType" is essential to include
/// Utilized by managers to determine how to handle various states of Purpose
enum class EPurposeState : uint8
{
	None UMETA(DisplayName = "None") ///Default to ensure data is set with HasData() for enums
	, Ongoing/// The purpose has not yet been completed
	, Complete
	, Ending/// This purpose is in the process of being ended and cleaned up
};

UENUM(BlueprintType)
enum class EPurposeSelectionEvent : uint8
{
	None/// Default to ensure a value is actually selected or not
	, OnSelected
	, OnFinished
};

USTRUCT()
/// When a purpose is selected or finished, change (or create) one chunk on one subject
/// See FDataModifier for what modifiers are for
struct AO_API FPurposeModificationEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Data Adjustment")
	/// A specifier indicating when to make an adjustment
	FText description;

	UPROPERTY(EditAnywhere, Category = "Data Adjustment")
	/// A specifier indicating when to make an adjustment
	EPurposeSelectionEvent selectionEvent = EPurposeSelectionEvent::None;

	UPROPERTY(EditAnywhere, Category = "Data Adjustment", DisplayName = "Target of Adjusted Data")
	///
	ESubject subjectToAdjust = ESubject::None;

	UPROPERTY(EditAnywhere, Category = "Data Adjustment", DisplayName = "Data to Adjust", meta = (Categories = "Data"))
	/// Key of the chunk to adjust on the subject. If the subject doesn't have it, the modifier creates it
	FGameplayTag dataKey;

	UPROPERTY(EditAnywhere, Category = "Data Adjustment", meta = (ExcludeBaseStruct))
	/// How the chunk is adjusted (Float, Contextual Tags, Contextual History, Time...)
	TInstancedStruct<FDataModifier> dataAdjustment;

};

USTRUCT(BlueprintType)
struct AO_API FPurpose
{
	GENERATED_BODY()
public:

	/// In case at some point we attempt to return an empty purpose, ensure everything has default empty values
	/// Ran into a bug where an return FGameplayTagContainer(); had thousands of gameplaytag entries
	FPurpose()
	{
		conditions = TArray<TObjectPtr<UCondition>>();
		dataAdjustmentsForPurposeEvents = TArray<FPurposeModificationEntry>();
	}

	UPROPERTY(EditAnywhere, Category = "Purpose", meta = (DisplayPriority = "0"))
	FString descriptionOfPurpose = "";

	UPROPERTY(Instanced, EditAnywhere, Category = "Purpose", meta = (TitleProperty=description, ShowInnerProperties))
		/// These conditions establish how a purpose fits to a context
		/// They are critical to create legible and realistic purpose for both individual actors and groups of actors
		/// The more conditions present, the greater the potential weight of a purpose
	TArray<TObjectPtr<UCondition>> conditions;
	const TArray<TObjectPtr<UCondition>>& GetConditions() const { return conditions; }

	UPROPERTY(EditAnywhere, Category = "Purpose", meta = (TitleProperty=description))
		/// These DataChunks will either be adjusted or created
		/// Adjustments are determined by the modifier selected on each entry
	TArray<FPurposeModificationEntry> dataAdjustmentsForPurposeEvents;

	UPROPERTY(Instanced, EditAnywhere, Category = "Purpose", DisplayName = "Behavior")
		TObjectPtr<class UBehavior_AI> behaviorAbility = nullptr;

	///Design: GAS GameplayAbilities Purpose; A purpose is "conditions that score whether an ability should run" + "the ability"
	/// GAS already has the activation half: ActivationRequiredTags/BlockedTags, CanActivateAbility and Cost/Cooldown GameplayEffects
	/// Cheap tag gates on behaviorAbility could pre-filter purposes before any condition is scored (a required tag missing = skip the purpose)
	/// Cooldown effects would also replace hand-rolled "don't repeat this behavior for N seconds" history conditions

	/// @param potentialScore = every condition with a exponential decay additional (1 + decaying additional)
	/// @param totalWeight = The weight of each condition->weight added together
	void Potential(float& potentialScore, float& totalWeight) const
	{
		float num = conditions.Num();
		if (num == 0) { return; }

		for (float i = 1.0f; i <= num; ++i)
		{
			float x = (1.0f / i);

			potentialScore += FMath::Pow(i, x);///For every condition, there is a diminishing addition to a base 0-1 score
			///This means that purposes with more conditions will have a higher score potential than those with less
			///Allowing us to prioritize purposes that meet more conditions without ruining the potential of purposes with less conditions
			totalWeight += conditions[i-1] ? conditions[i - 1]->weight : 0;///i-1 as the above utilizes index as position in array
		}
	}

	const TObjectPtr<UCondition> ConditionData(int iteration) const
	{
		if (conditions.IsValidIndex(iteration))
		{
			return conditions[iteration];
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("%d is not a valid index for conditions. EventAsset::ConditionData()"), iteration);
		}

		return nullptr;
	}

	const TArray<FPurposeModificationEntry>& DataAdjustments() const { return dataAdjustmentsForPurposeEvents; }
};

USTRUCT(BlueprintType)
/// The subjects of a context: which object fills each ESubject role
/// Subjects are held weakly: a context never keeps a subject alive, and a destroyed subject is detected safely
/// (Previously TScriptInterface, which GC didn't track because contexts aren't UPROPERTYs, so a destroyed subject was a dangling pointer)
struct AO_API FSubjectMap
{
	GENERATED_BODY()
public:

	FSubjectMap() {}

	UPROPERTY(VisibleAnywhere, Category = "Purpose")
	///Link an enum representing a subject with a corresponding object which holds DataChunks
	TMap<ESubject, TWeakObjectPtr<UObject>> subjects;

	/// Game thread. Objects that don't implement IDataMapInterface are refused
	bool Add(ESubject inSubject, UObject* inObject)
	{
		if (!IsValid(inObject) || !inObject->Implements<UDataMapInterface>())
		{
			return false;
		}
		subjects.Add(inSubject, inObject);
		return true;
	}

	bool Add(ESubject inSubject, const TScriptInterface<IDataMapInterface>& inObject) { return Add(inSubject, inObject.GetObject()); }

	/// Game thread
	UObject* Get(ESubject inSubject) const
	{
		const TWeakObjectPtr<UObject>* subject = subjects.Find(inSubject);
		return subject ? subject->Get() : nullptr;
	}

	/// Game thread
	TScriptInterface<IDataMapInterface> Interface(ESubject inSubject) const
	{
		return TScriptInterface<IDataMapInterface>(Get(inSubject));
	}

	/// Game thread
	bool Contains(ESubject inSubject) const { return Get(inSubject) != nullptr; }
};

USTRUCT(BlueprintType)
struct AO_API FContextData
{
	GENERATED_BODY()

#pragma region ContextData
public:

	/// In case at some point we attempt to return an empty FContextData, ensure everything has default empty values
	/// Ran into a bug where an return FGameplayTagContainer(); had thousands of gameplaytag entries
	FContextData()
	{
		cachedScoreOfPurpose = 0.0f;
	}

	FContextData(const FPurpose& inPurpose, const FSubjectMap& inSubjectMap, const FDataMap& inContextData, TWeakObjectPtr<UPurposeAbilityComponent> inPurposeOwner, const FString& inOwnerName = "")
		: purpose(inPurpose)
		, subjectMap(inSubjectMap)
		, contextData(inContextData)
		, purposeOwner(inPurposeOwner)
	{
		contextDataPurposeName = purpose.descriptionOfPurpose + "(" + inOwnerName + ")";
	}

	FPurpose purpose;

	/// We store the score of the purpose at the time of it's selection so that we may easily compare purposes against each other outside of purpose selection for an individual
	float cachedScoreOfPurpose = 0;

	/// FPlatformTime::Seconds() when the occurrence that selected this purpose was snapshot. 0 for contexts that didn't come from an occurrence
	double snapshotTimeSeconds = 0.0;

	/// Essentially this is the context
	/// Every context will store relevant ESubjects with their data maps to be evaluated against Conditions
	FSubjectMap subjectMap;

	/// We can store data specific to the context and not a subject here, such as a last known position, a type of sound heard, etc.
	/// During evaluation this is the ESubject::Context subject
	FDataMap contextData;

	TWeakObjectPtr<UPurposeAbilityComponent> purposeOwner = nullptr;

	inline bool ContextIsValid() const { return purpose.GetConditions().Num() > 0; }

	inline bool HasPurpose() const { return purpose.conditions.Num() > 0; }

	FString GetOwnerName() const;

	///@return FString: If a purpose has been found for this contextData, returns the name of the purpose itself
	FORCEINLINE FString GetName() const
	{
		return contextDataPurposeName;
	}

	/// Game thread
	FString Description() const;

	/// This could be unnecessarily expensive, and if we aren't actually going to log it no reason to bother
	FString Description(EHierarchicalCalltraceVerbosity verbosity) const
	{
		const UGlobalLogSettings* LogSettings = GetDefault<UGlobalLogSettings>();

		if (!LogSettings || static_cast<int>(LogSettings->GlobalLogVerbosity) < static_cast<int>(verbosity))
		{
			return "";
		}///Verbosity is too high, so we don't want to log

		return Description() + LINE_TERMINATOR;/// So that the calling method is shown below rather than after last line
	}

	///The name adjusted to represent the selected purpose
	FString contextDataPurposeName = "contextData";

#pragma endregion

#pragma region Subjects
public:

	/// Game thread
	TObjectPtr<UObject> Subject(ESubject inSubject) const
	{
		if (UObject* subject = subjectMap.Get(inSubject))
		{
			return subject;
		}

		Global::Log(DATADEBUG, PURPOSE, "FContextData", "Subject", TEXT("Subject %s is not contained."), *Global::EnumValueOnly<ESubject>(inSubject));
		return nullptr;
	}

	template<class T>
	TObjectPtr<T> Subject(ESubject inSubject) const
	{
		return Cast<T>(Subject(inSubject));
	}

	///Returns the data map interface of a given subject
	/// ESubject::Context is not an object, use contextData (or DataMapForSubject) for it
	TScriptInterface<IDataMapInterface> DataMapInterfaceForSubject(ESubject inSubject) const
	{
		if (HasSubject(inSubject))
		{
			return subjectMap.Interface(inSubject);
		}

		Global::Log(DATADEBUG, PURPOSE, "FContextData", "DataMapInterfaceForSubject", TEXT("Subject %s is not contained or invalid."), *Global::EnumValueOnly<ESubject>(inSubject));
		return nullptr;
	}

	/// Live DataMap of a subject, ESubject::Context included. Game thread
	const FDataMap* DataMapForSubject(ESubject inSubject) const
	{
		if (inSubject == ESubject::Context) { return &contextData; }

		TScriptInterface<IDataMapInterface> subject = DataMapInterfaceForSubject(inSubject);
		return subject ? &subject->DataMap() : nullptr;
	}

	bool HasSubject(ESubject inSubject) const
	{
		return subjectMap.Contains(inSubject);
	}

	bool HasData(ESubject inSubject, FGameplayTag inKey) const
	{
		const FDataMap* map = DataMapForSubject(inSubject);
		return map && map->Has(inKey);
	}

	///Add a DataMapInterface object to a given subject
	///@Param allowSwap == true: If there is already a DataMapInterface object linked to subject, replace
	bool AddSubject(ESubject inSubject, TScriptInterface<IDataMapInterface> inDataObject, bool allowSwap = false)
	{
		if (!IsValid(inDataObject.GetObject()))
		{
			Global::LogError(PURPOSE, "FContextData", "AddSubject", TEXT("Attempting to add null data for type: %s."), *Global::EnumValueOnly<ESubject>(inSubject));
			return false;
		}

		if (HasSubject(inSubject) && !allowSwap)
		{
			return false;
		}

		return subjectMap.Add(inSubject, inDataObject);
	}

	bool RemoveSubject(ESubject inSubject)
	{
		subjectMap.subjects.Remove(inSubject);
		return true;
	}

	///Get DataChunk of T from ESubject. Game thread
	///@return nullptr if the subject or chunk is missing (previously returned a NewObject)
	template<class T = FDataChunk>
	const T* DataChunk(ESubject inSubject, FGameplayTag inKey) const
	{
		const FDataMap* map = DataMapForSubject(inSubject);
		return map ? map->Find<T>(inKey) : nullptr;
	}

	/// Apply one modifier to the live data of a subject (ESubject::Context included). Game thread
	bool AdjustData(ESubject target, FGameplayTag inKey, const FDataModifier* modifier);

	/// Will perform Purpose's DataAdjustements as appropriate. Game thread
	/// @param inEventTypeToAdjust: Determines what data can be adjusted based on selection made in Asset
	/// @param inLogCat: Allows us to clarify where adjustment is coming from in fail case
	/// @param callingMethodName: Allows us to clarify where adjustment is coming from in fail case
	void AdjustDataIfPossible(EPurposeSelectionEvent inEventTypeToAdjust, const FLogCategoryBase& inLogCat, FString callingMethodName);

#pragma endregion
};

/// Everything the evaluation thread knows about one candidate, copied on the game thread when the occurrence is raised
/// The worker never touches the candidate's component: identity is compared with TObjectKey, data comes from the snapshot
struct AO_API FCandidateSnapshot
{
	/// Only handed back to the game thread with the selected purpose, never dereferenced by the worker
	TWeakObjectPtr<UPurposeAbilityComponent> component;
	TObjectKey<UObject> componentKey;

	/// The candidate's data with its current behavior's context data appended (ESubject::Candidate)
	FDataMap data;

	/// cachedScoreOfPurpose of the current behavior, the score a new purpose must beat
	float currentScore = 0.0f;

	/// For "already performing this purpose" checks
	TObjectKey<UObject> currentEventTarget;
	const UObject* currentBehaviorAbility = nullptr;/// Identity only, never dereferenced

	FString debugName;
};

/// <summary>
/// An occurrence queued to an evaluation thread
/// Built on the game thread (PurposeSystem::Occurrence), so every DataMap here is a snapshot the game thread can no longer change
/// </summary>
///Design: Purpose Performance Snapshot; Every occurrence snapshots every candidate (a copy of each DataMap)
/// If that shows up in profiling, components can cache an immutable TSharedRef<const FDataMap> snapshot rebuilt only when OnDataMapChanged fires
/// (plus once per frame for world-derived chunks), so occurrences share one copy per candidate instead of making their own
struct AO_API FOccurrence
{
	/// These are all the candidates we wish to evaluate for a new purpose against the provided context
	TArray<FCandidateSnapshot> candidates;

	/// Live subjects, only used to build the FContextData handed back to the game thread
	FSubjectMap subjectMap;

	/// Live (unresolved) copy of the context data, handed back with the selected purpose
	FDataMap contextData;

	/// What conditions read for every subject except Candidate. ESubject::Context is included
	TMap<ESubject, FDataMap> subjectSnapshots;

	TObjectKey<UObject> instigatorKey;
	TObjectKey<UObject> eventTargetKey;
	FString instigatorName;

	/// FPlatformTime::Seconds() when the snapshot was taken. Compared at evaluation to measure how old the data is
	double snapshotTimeSeconds = 0.0;
};

///Umbrella type for multiple queues of UContextData_Deprecated
typedef TQueue<FOccurrence> PurposeQueue;

class FAsyncGraphTask_PurposeSelected;

/// <summary>
///The purpose evaluation thread is the foundation of all gameplay logic
///Receiving a context data, the thread will compare that context data to a relevant layer of purpose
///If a purpose is found, it is then sent back to the owner
///
///As such, a large number of calculations must be performed on a large number of queued context datas
///While there is no guarantee of timely evaluation, it allows a huge number of purposes to be evaluated and selected without locking up the gamethread
///This allows the Event System to be very robust, allowing not only consideration of each action each actor makes
///But also allowing all other actors to potentially react to that action
/// </summary>
class FPurposeEvaluationThread : public FRunnable
{
public:

	FPurposeEvaluationThread(const TArray<FPurpose>& inPurposeCache, const TArray<TObjectPtr<UPurposeAbilityComponent>>& inCandidateCache)
		: purposeCacheForBackgroundThread(inPurposeCache)
		, candidateCache(inCandidateCache)
	{

	}

	///Controls while loop execution of Run()
	bool stopThread = true;

	///Essentially the speed that the background thread will call Run(); thanks to FPlatformMisc::Sleep()
	float tickTimer = 0.05f;

	/// Design: BackgroundThread Purpose; to implement a pause using FRunnable::Suspend
		/// halt any evaluation regardless of status
		/// Throw context data into a tgraphtask to re-add to it's queue

	///Virtual methods from FRunnable
	bool Init() final;
	void Stop() final;
	virtual void Exit() override;

	///Executed so long as Init() returns true
	///Runs Recursively to evaluate queues until thread is told to stop and/or shutdown
	uint32 Run() override;

	///@return bool: True when the purpose was stored to a queue to be evaluated at some point
	bool QueuePurpose(FOccurrence&& potentialPurposesToQueue)
	{
		if(potentialPurposeQueue.Enqueue(MoveTemp(potentialPurposesToQueue)))
		{
			/// Since TQueue doesn't have a .Num()
			numQueueItems++;/// ensure this thread has an increased number of queue items
			return true;
		}

		return false;
	}

	/// @param dequeudPurpose; TQueue requires an out param to dequeue to
	///@return bool: False only when a purpose was not dequeued
	bool DequeuePurpose(FOccurrence& dequeuedPurpose)
	{
		if (potentialPurposeQueue.Dequeue(dequeuedPurpose))
		{
			/// Since TQueue doesn't have a .Num()
			numQueueItems--;/// ensure this thread has a decreased number of queue items
			return true;
		}

		return false;
	}

	/// @param purposeToEvaluate; The combination of context, subjects and potential purposes to evaluate to a single purpose for a unique subject. After evaluation, the data may be copied to further the purpose system, but this struct will be destroyed regardless.
	///@return bool:
	bool SelectPurposeIfPossible(const FOccurrence& purposeToEvaluate);

	const TArray<FPurpose>& GetPurposeCacheForBackgroundThread() const { return purposeCacheForBackgroundThread; }
	const TArray<TObjectPtr<UPurposeAbilityComponent>>& GetCandidateCache() const { return candidateCache; }

	const int GetNumQueuedPurposes() { return numQueueItems; }

	/// Oldest snapshot seen at evaluation, in seconds. For the data age test (see AO.Purpose.SnapshotAgeWarningSeconds)
	double GetMaxSnapshotAgeSeconds() const { return maxSnapshotAgeSeconds; }

protected:
	const TArray<FPurpose>& purposeCacheForBackgroundThread;

	const TArray<TObjectPtr<UPurposeAbilityComponent>>& candidateCache;

	PurposeQueue potentialPurposeQueue;

	/// TQueues do not have a .Num() to track how many queued items
	/// We can track the number of queued items so that if we want to have multiple threads we can simply compare queue to the thread with less items
	std::atomic<int32> numQueueItems = 0;

	double maxSnapshotAgeSeconds = 0.0;

	bool CreateAsyncTask_PurposeSelected(FContextData context);

};

namespace PurposeSystem
{
	/// Game thread. Snapshots the subjects, context and candidates, then queues the occurrence to the least busy thread
	/// Candidates are the thread's candidate cache
	AO_API bool Occurrence(const FSubjectMap& subjectsOfContext, const FDataMap& context, const TArray<FPurposeEvaluationThread*>& backgroundThreads);

	/// Game thread. As above, with explicit candidates
	AO_API bool Occurrence(const TArray<TObjectPtr<UPurposeAbilityComponent>>& inCandidates, const FSubjectMap& subjectsOfContext, const FDataMap& context, const TArray<FPurposeEvaluationThread*>& backgroundThreads);

	/// The background thread has found a purpose
	/// It then calls this from an AsyncTask, which executes on the game thread
	/// It requests the owner of the purpose store the purpose
	/// Then performs the purpose's OnSelected data adjustments
	AO_API void PurposeSelected(FContextData contextOfSelectedPurpose);
}

#pragma endregion

/// <summary>
/// The Event thread handles:
///		Selecting Objectives for the AI belonging to a Manager with a selected Goal
///		Selecting Goals for Managers receiving an Event
///		Finding Events for Occurrences
/// </summary>
class FEventThread : public FPurposeEvaluationThread
{
public:

	~FEventThread();

protected:

};

/// <summary>
/// The Actor thread handles:
///		Finding a Reaction Objective for direct actions
///		Finding Tasks for Objectives
/// </summary>
class FActorThread : public FPurposeEvaluationThread
{
public:

	~FActorThread();



};

/// <summary>
/// The Companion thread handles:
///		Finding all companion purposes on the client side
/// </summary>
class FCompanionThread : public FEventThread
{
public:
	~FCompanionThread();


};

#pragma region TGraphTasks

/// <summary>
/// ASyncGraphTask_PurposeSelected is utilized by the background thread to send a ContextData with a purpose back to the gamethread
/// </summary>
class FAsyncGraphTask_PurposeSelected
{
protected:
	FContextData contextData;
//	FPurposeEvaluationThread& callingThread;///Used to detect stopThread
	bool shouldAbandon = false;

public:
	FAsyncGraphTask_PurposeSelected(FContextData inContext)
		: contextData(inContext)
	{
	}

	virtual ~FAsyncGraphTask_PurposeSelected()
	{
	}

	FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncGraphTask_PurposeSelected, STATGROUP_TaskGraphTasks); }
	static FORCEINLINE ENamedThreads::Type GetDesiredThread() { return ENamedThreads::GameThread; }
	static FORCEINLINE ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::FireAndForget; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		shouldAbandon ? Cancel() : PurposeSelected();
	}

	void PurposeSelected();

	/// Can be called by any thread in order to ensure that the purpose chain halts
	/// When the task attempts to DoTask(), it will instead cancel
	void Abandon() { shouldAbandon = true; }

protected:
	void Cancel()
	{
	}

};

#pragma endregion
