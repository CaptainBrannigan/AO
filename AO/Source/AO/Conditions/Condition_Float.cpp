// Copyright Jordan Cain. All Rights Reserved.


#include "Condition_Float.h"
#include "Purpose/DataChunks/FloatChunk.h"

float UCondition_Float::Evaluate(const FSubjectDataView& data) const
{
	const FFloatChunk* primary = Primary<FFloatChunk>(data);
	if (!primary)
	{
		return 0.0f;
	}

	/// Refactor: Scoring Condition Purpose Evaluation; Rather than a 1:1 comparison, secondaryvalue should actually be utilized as the Max value of the response curve
		/// This way all comparisons are evaluated between 0-1 rather than 0 or 1
	switch (conditional)
	{
		case EConditional_Float::ScoreValueAlongCurve:
			return primary->GetValue();/// We only utilize primary value here because as baseline float is only meant for a single value evaluation
		default:
			return 0.0f;
	}
}
