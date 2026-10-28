// Copyright Jordan Cain, All Rights Reserved.


#include "ContextualHistory_Engaged.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Contextual_Combat_Engaged, "Contextual.Combat.Engaged");
UE_DEFINE_GAMEPLAY_TAG(TAG_Contextual_Combat_Engaged_1, "Contextual.Combat.Engaged.1");
UE_DEFINE_GAMEPLAY_TAG(TAG_Contextual_Combat_Engaged_2, "Contextual.Combat.Engaged.2");
UE_DEFINE_GAMEPLAY_TAG(TAG_Contextual_Combat_Engaged_3, "Contextual.Combat.Engaged.3");
UE_DEFINE_GAMEPLAY_TAG(TAG_Contextual_Combat_Engaged_4, "Contextual.Combat.Engaged.4");
UE_DEFINE_GAMEPLAY_TAG(TAG_Contextual_Combat_Engaged_5, "Contextual.Combat.Engaged.5");

FGameplayTag FContextualHistoryModifier_Engaged::GetTagForAdjustment(const FContextualHistoryChunk& currentContextualHistoryForCandidate) const
{
	/// Here we're trying to determine what the highest count of engagements is and ensure it's time of updated
		/// However, as we don't have any unique identifiers, engagements can come from the same candidate
		/// So think of this as "times engaged" rather than "number of engaged combatants"

	FGameplayTag outTag = TAG_Contextual_Combat_Engaged_1;
	int highestEngagementCount = 0;

	for (const FContextHistoryEntry& entry : currentContextualHistoryForCandidate.Value())
	{
		Global::Log(DATATRIVIAL, LogData, "FContextualHistoryModifier_Engaged", "GetTagForAdjustment", TEXT("Tag: %s. Time since: %f")
			, *entry.contextualTag.ToString()
			, (FDateTime::UtcNow() - entry.timeOfContext).GetTotalSeconds()
		);

		if (entry.contextualTag.MatchesTag(TAG_Contextual_Combat_Engaged) )
		{
			TCHAR endChar = entry.contextualTag.ToString()[entry.contextualTag.ToString().Len() - 1];

			int engagedCount = FCString::Atoi(&endChar);

			/// By detracting the highest engaged count from the relevancy test
			/// Higher engaged counts will become irrelevant more quickly while lower engaged counts remain relevant longer
			/// This relates best to how often the subject is being engaged
			/// We firstly do engagedCount*2 to create a bigger discrepancy between the engagedCount timeSince values
			/// Currently max engaged count is 5, so 5*2 with the addition of 1 provides a flat 1 second buffer for max engagedCounts
			/// And with the higher discrepancy for lower values, that gives them a longer time to be considered relevant the lower engagedCount is
			if ((FDateTime::UtcNow() - entry.timeOfContext).GetTotalSeconds() < (11 - engagedCount*2) && engagedCount > highestEngagementCount)
			{
				Global::Log(DATATRIVIAL, LogData, "FContextualHistoryModifier_Engaged", "GetTagForAdjustment", TEXT("Engaged count is %d."), engagedCount);
				highestEngagementCount = engagedCount;
			}
		}
	}

	/// We have the last relevant engagement count
	/// We increase because this modification represents an additional engagement
	highestEngagementCount++;

	highestEngagementCount = FMath::Clamp(highestEngagementCount, 0, 5);

	/// We could just append highestEngagementCount to the string of the combat_engaged tag, but that would take unnecessary processing power
	/// We have to define the tags for them to exist anyways, so we may as well just use an cheap switch statement
	switch (highestEngagementCount)
	{
		default: case 0: case 1: outTag = TAG_Contextual_Combat_Engaged_1; break;
		case 2: outTag = TAG_Contextual_Combat_Engaged_2; break;
		case 3: outTag = TAG_Contextual_Combat_Engaged_3; break;
		case 4: outTag = TAG_Contextual_Combat_Engaged_4; break;
		case 5: outTag = TAG_Contextual_Combat_Engaged_5; break;
	}

	return outTag;
}
