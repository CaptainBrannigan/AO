// Copyright Jordan Cain. All Rights Reserved.


#include "Condition_ContextualHistory.h"
#include "Purpose/DataChunks/ContextualHistory.h"
#include "Purpose/DataChunks/TimeOf.h"

float UCondition_ContextualHistory::Evaluate(const FSubjectDataView& data) const
{
	const FContextualHistoryChunk* primary = Primary<FContextualHistoryChunk>(data);
	if (!primary || !behavior.IsValid())
	{
		Global::Log(DATADEBUG, LogCondition, "UCondition_ContextualHistory", "Evaluate", TEXT("Primary invalid. Description: %s"), *description.ToString());
		return 0.0f;
	}

	const FContextHistoryEntry* primaryValue = primary->Value(behavior);///Primary data is valid if we reach this point
	if (!primaryValue) { return bMaxIfNotFound * max; }/// if bMaxIfNotFound then we basically just return the max rang value, otherwise this will be 0

	float score = 0.0f;
	switch (conditional)
	{
		case EConditional_ContextualHistory::TimeSinceNow:
			{
				score = (FDateTime::UtcNow() - primaryValue->timeOfContext).GetTotalSeconds();

				Global::Log(DATATRIVIAL, LogCondition, "UCondition_ContextualHistory", "Evaluate", TEXT("Time of %s since now in seconds: %f.")
					, *primaryValue->contextualTag.ToString()
					, score
				);
			}
			break;
		case EConditional_ContextualHistory::TimeSince:
			{
				const FTimeChunk* secondary = Secondary<FTimeChunk>(data);
				if (!secondary)
				{
					Global::Log(DATADEBUG, LogCondition, "UCondition_ContextualHistory", "Evaluate", TEXT("Secondary invalid. Description: %s"), *description.ToString());
					return 0.0f;
				}

				score = (secondary->Value() - primaryValue->timeOfContext).GetTotalSeconds();

				Global::Log(DATATRIVIAL, LogCondition, "UCondition_ContextualHistory", "Evaluate", TEXT("Time of %s since %s in seconds: %f.")
					, *primaryValue->contextualTag.ToString()
					, *secondary->Key.ToString()
					, score
				);
			}
			break;
		default:
			return 0.0f;
	}

	return score;
}
