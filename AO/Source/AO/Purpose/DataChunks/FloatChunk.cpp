// Copyright Jordan Cain. All Rights Reserved.


#include "FloatChunk.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

void FAttributeChunk::InitializeForOwner(AActor* inAvatar, UAbilitySystemComponent* inAbilitySystem)
{
	abilitySystem = inAbilitySystem ? inAbilitySystem : UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(inAvatar);
	if (!abilitySystem.IsValid())
	{
		Global::LogError(LogData, "FAttributeChunk", "InitializeForOwner", TEXT("%s has no AbilitySystemComponent for attribute %s!")
			, inAvatar ? *inAvatar->GetName() : TEXT("Invalid owner")
			, *attribute.GetName()
		);
	}
}

void FAttributeChunk::ResolveForSnapshot()
{
	if (UAbilitySystemComponent* asc = abilitySystem.Get())
	{
		value = asc->GetNumericAttribute(attribute);
	}
}

float FAttributeChunk::GetValue() const
{
	if (IsInGameThread())
	{
		if (const UAbilitySystemComponent* asc = abilitySystem.Get())
		{
			return asc->GetNumericAttribute(attribute);
		}
	}
	return value;
}

bool FFloatModifier::Apply(FDataMap& map, FGameplayTag key) const
{
	FFloatChunk* chunk = map.FindOrAdd<FFloatChunk>(key);
	if (!chunk)
	{
		return false;
	}

	if (FAttributeChunk* attributeChunk = map.FindMutable<FAttributeChunk>(key))
	{
		if (UAbilitySystemComponent* asc = attributeChunk->abilitySystem.Get())
		{
			switch (dataModifier)
			{
				case EDataModifier::Increase: asc->ApplyModToAttribute(attributeChunk->attribute, EGameplayModOp::Additive, value); break;
				case EDataModifier::Decrease: asc->ApplyModToAttribute(attributeChunk->attribute, EGameplayModOp::Additive, -value); break;
				case EDataModifier::SetValue: asc->ApplyModToAttribute(attributeChunk->attribute, EGameplayModOp::Override, value); break;
				default: return false;
			}
			Global::Log(DATATRIVIAL, PURPOSE, "FFloatModifier", "Apply", TEXT("Adjusted attribute %s by %s. Value: %f."), *attributeChunk->attribute.GetName(), *Global::EnumValueOnly<EDataModifier>(dataModifier), attributeChunk->GetValue());
			return true;
		}
	}

	switch (dataModifier)
	{
		case EDataModifier::Increase: chunk->value += value; break;
		case EDataModifier::Decrease: chunk->value -= value; break;
		case EDataModifier::SetValue: chunk->value = value; break;
		default:
			return false;
	}
	Global::Log(DATATRIVIAL, PURPOSE, "FFloatModifier", "Apply", TEXT("Adjusted %s by %s. Value: %f."), *key.ToString(), *Global::EnumValueOnly<EDataModifier>(dataModifier), chunk->GetValue());

	return true;
}
