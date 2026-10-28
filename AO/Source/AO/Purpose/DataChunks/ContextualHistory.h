// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "Data/Data.h"
#include "Data/DataMapInterface.h"
#include "Data/DataTags.h"
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Data")
	FGameplayTag contextualTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Data")
	FDateTime timeOfContext = 0.0f;

	FORCEINLINE bool operator ==(const FContextHistoryEntry& other) const
	{
		return contextualTag == other.contextualTag;
	}

	FORCEINLINE bool operator ==(const FGameplayTag inTag) const
	{
		return contextualTag == inTag;
	}
};

USTRUCT(BlueprintType, meta = (DisplayName = "Contextual History"))
/// <summary>
/// Contextual history holds context FGameplayTags with a time, establishing a timeline of context conditions may query (Data.ContextualHistory)
/// Context can be any gameplay tag, from input tags to positional tags
/// </summary>
///Design: GAS GameplayTags Cues History; Part of this history is "what is this actor doing right now" (Contextual.Position.Cover.Moving), which is what ASC loose/owned tags are for
/// Tags with a duration (engaged for 5s) map to a GameplayEffect with a duration that grants the tag, removing the need for time math in conditions
/// The "how long ago" half (TimeSinceNow) has no GAS equivalent, so this chunk stays for that
struct AO_API FContextualHistoryChunk : public FDataChunk
{
	GENERATED_BODY()

	FContextualHistoryChunk() : FDataChunk(TAG_Data_ContextualHistory) {}

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Data")
	TArray<FContextHistoryEntry> contextualHistory;

	const TArray<FContextHistoryEntry>& Value() const { return contextualHistory; }

	/// @return nullptr if the tag is not in the history
	const FContextHistoryEntry* Value(FGameplayTag inContextualTag) const
	{
		for (int i = contextualHistory.Num() -1; i >= 0 ; --i)/// Work through history backwards, as newest behaviors are last in array
		{
			if (contextualHistory[i].contextualTag.MatchesTagExact(inContextualTag)) { return &contextualHistory[i]; }/// Return the first entry that's found
		}

		return nullptr;
	}

	void AddToValue(FGameplayTag inContextualTag)
	{
		AddToValue(inContextualTag, FDateTime::UtcNow());
	}

	void AddToValue(FGameplayTag inContextualTag, FDateTime inTimeOfBehavior)
	{
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

	///Important for use with DataMapInterface HasData() in determining if a DataChunk can be accessed
	bool HasValue() const override
	{
		return true;/// Because this chunk's array is always valid, we can consider it has a modifiable value
		/// Otherwise HasData() will always return false and we won't be able to adjust the chunk after those checks
	}

	bool HasValue(FGameplayTag inContextualTag) const
	{
		return Value(inContextualTag) != nullptr;
	}

	FString Description() const override
	{
		FString history;

		for (const FContextHistoryEntry& entry : contextualHistory)
		{
			history += FString::Printf(TEXT("%s. Time of: %s; "), (entry.contextualTag.IsValid() ? *entry.contextualTag.ToString() : TEXT("Invalid Behavior Input")), *entry.timeOfContext.ToString());
		}

		return FDataChunk::Description() + TEXT("Behaviors: ") + history;
	}
};

USTRUCT(BlueprintType, meta = (DisplayName = "Contextual History"))
/// Adds a tag to (or refreshes its time in) a subject's contextual history. Creates the history if the subject doesn't have one
struct AO_API FContextualHistoryModifier : public FDataModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Data Adjustment")
	/// Tag to add to the history of context
	FGameplayTag tagForAdjustment;

	/// Override in derived modifiers to choose the tag from the current history (see FContextualHistoryModifier_Engaged)
	virtual FGameplayTag GetTagForAdjustment(const FContextualHistoryChunk& currentContextualHistoryForCandidate) const { return tagForAdjustment; }

	bool Apply(FDataMap& map, FGameplayTag key) const override
	{
		FContextualHistoryChunk* history = map.FindOrAdd<FContextualHistoryChunk>(key);
		if (!history)
		{
			return false;
		}

		const FGameplayTag tag = GetTagForAdjustment(*history);
		if (!tag.IsValid())
		{
			return false;
		}

		history->AddToValue(tag);/// We only add to value as this contextual history is time based
		return true;
	}

	FString Description() const override { return TEXT("Add ") + tagForAdjustment.ToString(); }
};
