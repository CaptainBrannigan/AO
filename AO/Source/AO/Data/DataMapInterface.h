// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data.h"
#include "UObject/Interface.h"
#include "GlobalLog.h"
#include "DataMapInterface.generated.h"

USTRUCT(BlueprintType, Blueprintable)
struct FDataMapEntry
{
	GENERATED_BODY();
public:

	FDataMapEntry() {}
	FDataMapEntry(TObjectPtr<UDataChunk> inChunk)
		: Chunk(inChunk)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, meta = (ShowInnerProperties))
	TObjectPtr<UDataChunk> Chunk = nullptr;

	FORCEINLINE bool operator ==(const FDataMapEntry& other) const
	{
		return Chunk.GetClass() == other.Chunk.GetClass();
	}

	FORCEINLINE bool operator ==(const TSubclassOf<UDataChunk> inChunkType) const
	{
		return Chunk.GetClass() == inChunkType;
	}
};
FORCEINLINE uint32 GetTypeHash(const FDataMapEntry& b)
{
	return FCrc::MemCrc32(&b, sizeof(FDataMapEntry));
}

/** Static class with useful gameplay utility functions that can be called from both Blueprint and C++ */
UCLASS()
class AO_API UDataMapGlobals : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	template<class DataChunkClass>
	static TObjectPtr<DataChunkClass> DataChunk(const TArray<FDataMapEntry>& dataMap)/// Since returned dataMap is replicated, any changes done by client are overwritten by Server properties
	{
		if (dataMap.Contains(DataChunkClass::StaticClass()))
		{
			return Cast<DataChunkClass>(dataMap.FindByKey(DataChunkClass::StaticClass())->Chunk);
		}

		return nullptr;
	}

	static TObjectPtr<UDataChunk> DataChunk(const TArray<FDataMapEntry>& dataMap, TSubclassOf<UDataChunk> inType)/// Since returned dataMap is replicated, any changes done by client are overwritten by Server properties
	{
		if (dataMap.Contains(inType))
		{
			return dataMap.FindByKey(inType)->Chunk;
		}

		return nullptr;
	}
	
	UFUNCTION(BlueprintCallable)
	static UDataChunk* DataChunk_BP(const TArray<FDataMapEntry>& dataMap, TSubclassOf<UDataChunk> inType);/// Since returned dataMap is replicated, any changes done by client are overwritten by Server properties

	static TArray<FDataMapEntry> CopyOfDataChunks(const TArray<FDataMapEntry>& dataMap, UObject& outer)
	{
		TArray<FDataMapEntry> data;
		for (const FDataMapEntry& entry : dataMap)
		{
			TObjectPtr<UDataChunk> chunk = DuplicateObject<UDataChunk>(entry.Chunk, &outer);

			if (!chunk) { continue; }

			data.Add(FDataMapEntry(chunk));
		}

		return data;
	}

	///Returns true if a data type is found and that data type has a valid value
	static bool HasData(const TArray<FDataMapEntry>& dataMap, TSubclassOf<UDataChunk> inType)
	{
		Global::Log(DATATRIVIAL, LogData, "IDataMapInterface", "HasData", TEXT("Seeking %s; %s. %s."),
			*inType->GetName()
			, dataMap.Contains(inType) ? TEXT("Data is contained") : TEXT("Data is Not Contained")
			, IsValid(UDataMapGlobals::DataChunk(dataMap, inType)) ? TEXT("DataChunk is valid") : TEXT("DataChunk is not valid")
		);

		return dataMap.Contains(inType) && IsValid(UDataMapGlobals::DataChunk(dataMap, inType)) ? UDataMapGlobals::DataChunk(dataMap, inType)->HasValue() : false;
	}
	
	UFUNCTION(BlueprintCallable)
	///Returns true if a data type is found and that data type has a valid value
	static bool HasData_BP(const TArray<FDataMapEntry>& dataMap, TSubclassOf<UDataChunk> inType);

	///@Param: overwriteValue: If datachunk already exists, replace with incoming DataChunk
	static TObjectPtr<UDataChunk> AddData(TArray<FDataMapEntry>& dataMap, UDataChunk& inData, bool overwriteValue = true)
	{
		if (overwriteValue && dataMap.Contains(inData.GetClass()))
		{
			dataMap.FindByKey(inData.GetClass())->Chunk = &inData;
			return &inData;
		}
		else
		{
			//			UE_LOG(LogTemp, Log, TEXT("Attempting to add %s. IDataMapInterface::AddData()"), *inData.GetClass()->GetName());
			dataMap.Add(FDataMapEntry(&inData));
			return &inData;
		}
	}

	template<class DataChunkClass>
	///@Param: overwriteValue: If datachunk already exists, replace with incoming DataChunk
	static DataChunkClass* AddData(TArray<FDataMapEntry>& dataMap, UDataChunk& inData, bool overwriteValue = true)
	{
		if (overwriteValue && dataMap.Contains(inData.GetClass()))
		{
			dataMap.Emplace(inData.GetClass(), &inData);
			return Cast<DataChunkClass>(&inData);
		}
		else
		{
			dataMap.Add(FDataMapEntry(inData.GetClass(), &inData));
			return Cast<DataChunkClass>(&inData);
		}
	}

	///@Param inDataMap: Each existing DataChunk will be added to this DataMapInterface
	///@Param overwriteValue: If datachunk already exists, replace with incoming DataChunk
	static void AppendData(TArray<FDataMapEntry>& dataMapToAppendTo, const TArray<FDataMapEntry>& sourceDataMap, bool overwriteValue = true)
	{
		dataMapToAppendTo.Append(sourceDataMap);
		//for (const FDataMapEntry& data : sourceDataMap)
		//{
		//	if (overwriteValue && dataMapToAppendTo.Contains(data))
		//	{
		//		dataMapToAppendTo.Emplace(data);
		//	}
		//	else
		//	{
		//		//			UE_LOG(LogTemp, Log, TEXT("Attempting to add %s. IDataMapInterface::AddData()"), *inData.GetClass()->GetName());
		//		dataMapToAppendTo.Emplace(data);
		//	}
		//}
	}

	///Overwritevalue: If datachunk already exists, replace with indatachunk
	static bool RemoveData(TArray<FDataMapEntry>& dataMap, TSubclassOf<UDataChunk> inClass)
	{
		//		//Global::Log(EHierarchicalCalltraceVerbosity::DEBUG, *this, "RemoveData", TEXT("Attempting to add %s."), *inData->GetName());
		if (dataMap.Contains(inClass))
		{
			///Debug: DataChunk DataManagement DataRemoval; Will datachunk be GC'ed?
			dataMap.Remove(*dataMap.FindByKey(inClass));
			//dataMap.Remove(inClass);
			return true;///DataChunk successfully removed
		}
		else
		{
			return true;///If the class isn't contained then we don't need to worry
		}
	}
};

