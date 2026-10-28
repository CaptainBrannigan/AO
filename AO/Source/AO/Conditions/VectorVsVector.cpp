// Copyright Jordan Cain. All Rights Reserved.


#include "VectorVsVector.h"
#include "Purpose/DataChunks/Vector.h"

float UVectorVsVector::Evaluate(const FSubjectDataView& data) const
{
	const FVectorChunk* primary = Primary<FVectorChunk>(data);
	const FVectorChunk* secondary = Secondary<FVectorChunk>(data);
	if (!primary || !secondary)
	{
		Global::Log(DATADEBUG, LogCondition, "UVectorVsVector", "Evaluate", TEXT("Primary||Secondary invalid. Description: %s"), *description.ToString());
		return 0.0f;
	}

	switch (conditional)
	{
		case EConditional_VvV::ScoreWithRangeValues:
			return FVector::Distance(primary->GetValue(), secondary->GetValue());
		default:
			return 0.0f;
	}
}
