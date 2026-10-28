// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Purpose/Condition.h"
#include "Purpose/DataChunks/ActorRole.h"
#include "Condition_Role.generated.h"

UENUM()
enum class EConditional_Role : uint8
{
	None,
	IsA,/// An actor is a role
	IsNotA/// An actor is not a role
};

UCLASS(CollapseCategories)
/// Scores how many of the roles (or the secondary subject's roles) the primary subject has
///Design: GAS GameplayTags Role TagQuery; With roles as ASC owned tags this becomes an FGameplayTagQuery match (see FRoleChunk)
class AO_API UCondition_Role : public UCondition
{
	GENERATED_BODY()
public:

	UCondition_Role()
	{
		primaryKey = TAG_Data_Role;
	}

	UPROPERTY(EditAnywhere, Category = "Condition Data", DisplayName = "Optional Value", meta = (DisplayPriority = "5"))
	///Specify this value in place of a secondary subject + value
	///Primary data value of primary subject will be compared against this
	TArray<EActorRole> roles;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (DisplayPriority = "4"))
	///An Enum representing how the user wishes the two data types to be compared
	EConditional_Role conditional = EConditional_Role::None;

protected:

	float Evaluate(const FSubjectDataView& data) const override;
};