UINTERFACE(NotBlueprintable, meta = (CannotImplementInterfaceInBlueprint))
class AO_API UDataMapInterface : public UInterface
{
	GENERATED_BODY()

};

/// <summary>
///DataMapInteface is a means to enforce a data structure across any objects that can contain UDataChunks
///It also serves to abstract any objects implementing the interface to a base object pointer with only functionality critical to the data system
/// </summary>
class AO_API IDataMapInterface
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintCallable)
	///Allows evaluations to retrieve chunks of data from any type of object implementing interface
	virtual const TArray<FDataMapEntry>& DataMap() = 0;
	virtual TArray<FDataMapEntry> DataMapCopy() = 0;

	TArray<FDataMapEntry> CopyOfDataChunks(UObject& outer)
	{
		return UDataMapGlobals::CopyOfDataChunks(DataMap(), outer);
	}

	template<class DataChunkClass>
	TObjectPtr<DataChunkClass> DataChunk()/// Since returned DataMap() is replicated, any changes done by client are overwritten by Server properties
	{
		//return Cast<DataChunkClass>(DataMap().FindByKey(DataChunkClass::StaticClass())->Chunk);
		return UDataMapGlobals::DataChunk<DataChunkClass>(DataMap());
	}

	TObjectPtr<UDataChunk> DataChunk(TSubclassOf<UDataChunk> inType)/// Since returned DataMap() is replicated, any changes done by client are overwritten by Server properties
	{
		//return *DataMap().Find(inType);
		//return DataMap().FindByKey(inType)->Chunk;
		return UDataMapGlobals::DataChunk(DataMap(), inType);
	}

	///Returns true if a data type is found and that data type has a valid value
	bool HasData(TSubclassOf<UDataChunk> inType)
	{
		/*Global::Log(DATATRIVIAL, MANAGEMENT, "IDataMapInterface", "HasData", TEXT("Seeking %s, %s. %s. %s."),
			*inType->GetName(),
			IsValid(DataMap().FindRef(inType)) ? *FString::Printf(TEXT("Ref Data is %s"), *DataMap().FindRef(inType)->GetName()) : TEXT("Not Found"),
			IsValid(DataMap().Find(inType)) ? TEXT("Find Data is found") : TEXT("Find Data is Not Found"),
			DataMap().Contains(inType) ? TEXT("Data is contained") : TEXT("Data is Not Contained")
		);*/

		//return false;
		//return DataMap().Contains(inType) ? DataChunk(inType)->HasValue() : false;
		return UDataMapGlobals::HasData(DataMap(), inType);
	}

	virtual FString Description() { return ""; }

	/// Changing of data has to be called on Server otherwise changes won't be replicated
	/// These methods all require UFUNCTION(Server)
	virtual void AddData(UDataChunk* inData, bool overwriteValue = true) = 0;
	virtual void AppendData(const TArray<FDataMapEntry>& inDataMap, bool overwriteValue = true) = 0;
	virtual void RemoveData(TSubclassOf<UDataChunk> inClass) = 0;

