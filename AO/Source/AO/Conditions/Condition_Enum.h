// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Purpose/Condition.h"
#include "Condition_Enum.generated.h"

UENUM()
enum class EConditional_Enum : uint8
{
	None
	, Equal
	, NotEqual
};

UCLASS(CollapseCategories)
/// Compares any enum chunk (FEnumChunk: archetype, race...) against another, or against value when there is no secondary data
/// Ported from Warzone, where it was abstract with a per-enum Value()
class AO_API UCondition_Enum : public UCondition
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = "Condition Data", DisplayName = "Optional Value", meta = (DisplayPriority = "5"))
	///Specify this value in place of a secondary subject + value
	///Primary data value of primary subject will be compared against this
	uint8 value = 0;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (DisplayPriority = "4"))
	///An Enum representing how the user wishes the two data types to be compared
	EConditional_Enum conditional = EConditional_Enum::None;

protected:

	float Evaluate(const FSubjectDataView& data) const override;
};
