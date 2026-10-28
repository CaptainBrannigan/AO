// Copyright Jordan Cain. All Rights Reserved.


#include "Condition_MassHistoryEvaluation.h"
#include "Purpose/DataChunks/ContextualHistory.h"

UCondition_MassHistoryEvaluation::UCondition_MassHistoryEvaluation()
{
	bHasRange = true;
	bHasSecondSubject = false;
	primaryKey = TAG_Data_ContextualHistory;
}

float UCondition_MassHistoryEvaluation::Evaluate(const FSubjectDataView& data) const
{
	const FContextualHistoryChunk* primary = Primary<FContextualHistoryChunk>(data);
	if (!primary)
	{
		Global::Log(DATADEBUG, LogCondition, "UCondition_MassHistoryEvaluation", "Evaluate", TEXT("Primary invalid. Description: %s"), *description.ToString());
		return 0.0f;
	}

	const FDateTime now = FDateTime::UtcNow();
	float score = 0.0f;
	switch (conditional)
	{
		case EConditional_ContextualEntryExists::HasValid:
			{
				for (const FContextualEntryExistsEntry& entry : contextualTags)
				{
					const FContextHistoryEntry* primaryValue = primary->Value(entry.contextualTag);
					if (!primaryValue)/// If there is not a valid entry
					{
						if (entry.entryRequired)
						{
							return 0.0f;
						}
						continue;
					}

					const double timeSinceContext = (now - primaryValue->timeOfContext).GetTotalSeconds();
					Global::Log(DATATRIVIAL, LogCondition, "UCondition_MassHistoryEvaluation", "Evaluate", TEXT("Tag: %s. Time Since: %f. Max time allowed: %f.")
						, *entry.contextualTag.ToString()
						, timeSinceContext
						, entry.timeSince
					);

					if (timeSinceContext >= entry.timeSince)/// But the time since is greater than the indicated relevancy time
					{
						/// Then the tag is considered as irrelevant
						if (entry.entryRequired)
						{
							return 0.0f;
						}
						continue;
					}

					score++;
					Global::Log(DATATRIVIAL, LogCondition, "UCondition_MassHistoryEvaluation", "Evaluate", TEXT("Has valid %s context."), *entry.contextualTag.ToString());
				}
			}
			break;
		case EConditional_ContextualEntryExists::DoesNotHave:
			{
				for (const FContextualEntryExistsEntry& entry : contextualTags)
				{
					const FContextHistoryEntry* primaryValue = primary->Value(entry.contextualTag);
					if (primaryValue && primaryValue->timeOfContext.GetTicks() != 0)/// If there is a valid entry, and that entry has a time
					{
						const double timeSinceContext = (now - primaryValue->timeOfContext).GetTotalSeconds();
						Global::Log(DATATRIVIAL, LogCondition, "UCondition_MassHistoryEvaluation", "Evaluate", TEXT("Tag: %s. Time Since: %f. Max time allowed: %f.")
							, *entry.contextualTag.ToString()
							, timeSinceContext
							, entry.timeSince
						);

						if (timeSinceContext >= entry.timeSince)/// But that time is greater than the indicated relevancy time
						{
							score++;/// Then the status is not relevant, so increase score
							continue;
						}

						if (entry.entryRequired)
						{
							return 0.0f;
						}
						continue;
					}

					score++;
					Global::Log(DATATRIVIAL, LogCondition, "UCondition_MassHistoryEvaluation", "Evaluate", TEXT("Did not have valid %s context."), *entry.contextualTag.ToString());
				}
			}
			break;
		default:
			return 0.0f;
	}

	return score;
}
