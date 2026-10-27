// Copyright Jordan Cain, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/Data.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Data/GlobalLog.h"
#include "ContextualTags.generated.h"

UENUM(BlueprintType)
enum class EBehaviorTagModifier : uint8
{
	None/// Default to ensure a value is actually selected or not
	, AddTag
	, RemoveTag
};

/**
 *
 */
UCLASS(Abstract)
class UContextualTags : public UDataChunk
{
	GENERATED_BODY()
public:

	///Overload this to require new object arguments
	UContextualTags& Initialize()
	{
		Global::Log(DATATRIVIAL, PURPOSE, "ULyraBlacboardData", "Initialize", TEXT(""));

		return *this;
	}

	///Important for use with DataMapInterface HasData() in determining if a DataChunk can be accessed
	///Override this with determination of whether a value has been set for this chunk or not
	virtual bool HasValue()
	{
		return true;
	}

	UPROPERTY(EditAnywhere)
		FGameplayTagContainer tags;

	///Value is used solely for a consistent name for getting a datachunk's value
	///Because derivatives hold different data types, from primitive to custom, it cannot be virtual
	const FGameplayTagContainer& Value() const { return tags; };

	/// Override this method so that datachunks specified in a purpose asset may be modified when purpose is selected
	/// @param modificationContext: What purpose selection event will lead to this data modification
	/// @param inModifier: The encapsulated modifier specified on the derived DataChunks class
	TObjectPtr<UDataChunk> AdjustData(const UDataChunk& inModifier) final;

	/// Override in subclasses so they may return the correct enum representing modification logic
	const uint8 DataModifier() final { return static_cast<uint8>(modifier); }

	///Override this to provide more detailed information about a data chunk
	///Call Super::Description() to include string about EDataType
	virtual FString Description() const
	{
		return TEXT("Type: ") + GetClass()->GetName() + TEXT(" ");
	}

	UFUNCTION(BlueprintCallable)
		void AddToValue(FGameplayTag inTag)
	{
		if (!tags.HasTagExact(inTag))
		{
			tags.AddTag(inTag);
		}
	}

	UFUNCTION(BlueprintCallable)
	void RemoveFromVallue(FGameplayTag inTag)
	{
		if (tags.HasTagExact(inTag))
		{
			tags.RemoveTag(inTag);
		}
	}

	///If implicit conversion is desired
	//operator insertdatatype() { return instertvaluemember; }
protected:

	UPROPERTY(EditAnywhere, Category = "Data Adjustment")
		/// Set on a purpose asset in the DataAdjustment array
		/// Not otherwise relevant
		EBehaviorTagModifier modifier = EBehaviorTagModifier::None;

	UPROPERTY(EditAnywhere, Category = "Data Adjustment")
		/// Tag we are either adding or removing
		FGameplayTag tagForAdjustment;
};

