// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "Data/Data.h"
#include "Data/DataTags.h"
#include "Data/GlobalLog.h"
#include "ActorRole.generated.h"

UENUM(BlueprintType)//"BlueprintType" is essential to include
///Subjects are a way linking an identifier to a DataMap object for use with ContextData
enum class EActorRole : uint8
{
	None UMETA(DisplayName = "None")///Default to ensure data is set with HasData()
	, Player UMETA(DisplayName = "Player")
	, AI UMETA(DisplayName = "AI")
	, Companion UMETA(DisplayName = "Player Companion")/// AI belonging to a Player
	, Hero UMETA(DisplayName = "Hero")/// Type of actor who autonomously executes logic
	, Thrall UMETA(DisplayName = "Thrall")/// Type of actor unintended for independent logic processing
};

USTRUCT(BlueprintType, meta = (DisplayName = "Actor Role"))
/// <summary>
/// Actor Role is a means of differentiating actors based on an arbitrary "Role" (Data.Role)
/// </summary>
///Design: GAS GameplayTags Role; Roles are a set of labels, which is what an ASC's owned GameplayTags are (Role.AI, Role.Companion)
/// As tags, Condition_Role becomes a tag query against the subject's tags, and roles can be granted/removed by GameplayEffects
/// This would need the subject's owned tags snapshotted into its DataMap (an FContextualTagsChunk resolved from the ASC at snapshot)
struct AO_API FRoleChunk : public FDataChunk
{
	GENERATED_BODY()

	FRoleChunk() : FDataChunk(TAG_Data_Role) {}
	explicit FRoleChunk(EActorRole inRole) : FRoleChunk() { roles.Add(inRole); }

	///Important for use with DataMapInterface HasData() in determining if a DataChunk can be accessed
	bool HasValue() const override
	{
		return roles.Num() > 0;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	TArray<EActorRole> roles;

	const TArray<EActorRole>& Value() const { return roles; }

	void AddToValue(EActorRole inRole) { roles.AddUnique(inRole); }

	FString ValueAsString() const
	{
		FString name = "EActorRole:";

		if (roles.Num() < 1) { name += ":None"; }
		for (const EActorRole role : roles)/// Add each role to the spawners name
		{
			name += ":" + Global::EnumValueOnly<EActorRole>(role);
		}

		return name;
	}

	FString Description() const override
	{
		return FDataChunk::Description() + ValueAsString();
	}
};
