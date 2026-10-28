// Copyright Jordan Cain, All Rights Reserved.


#include "Condition_ContextualTags.h"
#include "DataChunks/ContextualTags.h"

float UCondition_ContextualTags::Evaluate(const FSubjectDataView& data) const
{
	const FContextualTagsChunk* primary = Primary<FContextualTagsChunk>(data);
	if (!primary)
	{
		Global::Log(DATADEBUG, LogCondition, "UCondition_ContextualTags", "Evaluate", TEXT("Primary invalid. Description: %s"), *description.ToString());
		return 0.0f;
	}

	const FGameplayTagContainer& primaryValue = primary->Value();///Primary data is valid if we reach this point
	if (primaryValue.Num() < 1) { return 0.0f; }

	/// In condition, we can specify a range so that we can adjust score on a custom number of tags contained
	float score = 0.0f;
	for (int i = 0; i < tags.Num(); ++i)
	{
		const FGameplayTag tag = tags.GetByIndex(i);
		switch (conditional)
		{
			case EConditional_ContextualTagComparison::HasTags:					score += primaryValue.HasTag(tag); break;/// For each tag, increase score when contained
			case EConditional_ContextualTagComparison::DoesNotHaveTags:			score += !primaryValue.HasTag(tag); break;/// For each tag, increase score when not contained
			case EConditional_ContextualTagComparison::HasTagsExact:			score += primaryValue.HasTagExact(tag); break;
			case EConditional_ContextualTagComparison::DoesNotHaveTagsExact:	score += !primaryValue.HasTagExact(tag); break;
			default:
				return 0.0f;
		}
	}

	return score;
}
