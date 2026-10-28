// Copyright Jordan Cain. All Rights Reserved.


#include "Condition_RelativePosition.h"
#include "DataChunks/ActorViewPoint.h"

UCondition_RelativePosition::UCondition_RelativePosition()
{
	bHasRange = true;
	primaryKey = TAG_Data_ViewPoint;
}

float UCondition_RelativePosition::Evaluate(const FSubjectDataView& data) const
{
	const FActorViewPointChunk* primary = Primary<FActorViewPointChunk>(data);
	const FVectorChunk* secondary = Secondary<FVectorChunk>(data);
	if (!primary || !secondary || !primary->HasValue() || !secondary->HasValue())
	{
		Global::Log(DATADEBUG, LogCondition, "UCondition_RelativePosition", "Evaluate", TEXT("Primary||Secondary invalid. Description: %s"), *description.ToString());
		return 0.0f;
	}

	const FVector primaryLocation = primary->GetValue();
	const FVector secondaryLocation = secondary->GetValue();

	switch (conditional)
	{
		case EConditional_RelativePosition::Dot:
			{
				///If dotProduct is 0, secondary is perpendicular to the primary's view
				///if dotProduct is greater than 0, secondary is in front of the primary's view
				const float dot = FVector::DotProduct(primary->GetDirection().GetSafeNormal2D(), (secondaryLocation - primaryLocation).GetSafeNormal2D());
				Global::Log(DATATRIVIAL, LogCondition, "UCondition_RelativePosition", "Evaluate", TEXT("Dot: %f."), dot);
				return dot;
			}
		case EConditional_RelativePosition::Height:
			{
				const float height = secondaryLocation.Z - primaryLocation.Z;
				Global::Log(DATATRIVIAL, LogCondition, "UCondition_RelativePosition", "Evaluate", TEXT("Height: %f."), height);
				return height;
			}
		default:
			return 0.0f;
	}
}
