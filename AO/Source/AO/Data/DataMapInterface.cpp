// Copyright Jordan Cain. All Rights Reserved.


#include "DataMapInterface.h"
#include "Purpose/DataChunks/FloatChunk.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataMapInterface)

bool FDataMapEntry::CopyValueFrom(const FDataMapEntry& source)
{
	if (!source.Chunk.IsValid())
	{
		return false;
	}

	if (!Chunk.IsValid())/// Nothing to keep, take the source as is
	{
		Chunk = source.Chunk;
		return true;
	}

	if (Chunk.GetScriptStruct() == source.Chunk.GetScriptStruct())
	{
		const FGameplayTag key = Key();
		Chunk = source.Chunk;
		GetMutable()->Key = key;
		return true;
	}

	return GetMutable()->CopyValueFrom(source.Chunk);
}

bool FDataMap::Set(const FInstancedStruct& chunk)
{
	if (!chunk.IsValid() || !chunk.GetScriptStruct()->IsChildOf(FDataChunk::StaticStruct()))
	{
		Global::LogError(LogData, "FDataMap", "Set", TEXT("Attempting to add %s, which is not a DataChunk!"), chunk.GetScriptStruct() ? *chunk.GetScriptStruct()->GetName() : TEXT("an empty struct"));
		return false;
	}

	const FGameplayTag key = chunk.Get<FDataChunk>().Key;
	if (!key.IsValid())
	{
		Global::LogError(LogData, "FDataMap", "Set", TEXT("Attempting to add a %s with no key!"), *chunk.GetScriptStruct()->GetName());
		return false;
	}

	if (FDataMapEntry* entry = FindEntry(key))
	{
		entry->Chunk = chunk;
	}
	else
	{
		Entries.Add(FDataMapEntry(chunk));
	}
	return true;
}

FString FDataMap::Description() const
{
	FString description;
	for (const FDataMapEntry& entry : Entries)
	{
		const FDataChunk* chunk = entry.Get();
		description += FString::Printf(TEXT("[%s] %s; "), entry.Chunk.GetScriptStruct() ? *entry.Chunk.GetScriptStruct()->GetName() : TEXT("Invalid"), chunk ? *chunk->Description() : TEXT(""));
	}
	return description;
}

bool UDataMapGlobals::FindDataChunk_BP(const FDataMap& dataMap, FGameplayTag key, FInstancedStruct& outChunk)
{
	if (const FDataMapEntry* entry = dataMap.FindEntry(key))
	{
		outChunk = entry->Chunk;
		return true;
	}

	outChunk.Reset();
	return false;
}

bool UDataMapGlobals::GetFloatData_BP(const FDataMap& dataMap, FGameplayTag key, float& outValue)
{
	if (const FFloatChunk* chunk = dataMap.Find<FFloatChunk>(key))
	{
		outValue = chunk->GetValue();
		return true;
	}

	outValue = 0.0f;
	return false;
}
