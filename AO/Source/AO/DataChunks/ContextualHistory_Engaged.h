// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "Purpose/DataChunks/ContextualHistory.h"
#include "Data/GlobalLog.h"
#include "ContextualHistory_Engaged.generated.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Contextual_Combat_Engaged);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Contextual_Combat_Engaged_1);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Contextual_Combat_Engaged_2);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Contextual_Combat_Engaged_3);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Contextual_Combat_Engaged_4);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Contextual_Combat_Engaged_5);

USTRUCT(BlueprintType, meta = (DisplayName = "Contextual History: Engaged"))
/// <summary>
/// ContextualHistory_Engaged is a datamodifier only, which can automatically track the number of engagements on a subject
/// (Was UContextualHistory_Engaged, a chunk subclass used only as a modifier, which is now a modifier type of its own)
/// </summary>
///Design: GAS GameplayEffects Stacking Engaged; This is a hand-built stacking duration effect: Engaged.1-5 with shorter relevance for higher counts
/// A GameplayEffect with StackingType = AggregateByTarget, StackLimitCount = 5 and a duration does this natively,
/// and the stack count can be read as a float (or the granted tag queried) instead of parsing the last character of the tag
struct AO_API FContextualHistoryModifier_Engaged : public FContextualHistoryModifier
{
	GENERATED_BODY()

	FContextualHistoryModifier_Engaged()
	{
		tagForAdjustment = TAG_Contextual_Combat_Engaged;
	}

	/// Override this in derived datachunk classes to allow evaluation of potential child tags from a parent
	FGameplayTag GetTagForAdjustment(const FContextualHistoryChunk& currentContextualHistoryForCandidate) const override;
};
