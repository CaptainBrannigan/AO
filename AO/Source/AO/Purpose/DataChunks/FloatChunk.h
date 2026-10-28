// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "Data/Data.h"
#include "Data/DataMapInterface.h"
#include "AttributeSet.h"
#include "Data/GlobalLog.h"
#include "FloatChunk.generated.h"

class UAbilitySystemComponent;

UENUM(BlueprintType)
enum class EDataModifier : uint8
{
	None/// Default to ensure a value is actually selected or not
	, Increase
	, Decrease
	, SetValue
};

USTRUCT(BlueprintType, meta = (DisplayName = "Float"))
/// <summary>
/// A base type of data chunk for float, with base structure for conditions
/// Specific uses for float values are represented by their Key (Data.Morale), or by a derived struct when they need their own logic
/// </summary>
struct AO_API FFloatChunk : public FDataChunk
{
	GENERATED_BODY()

	FFloatChunk() {}
	FFloatChunk(FGameplayTag inKey, float inValue = 0.0f) : FDataChunk(inKey), value(inValue) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	///
	float value = 0;

	/// Virtual so derived chunks can compute their value. Conditions call this, never value directly
	/// Overrides must be safe on the evaluation thread
	virtual float GetValue() const { return value; }

	///@return float; value of chunk updated with inValue
	float AddToValue(float inValue) { return value += inValue; }

	///Important for use with DataMapInterface HasData() in determining if a DataChunk can be accessed
	bool HasValue() const override
	{
		return true;/// Value can never be a nullptr, so always return true to allow evaluation
	}

	/// Any float chunk can be copied into any other, regardless of name
	bool CopyValueFrom(const FInstancedStruct& source) override
	{
		if (const FFloatChunk* floatSource = source.GetPtr<FFloatChunk>())
		{
			value = floatSource->GetValue();
			return true;
		}
		return false;
	}

	FString Description() const override
	{
		return FDataChunk::Description() + FString::Printf(TEXT("Value: %f"), GetValue());
	}
};

USTRUCT(BlueprintType, meta = (DisplayName = "Float (Gameplay Attribute)"))
/// <summary>
/// A float chunk whose value is a GameplayAttribute on the owner's AbilitySystemComponent
/// Conditions targeting FFloatChunk read it unchanged. The attribute is read on the game thread when the snapshot is taken
/// </summary>
///Design: GAS Attributes FloatChunk Snapshot; This is the bridge between DataChunks and GameplayAttributes
/// Any float that is a per-actor stat (Morale, Mass, LevelOfAlarm) can live in an AttributeSet and be exposed here by key
/// The attribute then gets GAS's replication, prediction, clamping (PreAttributeChange) and GameplayEffect modification for free
/// Floats that are context-only (path length, task timers) should stay plain FFloatChunks, attributes are per-ASC not per-context
struct AO_API FAttributeChunk : public FFloatChunk
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FGameplayAttribute attribute;

	UPROPERTY(Transient, NotReplicated)
	/// Set by InitializeForOwner. Never dereferenced off the game thread
	TWeakObjectPtr<UAbilitySystemComponent> abilitySystem = nullptr;

	void InitializeForOwner(AActor* inAvatar, UAbilitySystemComponent* inAbilitySystem) override;
	void ResolveForSnapshot() override;

	/// Live attribute on the game thread, otherwise the value copied at snapshot
	float GetValue() const override;

	bool HasValue() const override { return attribute.IsValid(); }

	FString Description() const override
	{
		return FFloatChunk::Description() + TEXT(" Attribute: ") + attribute.GetName();
	}
};

USTRUCT(BlueprintType, meta = (DisplayName = "Float"))
/// Increase, decrease or set a float chunk. Creates an FFloatChunk under the key if the subject doesn't have one
/// When the chunk is an FAttributeChunk, the attribute itself is modified
struct AO_API FFloatModifier : public FDataModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Data Adjustment")
	EDataModifier dataModifier = EDataModifier::None;

	UPROPERTY(EditAnywhere, Category = "Data Adjustment")
	float value = 0.0f;

	///Design: GAS GameplayEffects Modifiers; Modifiers on attribute-backed chunks call ApplyModToAttribute, which skips GameplayEffects
	/// For anything that should stack, expire, be predicted or show up in GAS debugging, the modifier should instead hold a TSubclassOf<UGameplayEffect>
	/// and apply it to the subject's ASC (an FGameplayEffectModifier struct alongside this one)
	bool Apply(FDataMap& map, FGameplayTag key) const override;

	FString Description() const override
	{
		return FString::Printf(TEXT("%s %f"), *Global::EnumValueOnly<EDataModifier>(dataModifier), value);
	}
};
