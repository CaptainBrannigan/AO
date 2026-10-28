// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Purpose/Condition.h"
#include "GameFramework/Actor.h"
#include "Condition_ActorClassIsA.generated.h"

UENUM()
enum class EConditional_ClassComparison : uint8
{
	None
	, Is/// The subject is exact class
	, IsA/// The class derives from
	, IsNot/// The subjects not the same class
	, IsNotA/// The subject does not derive from
};

UCLASS(CollapseCategories)
/// <summary>
/// Compares the class of an actor reference chunk (Data.Actor by default) against a class set on the condition
/// The class is copied by FActorReferenceChunk::ResolveForSnapshot, so the actor is never touched on the evaluation thread
/// </summary>
///Design: GAS GameplayTags Classification; Class checks tie purposes to the C++/BP hierarchy, an identity tag (Actor.Type.Vehicle) granted to the ASC
/// or kept on FRoleChunk can classify actors across unrelated classes and be checked with the existing tag conditions
class AO_API UCondition_ActorClassIsA : public UCondition
{
	GENERATED_BODY()
public:

	UCondition_ActorClassIsA();

	UPROPERTY(EditAnywhere, Category = "Condition Data", DisplayName = "Class To Compare", meta = (DisplayPriority = "5"))
	///Primary data value of primary subject will be compared against this
	TSubclassOf<AActor> classToCompare = nullptr;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (DisplayPriority = "4"))
	///An Enum representing how the user wishes the two data types to be compared
	EConditional_ClassComparison conditional = EConditional_ClassComparison::None;

protected:

	float Evaluate(const FSubjectDataView& data) const override;
};
