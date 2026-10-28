// Copyright Jordan Cain. All Rights Reserved.


#include "Condition_Enum.h"
#include "Purpose/DataChunks/Enum.h"

float UCondition_Enum::Evaluate(const FSubjectDataView& data) const
{
	const FEnumChunk* primary = Primary<FEnumChunk>(data);
	if (!primary)
	{
		Global::Log(DATADEBUG, LogCondition, "UCondition_Enum", "Evaluate", TEXT("Primary invalid. Description: %s"), *description.ToString());
		return 0.0f;
	}

	const FEnumChunk* secondary = Secondary<FEnumChunk>(data);

	const uint8 primaryValue = primary->GetEnumValue();
	const uint8 secondaryValue = secondary ? secondary->GetEnumValue() : value;///Use found value of secondary, otherwise use specified value

	switch (conditional)
	{
		case EConditional_Enum::Equal:
			return primaryValue == secondaryValue;
		case EConditional_Enum::NotEqual:
			return primaryValue != secondaryValue;
		default:
			return 0.0f;
	}
}
