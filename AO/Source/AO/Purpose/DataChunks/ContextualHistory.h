// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "Data/Data.h"
#include "Misc/Timespan.h"
#include "NativeGameplayTags.h"
#include "Data/GlobalLog.h"
#include "ContextualHistory.generated.h"

USTRUCT(BlueprintType)
struct FContextHistoryEntry
{
	GENERATED_BODY()

	FContextHistoryEntry() 
	{
		contextualTag = FGameplayTag();
		timeOfContext = 0.0f;
	}

	FContextHistoryEntry(FGameplayTag inContextualTag, FDateTime inTimeOfBehavior)
		: contextualTag(inContextualTag),
		timeOfContext(inTimeOfBehavior)
	{}

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	//TSubclassOf<class UGA_Behavior> behavior = nullptr;
	FGameplayTag contextualTag;

	UPROPERTY(BlueprintReadOnly)
	FDateTime timeOfContext = 0.0f;

	FORCEINLINE bool operator ==(const FContextHistoryEntry& other) const
	{
		return contextualTag == other.contextualTag;
	}

	FORCEINLINE bool operator ==(const FGameplayTag inTag) const
	{
		return contextualTag == inTag;
	}

	/*FORCEINLINE bool operator ==(const TObjectPtr<AActor> inKnownLocation) const
	{
		return lastLocationOfPlayer == inKnownLocation;
	}*/
};
FORCEINLINE uint32 GetTypeHash(const FContextHistoryEntry& b)
{
	return FCrc::MemCrc32(&b, sizeof(FContextHistoryEntry));
}

UCLASS(MinimalAPI)
/// <summary>
/// Contextual history holds context FGameplayTags with a time, establishing a timeline of context conditions may query
/// Context can be any gameplay tag, from input tags to positional tags
/// </summary>
class UContextualHistory : public UDataChunk
{
	GENERATED_BODY()
public:

	UContextualHistory() {}

	///Overload this to require new object arguments
	UContextualHistory& Initialize()
	{
		return *this;
	}

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
		TArray<FContextHistoryEntry> contextualHistory;

	TArray<FContextHistoryEntry> Value() const { return contextualHistory; }
	FContextHistoryEntry Value(FGameplayTag inContextualTag) const 
	{ 
		for (int i = contextualHistory.Num() -1; i >= 0 ; --i)/// Work through history backwards, as newest behaviors are last in array
		{
			if (contextualHistory[i].contextualTag.MatchesTagExact(inContextualTag)) { return contextualHistory[i]; }/// Return the first entry that's found
		}

		return FContextHistoryEntry();
	}

	UFUNCTION(BlueprintCallable)
	void AddToValue(FGameplayTag inContextualTag)
	{
		AddToValue(inContextualTag, FDateTime::UtcNow());
	}

	void AddToValue(FGameplayTag inContextualTag, FDateTime inTimeOfBehavior)
	{
		/*if (contextualHistory.Num() > 0 && contextualHistory[0].timeOfContext.GetTotalSeconds() / 100000 > 5) /// Todo: Time BehaviorHistory; Solve why Timespan is 100k over valued
		{
			contextualHistory.RemoveAt(0, 1, true);/// Remove the first and oldest entry
		}*/
		int behaviorIndex = contextualHistory.IndexOfByKey(inContextualTag);
		if (behaviorIndex != INDEX_NONE)
		{
			Global::Log(DATATRIVIAL, LogData, "ContextHistory", "AddToValue", TEXT("Updating time of %s."), *inContextualTag.ToString());
			contextualHistory[behaviorIndex].timeOfContext = inTimeOfBehavior;
		}
		else
		{
			Global::Log(DATATRIVIAL, LogData, "ContextHistory", "AddToValue", TEXT("Adding %s."), *inContextualTag.ToString());
			contextualHistory.Add(FContextHistoryEntry(inContextualTag, inTimeOfBehavior));
		}
	}

	/// Override this method so that datachunks specified in a purpose asset may be modified when purpose is selected
	/// @param modificationContext: What purpose selection event will lead to this data modification
	/// @param inModifier: The encapsulated modifier specified on the derived DataChunks class
	TObjectPtr<UDataChunk> AdjustData(const UDataChunk& inModifier) final
	{
		if (!inModifier.IsA<UContextualHistory>())
		{
			Global::Log(DATADEBUG, LogData, "UContextualTags", "AdjustData", TEXT("Provided a non-ContextualTags modifier chunk."));
			return this;
		}

		/// We only add to value as this contextual history is time based
		AddToValue(Cast<UContextualHistory>(&inModifier)->GetTagForAdjustment(contextualHistory));

		return this;
	}

	virtual FGameplayTag GetTagForAdjustment(const TArray<FContextHistoryEntry>& currentContextualHistoryForCandidate) const { return tagForAdjustment; }

	///Important for use with DataMapInterface HasData() in determining if a DataChunk can be accessed
	///Override this with determination of whether a value has been set for this chunk or not
	bool HasValue() final
	{
		//return contextualHistory.Num() > 0;
		return true;/// Because this chunk's array is always valid, we can consider it has a modifiable value
		/// Otherwise HasData() will always return false and we won't be able to adjust the chunk after those checks
	}

	bool HasValue(FGameplayTag inContextualTag) 
	{
		if (HasValue())
		{
			for (const FContextHistoryEntry& entry : contextualHistory)
			{
				if (entry.contextualTag.MatchesTagExact(inContextualTag)) { return true; }
			}
		}
		return false;
	}

	///Override this to provide more detailed information about a data chunk
	///Call Super::Description() to include string about EDataType
	FString Description() const final
	{
		FString history;

		for (auto& entry : contextualHistory)
		{
			history += FString::Printf(TEXT("%s. Time of: %s; "), (entry.contextualTag.IsValid() ? *entry.contextualTag.ToString() : TEXT("Invalid Behavior Input")), *entry.timeOfContext.ToString());
		}

		return Super::Description() + TEXT("Behaviors: ") + history;
	}

	///If implicit conversion is desired
	//operator insertdatatype() { return instertvaluemember; }
protected:

	UPROPERTY(EditAnywhere, Category = "Data Adjustment")
		/// Tag to add to the history of context
		FGameplayTag tagForAdjustment;

};
