// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Purpose/Condition.h"
#include "Condition_Float.generated.h"

UENUM()
enum class EConditional_Float : uint8
{
	None
	, ScoreValueAlongCurve/// Rather than a direct comparison, score the value on the provided curve, wil return the value as the score of the condition so that it will be evaluated against the condition's response curve in the evaluation thread
};

UCLASS(CollapseCategories)
/// Scores any float chunk (FFloatChunk or derived, attribute-backed chunks included) along the condition's range and curve
/// Ported from Warzone: primaryDataType (TSubclassOf<UFloatChunk>) is now primaryKey
class AO_API UCondition_Float : public UCondition
{
	GENERATED_BODY()
public:

	UCondition_Float()
	{
		bHasRange = true;///  Allows us to use min/max values
	}

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (DisplayPriority = "4"))
	///An Enum representing how the user wishes the two data types to be compared
	EConditional_Float conditional = EConditional_Float::ScoreValueAlongCurve;

protected:

	float Evaluate(const FSubjectDataView& data) const override;
};
