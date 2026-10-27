// Copyright Jordan Cain. All Rights Reserved.


#include "ContextualTags.h"
#include "NativeGameplayTags.h"
#include "GameplayTagAssetInterface.h"
#include "Data/GlobalLog.h"

//UE_DEFINE_GAMEPLAY_TAG(TAG_InputTag_Primary_Grip, "InputTag.Primary.Grip");

TObjectPtr<UDataChunk> UContextualTags::AdjustData(const UDataChunk& inModifier)
{
	if (!inModifier.IsA<UContextualTags>())
	{
		Global::Log(DATADEBUG, LogData, "UContextualTags", "AdjustData", TEXT("Provided a non-ContextualTags modifier chunk."));
		return this;
	}

	EBehaviorTagModifier dataModifier = Cast<UContextualTags>(&inModifier)->modifier;

	switch (dataModifier)
	{
		case EBehaviorTagModifier::AddTag:
			tags.AddTag(Cast<UContextualTags>(&inModifier)->tagForAdjustment);
			break;
		case EBehaviorTagModifier::RemoveTag:
			tags.RemoveTag(Cast<UContextualTags>(&inModifier)->tagForAdjustment);
			break;
	}

	return this;
}
