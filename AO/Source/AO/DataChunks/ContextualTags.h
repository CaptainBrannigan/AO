// Copyright Jordan Cain, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/Data.h"
#include "Data/DataMapInterface.h"
#include "Data/DataTags.h"
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

USTRUCT(BlueprintType, meta = (DisplayName = "Contextual Tags"))
/// <summary>
/// A set of gameplay tags describing a subject or context
/// </summary>
///Design: GAS GameplayTags OwnedTags; When the subject has an ASC, its owned tags (GetOwnedGameplayTags) are the same idea and are already replicated
/// A chunk that copies the ASC's owned tags at snapshot (ResolveForSnapshot, like FAttributeChunk) would let Condition_ContextualTags query GAS state directly
struct AO_API FContextualTagsChunk : public FDataChunk
{
	GENERATED_BODY()

	FContextualTagsChunk() {}
	explicit FContextualTagsChunk(FGameplayTag inKey) : FDataChunk(inKey) {}

	///Important for use with DataMapInterface HasData() in determining if a DataChunk can be accessed
	bool HasValue() const override
	{
		return true;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	FGameplayTagContainer tags;

	const FGameplayTagContainer& Value() const { return tags; };

	void AddToValue(FGameplayTag inTag)
	{
		if (!tags.HasTagExact(inTag))
		{
			tags.AddTag(inTag);
		}
	}

	void RemoveFromValue(FGameplayTag inTag)
	{
		if (tags.HasTagExact(inTag))
		{
			tags.RemoveTag(inTag);
		}
	}

	bool CopyValueFrom(const FInstancedStruct& source) override
	{
		if (const FContextualTagsChunk* tagSource = source.GetPtr<FContextualTagsChunk>())
		{
			tags = tagSource->tags;
			return true;
		}
		return false;
	}

	FString Description() const override
	{
		return FDataChunk::Description() + tags.ToStringSimple();
	}
};

USTRUCT(BlueprintType, meta = (DisplayName = "Behavior Tags"))
/// Tags describing the current behavior (Data.BehaviorTags), added to the context data by UAI_SequentialActions
/// (Was UBehaviorTags in AI_SequentialActions.h)
struct AO_API FBehaviorTagsChunk : public FContextualTagsChunk
{
	GENERATED_BODY()

	FBehaviorTagsChunk() : FContextualTagsChunk(TAG_Data_BehaviorTags) {}
};

USTRUCT(BlueprintType, meta = (DisplayName = "Contextual Tags"))
/// Adds or removes a tag. Creates an FContextualTagsChunk under the key if the subject doesn't have one
struct AO_API FContextualTagsModifier : public FDataModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Data Adjustment")
	EBehaviorTagModifier modifier = EBehaviorTagModifier::None;

	UPROPERTY(EditAnywhere, Category = "Data Adjustment")
	/// Tag we are either adding or removing
	FGameplayTag tagForAdjustment;

	bool Apply(FDataMap& map, FGameplayTag key) const override;

	FString Description() const override
	{
		return Global::EnumValueOnly<EBehaviorTagModifier>(modifier) + TEXT(" ") + tagForAdjustment.ToString();
	}
};