protected:

	///@Param: overwriteValue: If datachunk already exists, replace with incoming DataChunk
	TObjectPtr<UDataChunk> AddDataLocal(UDataChunk& inData, bool overwriteValue = true)
	{
//		if (overwriteValue && DataMap().Contains(inData.GetClass()))
//		{
//			DataMapInternal().Emplace(inData.GetClass(), &inData);
//			return &inData;
//		}
//		else
//		{
////			UE_LOG(LogTemp, Log, TEXT("Attempting to add %s. IDataMapInterface::AddData()"), *inData.GetClass()->GetName());
//			DataMapInternal().Add(FDataMapEntry(&inData));
//			return &inData;
//		}

		//return nullptr;
		return UDataMapGlobals::AddData(DataMapInternal(), inData, overwriteValue);
	}

	template<class DataChunkClass>
	///@Param: overwriteValue: If datachunk already exists, replace with incoming DataChunk
	DataChunkClass* AddDataLocal(UDataChunk& inData, bool overwriteValue = true)
	{
		/*if (overwriteValue && DataMap().Contains(inData.GetClass()))
		{
			DataMapInternal().Emplace(inData.GetClass(), &inData);
			return Cast<DataChunkClass>(&inData);
		}
		else
		{
			DataMapInternal().Add(FDataMapEntry(inData.GetClass(), &inData));
			return Cast<DataChunkClass>(&inData);
		}

		return nullptr;*/
		return UDataMapGlobals::AddData<DataChunkClass>(DataMapInternal(), inData, overwriteValue);
	}

	///@Param inDataMap: Each existing DataChunk will be added to this DataMapInterface
	///@Param overwriteValue: If datachunk already exists, replace with incoming DataChunk
	void AppendDataLocal(const TArray<FDataMapEntry>& inDataMap, bool overwriteValue = true)
	{
		//DataMap().Append(inDataMap->DataMap());
	//	for (const FDataMapEntry& data : inDataMap)
	//	{
	//		if (overwriteValue && DataMap().Contains(data))
	//		{
	//			DataMapInternal().Emplace(data);
	//		}
	//		else
	//		{
	////			UE_LOG(LogTemp, Log, TEXT("Attempting to add %s. IDataMapInterface::AddData()"), *inData.GetClass()->GetName());
	//			DataMapInternal().Emplace(data);
	//		}
	//	}
		UDataMapGlobals::AppendData(DataMapInternal(), inDataMap, overwriteValue);
	}

	///Overwritevalue: If datachunk already exists, replace with indatachunk
	bool RemoveDataLocal(TSubclassOf<UDataChunk> inClass)
	{
////		//Global::Log(EHierarchicalCalltraceVerbosity::DEBUG, *this, "RemoveData", TEXT("Attempting to add %s."), *inData->GetName());
//		if (DataMap().Contains(inClass))
//		{
//			///Debug: DataChunk DataManagement DataRemoval; Will datachunk be GC'ed?
//			DataMapInternal().Remove(*DataMap().FindByKey(inClass));
//			//DataMap().Remove(inClass);
//			return true;///DataChunk successfully removed
//		}
//		else
//		{
//			return true;///If the class isn't contained then we don't need to worry
//		}
//
//		return false;
		return UDataMapGlobals::RemoveData(DataMapInternal(), inClass);
	}



private:

	/// Force implementers to provide an editable datamap
	/// The only way to the change the DataMap is through the Server RPCs
	virtual TArray<FDataMapEntry>& DataMapInternal() = 0;
};
