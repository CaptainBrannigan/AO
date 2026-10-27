// Copyright Jordan Cain. All Rights Reserved.


#include "DataMapInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataMapInterface)

UDataChunk* UDataMapGlobals::DataChunk_BP(const TArray<FDataMapEntry>& dataMap, TSubclassOf<UDataChunk> inType)
{
	if (dataMap.Contains(inType))
	{
		return dataMap.FindByKey(inType)->Chunk;
	}

	return nullptr;
}

bool UDataMapGlobals::HasData_BP(const TArray<FDataMapEntry>& dataMap, TSubclassOf<UDataChunk> inType)
{
	/*Global::Log(DATATRIVIAL, MANAGEMENT, "IDataMapInterface", "HasData", TEXT("Seeking %s, %s. %s. %s."),
		*inType->GetName(),
		IsValid(dataMap.FindRef(inType)) ? *FString::Printf(TEXT("Ref Data is %s"), *dataMap.FindRef(inType)->GetName()) : TEXT("Not Found"),
		IsValid(dataMap.Find(inType)) ? TEXT("Find Data is found") : TEXT("Find Data is Not Found"),
		dataMap.Contains(inType) ? TEXT("Data is contained") : TEXT("Data is Not Contained")
	);*/

	//return false;
	return dataMap.Contains(inType) ? UDataMapGlobals::DataChunk(dataMap, inType)->HasValue() : false;
}
