// Copyright Jordan Cain. All Rights Reserved.


#include "Condition_ActorReference.h"
#include "DataChunks/ActorReference.h"

float UCondition_ActorReference::Evaluate(const FSubjectDataView& data) const
{
	const FActorReferenceChunk* primary = Primary<FActorReferenceChunk>(data);
	const FActorReferenceChunk* secondary = Secondary<FActorReferenceChunk>(data);
	if (!primary || !secondary)
	{
		Global::Log(DATADEBUG, LogCondition, "UCondition_ActorReference", "Evaluate", TEXT("Primary||Secondary invalid. Description: %s"), *description.ToString());
		return 0.0f;
	}

	if (!primary->HasValue()) { return 0.0f; }

	switch (conditional)
	{
		case EConditional_ActorReference::Is:
			return primary->IsSameActor(*secondary);
		case EConditional_ActorReference::IsNot:
			return !primary->IsSameActor(*secondary);
		default:
			return 0.0f;
	}
}
