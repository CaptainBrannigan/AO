// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Purpose/Condition.h"
#include "VectorVsVector.generated.h"

UENUM()
enum class EConditional_VvV : uint8
{
	None,
	ScoreWithRangeValues/// Distance between the two vectors, scored along the condition's range
};

UCLASS(CollapseCategories)
/// Distance between two vector chunks (FActorLocationChunk, task goals, EQS results...)
/// Ported from Warzone, with UCondition_Vector folded in
class AO_API UVectorVsVector : public UCondition
{
	GENERATED_BODY()
public:

	UVectorVsVector()
	{
		bHasRange = true; ///  Allows us to use min/max values
	}

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (DisplayPriority = "4"))
	EConditional_VvV conditional = EConditional_VvV::ScoreWithRangeValues;

protected:

	float Evaluate(const FSubjectDataView& data) const override;
};
