// Copyright Jordan Cain. All Rights Reserved.


#include "Condition_ContextualHistory_EngagedByCountForSubject.h"
#include "Purpose/DataChunks/ContextualHistory.h"
#include "DataChunks/ContextualHistory_Engaged.h"

namespace
{
	/// 0 if the tag isn't one of the Engaged.N tags
	int32 EngagedCountOf(const FGameplayTag& inTag)
	{
		if (inTag == TAG_Contextual_Combat_Engaged_1) { return 1; }
		if (inTag == TAG_Contextual_Combat_Engaged_2) { return 2; }
		if (inTag == TAG_Contextual_Combat_Engaged_3) { return 3; }
		if (inTag == TAG_Contextual_Combat_Engaged_4) { return 4; }
		if (inTag == TAG_Contextual_Combat_Engaged_5) { return 5; }
		return 0;
	}
}

UCondition_ContextualHistory_EngagedByCountForSubject::UCondition_ContextualHistory_EngagedByCountForSubject()
{
	behavior = TAG_Contextual_Combat_Engaged;
	conditional = EConditional_ContextualHistory::TimeSinceNow;
}

float UCondition_ContextualHistory_EngagedByCountForSubject::Evaluate(const FSubjectDataView& data) const
{
	const FContextualHistoryChunk* primary = Primary<FContextualHistoryChunk>(data);
	if (!primary)
	{
		Global::Log(DATADEBUG, LogCondition, "UCondition_ContextualHistory_EngagedByCountForSubject", "Evaluate", TEXT("Primary invalid. Description: %s"), *description.ToString());
		return 0.0f;
	}

	const FDateTime now = FDateTime::UtcNow();
	int32 highestEngagementCount = 0;
	for (const FContextHistoryEntry& entry : primary->Value())
	{
		const int32 engagedCount = EngagedCountOf(entry.contextualTag);
		if (engagedCount <= highestEngagementCount)
		{
			continue;
		}

		const double timeSince = (now - entry.timeOfContext).GetTotalSeconds();
		Global::Log(DATATRIVIAL, LogCondition, "UCondition_ContextualHistory_EngagedByCountForSubject", "Evaluate", TEXT("Tag: %s. Time since: %f")
			, *entry.contextualTag.ToString()
			, timeSince
		);

		if (timeSince < maxTimeSinceEngagement)
		{
			highestEngagementCount = engagedCount;
		}
	}

	Global::Log(DATATRIVIAL, LogCondition, "UCondition_ContextualHistory_EngagedByCountForSubject", "Evaluate", TEXT("Engaged count is %d."), highestEngagementCount);
	return highestEngagementCount;
}
