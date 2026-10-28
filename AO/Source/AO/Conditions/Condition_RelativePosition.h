// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Purpose/Condition.h"
#include "Condition_RelativePosition.generated.h"

UENUM()
enum class EConditional_RelativePosition : uint8
{
	None
	, Dot /// Rotational position on XY plane: 1 when the secondary is straight ahead of the primary's view, 0 perpendicular, -1 behind
	, Height /// Secondary Z minus primary Z, in uu (positive when the secondary is above)
};

UCLASS(CollapseCategories)
/// <summary>
/// Where the secondary subject is relative to the primary subject's view point
/// Primary reads an FActorViewPointChunk (Data.ViewPoint by default), the secondary any vector chunk (its own view point, location, a task goal...)
/// Ported from Warzone, which read the actors' forward vector and location on the evaluation thread
///
/// Negative values (behind, below) score 0 with the default floor at min 0. Turn off "Score is clamped by Min" and lower min to score them
/// </summary>
class AO_API UCondition_RelativePosition : public UCondition
{
	GENERATED_BODY()
public:

	UCondition_RelativePosition();

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (DisplayPriority = "4"))
	///An Enum representing how the user wishes the two data types to be compared
	EConditional_RelativePosition conditional = EConditional_RelativePosition::None;

protected:

	float Evaluate(const FSubjectDataView& data) const override;
};
