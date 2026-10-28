// Copyright Jordan Cain. All Rights Reserved.


#include "Condition_ActorClassIsA.h"
#include "DataChunks/ActorReference.h"

UCondition_ActorClassIsA::UCondition_ActorClassIsA()
{
	bHasSecondSubject = false;
	primaryKey = TAG_Data_Actor;
}

float UCondition_ActorClassIsA::Evaluate(const FSubjectDataView& data) const
{
	const FActorReferenceChunk* primary = Primary<FActorReferenceChunk>(data);
	const UClass* primaryValue = primary ? primary->ResolvedClass() : nullptr;
	const UClass* secondaryValue = classToCompare.Get();
	if (!primaryValue || !secondaryValue)
	{
		Global::Log(DATADEBUG, LogCondition, "UCondition_ActorClassIsA", "Evaluate", TEXT("Primary class||Class to compare invalid. Description: %s"), *description.ToString());
		return 0.0f;
	}

	Global::Log(DATADEBUG, LogCondition, "UCondition_ActorClassIsA", "Evaluate", TEXT("Primary Class: %s. Class to compare: %s."), *primaryValue->GetName(), *secondaryValue->GetName());

	switch (conditional)
	{
		case EConditional_ClassComparison::Is:
			return primaryValue == secondaryValue;
		case EConditional_ClassComparison::IsA:
			return primaryValue->IsChildOf(secondaryValue);
		case EConditional_ClassComparison::IsNot:
			return primaryValue != secondaryValue;
		case EConditional_ClassComparison::IsNotA:
			return !primaryValue->IsChildOf(secondaryValue);
		default:
			return 0.0f;
	}
}
