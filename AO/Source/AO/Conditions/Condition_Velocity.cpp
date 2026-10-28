// Copyright Jordan Cain, All Rights Reserved.


#include "Condition_Velocity.h"
#include "Purpose/DataChunks/Vector.h"

float UCondition_Velocity::Evaluate(const FSubjectDataView& data) const
{
	const FVectorChunk* primary = Primary<FVectorChunk>(data);
	if (!primary || !primary->HasValue())
	{
		Global::Log(DATADEBUG, LogCondition, "UCondition_Velocity", "Evaluate", TEXT("Primary invalid. Description: %s"), *description.ToString());
		return 0.0f;
	}

	switch (conditional)
	{
		case EConditional_Velocity::SpeedWithinRange:
			{
				const float speed = primary->GetValue().Length() / 100;/// uu/s to m/s
				Global::Log(DATATRIVIAL, LogCondition, "UCondition_Velocity", "Evaluate", TEXT("Primary velocity: %f"), speed);
				return speed;
			}
		default:
			return 0.0f;
	}
}
