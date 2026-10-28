// Copyright Jordan Cain. All Rights Reserved.


#include "ContextualTags.h"
#include "NativeGameplayTags.h"
#include "GameplayTagAssetInterface.h"
#include "Data/GlobalLog.h"

bool FContextualTagsModifier::Apply(FDataMap& map, FGameplayTag key) const
{
	if (modifier == EBehaviorTagModifier::None || !tagForAdjustment.IsValid())
	{
		Global::Log(DATADEBUG, LogData, "FContextualTagsModifier", "Apply", TEXT("Modifier for %s has no modification or tag set."), *key.ToString());
		return false;
	}

	FContextualTagsChunk* chunk = map.FindOrAdd<FContextualTagsChunk>(key);
	if (!chunk)
	{
		return false;
	}

	switch (modifier)
	{
		case EBehaviorTagModifier::AddTag:
			chunk->AddToValue(tagForAdjustment);
			break;
		case EBehaviorTagModifier::RemoveTag:
			chunk->RemoveFromValue(tagForAdjustment);
			break;
		default:
			return false;
	}

	return true;
}
