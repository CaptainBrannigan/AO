// Copyright Jordan Cain, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Purpose/Condition.h"
#include "Data/DataTags.h"
#include "Condition_Velocity.generated.h"

UENUM()
enum class EConditional_Velocity : uint8
{
	None
	, SpeedWithinRange
};

UCLASS(CollapseCategories)
/// Speed of a vector chunk in m/s, normally the subject's Data.Velocity (FActorVelocityChunk)
/// Previously read GetVelocity() from the actor on the evaluation thread, now reads the velocity copied at snapshot
class AO_API UCondition_Velocity : public UCondition
{
	GENERATED_BODY()
public:

	UCondition_Velocity()
	{
		bHasRange = true;
		bHasSecondSubject = false;
		primaryKey = TAG_Data_Velocity;
	}

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (DisplayPriority = "4"))
	///An Enum representing how the user wishes the two data types to be compared
	EConditional_Velocity conditional = EConditional_Velocity::None;

protected:

	float Evaluate(const FSubjectDataView& data) const override;
};
