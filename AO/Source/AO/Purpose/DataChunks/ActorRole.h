// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "Data/Data.h"
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

USTRUCT(BlueprintType)
struct FRoleEntry
{
	GENERATED_BODY()

	FRoleEntry(){}

	FRoleEntry(EActorRole inRole) 
		: role(inRole)
	{}

	UPROPERTY(EditAnywhere)
	EActorRole role = EActorRole::None;

	FORCEINLINE bool operator ==(const FRoleEntry& other) const
	{
		return role == other.role;
	}
};
FORCEINLINE uint32 GetTypeHash(const FRoleEntry& b)
{
	return FCrc::MemCrc32(&b, sizeof(FRoleEntry));
}

UCLASS()
/// <summary>
/// Actor Role is a means of differentiating actors based on an arbitrary "Role"
/// The class itself is actually a definition of a "Role"
/// </summary>
class AO_API UActorRole : public UDataChunk
{
	GENERATED_BODY()
public:

	UActorRole() {}

	///Overload this to require new object arguments
	UActorRole& Initialize() { return *this; }

	///Important for use with DataMapInterface HasData() in determining if a DataChunk can be accessed
	///Override this with determination of whether a value has been set for this chunk or not
	bool HasValue() final
	{
		return Value().Num() > 0;
	}

	UPROPERTY(EditAnywhere)
	TArray<FRoleEntry> roles;

	///Value is used solely for a consistent name for getting a datachunk's value
	///Because derivatives hold different data types, from primitive to custom, it cannot be virtual
	const TArray<FRoleEntry>& Value() const { return roles; }

	void AddToValue(FRoleEntry inEntry) { roles.Add(inEntry); }

	const FString ValueAsString() const
	{
		FString name = "EActorRole:";

		if (roles.Num() < 1) { name += ":None"; }
		for (const FRoleEntry& role : roles)/// Add each role to the spawners name
		{
			name += ":" + Global::EnumValueOnly<EActorRole>(role.role);
		}

		return name;
	}

	///Override this to provide more detailed information about a data chunk
	///Call Super::Description() to include string about EDataType
	FString Description() const final
	{
		return Super::Description() + FString::Printf(TEXT("Role Num: %d"), roles.Num());
	}

	///If implicit conversion is desired
	//operator insertdatatype() { return instertvaluemember; }
};
