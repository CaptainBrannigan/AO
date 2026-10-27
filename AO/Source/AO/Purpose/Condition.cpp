// Copyright Jordan Cain. All Rights Reserved.


#include "Condition.h"
#include "Data/GlobalLog.h"

DEFINE_LOG_CATEGORY(LogCondition);

UCondition::UCondition()
{
	/*///Establish a default response curve
	responseCurve = CreateDefaultSubobject<UCurveFloat>(FName("ConditionResponseCurve"));

	///Ensure the curve is linear
	auto key = responseCurve->FloatCurve.AddKey(0.f, 0.f);
	responseCurve->FloatCurve.AddKey(1.f, 1.f);
	responseCurve->FloatCurve.SetKeyTime(key, 1.0f);
	responseCurve->FloatCurve.SetKeyInterpMode(key, RCIM_Linear);*/
}

float UCondition::EvaluateConditionInternal_Implementation(const UDataChunk* primaryDataChunk, const UDataChunk* secondaryDataChunk, class UPurposeAbilityComponent* owner) const
{
	Global::LogError(LogCondition, GetName(), "EvaluatConditionInternal_Implementation", TEXT("This is never supposed to be called!"));
	return 0.0f;
}
