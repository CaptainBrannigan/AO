// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Purpose/Condition.h"
#include "Condition_MassHistoryEvaluation.generated.h"

USTRUCT()
struct FContextualEntryExistsEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Condition")
	FGameplayTag contextualTag;

	UPROPERTY(EditAnywhere, Category = "Condition")
	/// Seconds an entry stays relevant after its time
	float timeSince = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Condition")
	/// Dependent on conditional, if this entry evaluates false then whole condition fails
	bool entryRequired = false;

	FORCEINLINE bool operator ==(const FContextualEntryExistsEntry& other) const
	{
		return contextualTag == other.contextualTag;
	}

	FORCEINLINE bool operator ==(const FGameplayTag inTag) const
	{
		return contextualTag == inTag;
	}
};

UENUM()
enum class EConditional_ContextualEntryExists : uint8
{
	None
	, HasValid/// Score is the number of tags with an entry newer than their timeSince
	, DoesNotHave/// Score is the number of tags with no entry, or an entry older than their timeSince
};

UCLASS(CollapseCategories)
/// <summary>
/// Evaluates several contextual history tags at once against their own relevancy times
/// Ported from Warzone. Its "entry has a time" check used FDateTime::GetSecond (seconds within the minute),
/// so an entry stamped on :00 read as having no time; this checks the ticks instead
/// </summary>
///Design: GAS GameplayEffects TagQuery History; With history tags granted by duration GameplayEffects, "has a recent X" is just "has tag X"
/// and this whole condition becomes an FGameplayTagQuery (all/any/none) against the subject's owned tags, snapshotted at occurrence time
class AO_API UCondition_MassHistoryEvaluation : public UCondition
{
	GENERATED_BODY()
public:

	UCondition_MassHistoryEvaluation();

	UPROPERTY(EditAnywhere, Category = "Condition", DisplayName = "Contextual Tags", meta = (DisplayPriority = "5"))
	TArray<FContextualEntryExistsEntry> contextualTags;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (DisplayPriority = "4"))
	///An Enum representing how the user wishes the two data types to be compared
	EConditional_ContextualEntryExists conditional = EConditional_ContextualEntryExists::None;

protected:

	float Evaluate(const FSubjectDataView& data) const override;
};
