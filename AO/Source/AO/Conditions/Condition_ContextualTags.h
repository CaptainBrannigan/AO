// Copyright Jordan Cain, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Purpose/Condition.h"
#include "GameplayTagContainer.h"
#include "Condition_ContextualTags.generated.h"

UENUM()
enum class EConditional_ContextualTagComparison : uint8
{
	None
	, HasTags
	, DoesNotHaveTags
	, HasTagsExact
	, DoesNotHaveTagsExact
};

UCLASS(CollapseCategories)
/// Counts how many of tags a contextual tags chunk (FContextualTagsChunk, FBehaviorTagsChunk) does or doesn't have
///Design: GAS GameplayTags TagQuery; FGameplayTagQuery would let one condition express any/all/none combinations instead of a count
class AO_API UCondition_ContextualTags : public UCondition
{
	GENERATED_BODY()
public:

	UCondition_ContextualTags()
	{
		bHasRange = true;
		bHasSecondSubject = false;
	}

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (DisplayPriority = "4"))
	///An Enum representing how the user wishes the two data types to be compared
	EConditional_ContextualTagComparison conditional = EConditional_ContextualTagComparison::None;

	UPROPERTY(EditAnywhere, Category = "Condition", DisplayName = "Optional Value", meta = (DisplayPriority = "5"))
	///Primary data value of primary subject will be compared against this
	FGameplayTagContainer tags;

protected:

	float Evaluate(const FSubjectDataView& data) const override;
};
