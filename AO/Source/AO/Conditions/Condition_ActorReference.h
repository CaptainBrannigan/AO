// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Purpose/Condition.h"
#include "Condition_ActorReference.generated.h"

UENUM()
enum class EConditional_ActorReference : uint8
{
	None
	, Is/// The subjects are the same actor
	, IsNot/// The subjects are different actors
};

UCLASS(CollapseCategories)
/// Whether two actor reference chunks point at the same actor. Compares identity only, so it never touches the actors
class AO_API UCondition_ActorReference : public UCondition
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (DisplayPriority = "4"))
	///An Enum representing how the user wishes the two data types to be compared
	EConditional_ActorReference conditional = EConditional_ActorReference::None;

protected:

	float Evaluate(const FSubjectDataView& data) const override;
};
