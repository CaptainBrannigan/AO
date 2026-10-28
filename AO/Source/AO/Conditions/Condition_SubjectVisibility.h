// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Purpose/Condition.h"
#include "Engine/EngineTypes.h"
#include "Condition_SubjectVisibility.generated.h"

UENUM()
enum class EConditional_Visibility : uint8
{
	None
	, IsVisible///
	, IsNotVisible///
};

UCLASS(CollapseCategories)
/// <summary>
/// Whether there is a clear line between the primary and secondary subjects' view points (Data.ViewPoint by default)
/// Ported from Warzone, where it line traced on the evaluation thread
///
/// The evaluation is commented out and scores 0 for now: a world trace isn't safe off the game thread,
/// so the result has to be gathered on the game thread into the snapshot before this can read it (see Evaluate)
/// </summary>
class AO_API UCondition_SubjectVisibility : public UCondition
{
	GENERATED_BODY()
public:

	UCondition_SubjectVisibility();

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (DisplayPriority = "4"))
	///An Enum representing how the user wishes the two data types to be compared
	EConditional_Visibility conditional = EConditional_Visibility::None;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (DisplayPriority = "4"))
	TEnumAsByte<ECollisionChannel> TraceChannelProperty = ECC_Pawn;

protected:

	float Evaluate(const FSubjectDataView& data) const override;
};
