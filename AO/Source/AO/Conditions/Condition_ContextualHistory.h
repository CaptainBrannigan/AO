// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Purpose/Condition.h"
#include "Data/DataTags.h"
#include "Condition_ContextualHistory.generated.h"

UENUM()
enum class EConditional_ContextualHistory : uint8
{
	None
	, TimeSinceNow/// Measures the time since the behavior and now
	, TimeSince/// Measures the time since the behavior and a secondary time of datachunk from secondary subject
};

UCLASS(CollapseCategories)
/// How long ago a tag entered a subject's contextual history
///Design: GAS GameplayEffects Duration History; "Tag added less than N seconds ago" is a duration GameplayEffect granting the tag,
/// which makes this a plain tag check. Keep this condition for open-ended "how long ago" curves
class AO_API UCondition_ContextualHistory : public UCondition
{
	GENERATED_BODY()
public:

	UCondition_ContextualHistory()
	{
		bHasRange = true;
		bHasSecondSubject = false;
		primaryKey = TAG_Data_ContextualHistory;
	}

	UPROPERTY(EditAnywhere, Category = "Condition", DisplayName = "Contextual Tag", meta = (DisplayPriority = "5"))
	///Primary data value of primary subject will be compared against this
	FGameplayTag behavior;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (DisplayPriority = "4"))
	///An Enum representing how the user wishes the two data types to be compared
	EConditional_ContextualHistory conditional = EConditional_ContextualHistory::None;

	UPROPERTY(EditAnywhere, Category = "Condition Data", DisplayName = "Score = Max if value not found", meta = (DisplayPriority = "14"))
	/// There may be instance where we wish to check a Not statement for contextual tag history
	/// If the tag doesn't exist that would be an automatic max score in this instance
	bool bMaxIfNotFound = false;

protected:

	/// For TimeSince, set secondaryKey to a time chunk (Data.Time.Occurrence, etc.)
	float Evaluate(const FSubjectDataView& data) const override;
};
