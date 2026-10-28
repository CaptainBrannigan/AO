// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data.h"
#include "UObject/Interface.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GlobalLog.h"
#include "DataMapInterface.generated.h"

#pragma region DataMapEntry

USTRUCT(BlueprintType)
/// One chunk in a DataMap
/// The entry exists so the array element has a struct of its own: it gives the FInstancedStruct its picker restrictions,
/// and it is the type that becomes an FFastArraySerializerItem if DataMaps ever need per-entry delta replication
struct AO_API FDataMapEntry
{
	GENERATED_BODY()
public:

	FDataMapEntry() {}
	explicit FDataMapEntry(const FInstancedStruct& inChunk)
		: Chunk(inChunk)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data", meta = (BaseStruct = "/Script/AO.DataChunk", ExcludeBaseStruct))
	/// Struct-type dropdown with the chunk's fields inline, the same workflow EditInlineNew gave UDataChunk
	FInstancedStruct Chunk;

	const FDataChunk* Get() const { return Chunk.GetPtr<FDataChunk>(); }
	FDataChunk* GetMutable() { return Chunk.GetMutablePtr<FDataChunk>(); }

	template<typename T>
	const T* Get() const { return Chunk.GetPtr<T>(); }

	template<typename T>
	T* GetMutable() { return Chunk.GetMutablePtr<T>(); }

	/// The key lives on the chunk itself, so it can never disagree with the chunk that was selected
	FGameplayTag Key() const
	{
		const FDataChunk* chunk = Get();
		return chunk ? chunk->Key : FGameplayTag();
	}

	/// Copy the value of source into this entry while keeping this entry's key
	/// Same struct type: plain copy. Different types: FDataChunk::CopyValueFrom decides (replaces UDataChunk::CopyDataFrom)
	bool CopyValueFrom(const FDataMapEntry& source);

	FORCEINLINE bool operator ==(const FGameplayTag inKey) const
	{
		return Key() == inKey;
	}
};

#pragma endregion

#pragma region DataMap

USTRUCT(BlueprintType)
/// <summary>
/// A DataMap is a set of DataChunks, at most one per key.
/// It is an array rather than a TMap because TMap properties can't replicate, and maps hold ~5-20 entries where a linear scan beats hashing.
/// Copying a DataMap copies every chunk's data.
/// </summary>
struct AO_API FDataMap
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TArray<FDataMapEntry> Entries;

	int32 Num() const { return Entries.Num(); }

	int32 IndexOf(FGameplayTag key) const
	{
		for (int32 i = 0; i < Entries.Num(); ++i)
		{
			if (Entries[i].Key() == key) { return i; }
		}
		return INDEX_NONE;
	}

	const FDataMapEntry* FindEntry(FGameplayTag key) const
	{
		const int32 index = IndexOf(key);
		return index != INDEX_NONE ? &Entries[index] : nullptr;
	}

	FDataMapEntry* FindEntry(FGameplayTag key)
	{
		const int32 index = IndexOf(key);
		return index != INDEX_NONE ? &Entries[index] : nullptr;
	}

	/// @return nullptr if the key is missing OR the chunk isn't a T (or derived from T)
	template<typename T = FDataChunk>
	const T* Find(FGameplayTag key) const
	{
		const FDataMapEntry* entry = FindEntry(key);
		return entry ? entry->Get<T>() : nullptr;
	}

	template<typename T = FDataChunk>
	T* FindMutable(FGameplayTag key)
	{
		FDataMapEntry* entry = FindEntry(key);
		return entry ? entry->GetMutable<T>() : nullptr;
	}

	///Returns true if a chunk is found for key and that chunk has a valid value
	bool Has(FGameplayTag key) const
	{
		const FDataChunk* chunk = Find(key);
		return chunk && chunk->HasValue();
	}

	/// Add or overwrite the chunk under chunk.Key. The chunk is copied into the map
	template<typename T>
	T* Set(const T& chunk)
	{
		static_assert(TIsDerivedFrom<T, FDataChunk>::IsDerived, "DataMaps only hold FDataChunk types");
		if (!ensureMsgf(chunk.Key.IsValid(), TEXT("Attempting to add a %s with no key to a DataMap!"), *T::StaticStruct()->GetName()))
		{
			return nullptr;
		}

		FDataMapEntry* entry = FindEntry(chunk.Key);
		if (!entry) { entry = &Entries.AddDefaulted_GetRef(); }

		return &entry->Chunk.InitializeAs<T>(chunk);
	}

	/// Untyped version of Set, for chunks that come from an entry or an editor-authored FInstancedStruct
	bool Set(const FInstancedStruct& chunk);

	/// If a chunk exists under key, return it as a T. Otherwise add a default T under key
	/// @return nullptr if a chunk of a different type already owns the key
	template<typename T>
	T* FindOrAdd(FGameplayTag key)
	{
		if (FDataMapEntry* entry = FindEntry(key))
		{
			T* existing = entry->GetMutable<T>();
			if (!existing)
			{
				Global::LogError(LogData, "FDataMap", "FindOrAdd", TEXT("%s already holds a %s, not a %s!")
					, *key.ToString()
					, entry->Chunk.GetScriptStruct() ? *entry->Chunk.GetScriptStruct()->GetName() : TEXT("invalid chunk")
					, *T::StaticStruct()->GetName()
				);
			}
			return existing;
		}

		T chunk;
		chunk.Key = key;
		return Set(chunk);
	}

	bool Remove(FGameplayTag key)
	{
		return Entries.RemoveAll([&key](const FDataMapEntry& entry) { return entry.Key() == key; }) > 0;
	}

	/// Each chunk of other is added to this map, replacing chunks with matching keys
	void Append(const FDataMap& other)
	{
		for (const FDataMapEntry& entry : other.Entries) { Set(entry.Chunk); }
	}

	/// Only chunks whose keys are not already in this map are added
	void AppendMissing(const FDataMap& other)
	{
		for (const FDataMapEntry& entry : other.Entries)
		{
			if (IndexOf(entry.Key()) == INDEX_NONE) { Set(entry.Chunk); }
		}
	}

	/// Game thread only. A copy of this map with every chunk's world-derived value resolved (FDataChunk::ResolveForSnapshot)
	/// The copy is safe to hand to the evaluation thread
	FDataMap Snapshot() const
	{
		FDataMap copy = *this;
		for (FDataMapEntry& entry : copy.Entries)
		{
			if (FDataChunk* chunk = entry.GetMutable()) { chunk->ResolveForSnapshot(); }
		}
		return copy;
	}

	/// Game thread only. Lets chunks bind to the actor that owns this map (see FDataChunk::InitializeForOwner)
	void InitializeForOwner(AActor* inAvatar, UAbilitySystemComponent* inAbilitySystem)
	{
		for (FDataMapEntry& entry : Entries)
		{
			if (FDataChunk* chunk = entry.GetMutable()) { chunk->InitializeForOwner(inAvatar, inAbilitySystem); }
		}
	}

	FString Description() const;
};

#pragma endregion

#pragma region DataModifier

USTRUCT(BlueprintType)
/// <summary>
/// Modifiers are how purposes WRITE data: when a purpose is selected or finished, each modifier changes (or creates) one chunk on one subject
/// Later purposes' conditions READ that data. It is the system's memory and state-transition mechanism
/// Replaces the dataModifier/modifier/tagForAdjustment fields every UDataChunk used to carry, which were only used on purpose assets
/// </summary>
///Design: GAS GameplayEffects Modifiers Purpose; Modifiers are GAS's GameplayEffects for non-attribute data
/// A modifier type FGameplayEffectModifier { TSubclassOf<UGameplayEffect> } that applies the effect to the subject's ASC would cover attributes, tags,
/// durations and stacking in one asset, leaving these struct modifiers for data GAS can't hold (vectors, actor references, history)
struct AO_API FDataModifier
{
	GENERATED_BODY()

	virtual ~FDataModifier() {}

	/// Game thread only. Change, or create, the chunk under key in map
	/// @return bool: true if the map was changed
	virtual bool Apply(FDataMap& map, FGameplayTag key) const { return false; }

	virtual FString Description() const { return TEXT("No modification"); }
};

#pragma endregion

/** Static class with useful gameplay utility functions that can be called from both Blueprint and C++ */
UCLASS()
class AO_API UDataMapGlobals : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintPure, Category = "Data", meta = (DisplayName = "Has Data"))
	///Returns true if a chunk is found for key and that chunk has a valid value
	static bool HasData_BP(const FDataMap& dataMap, UPARAM(meta = (Categories = "Data")) FGameplayTag key) { return dataMap.Has(key); }

	UFUNCTION(BlueprintCallable, Category = "Data", meta = (DisplayName = "Find Data Chunk"))
	/// Copies the chunk for key into outChunk. Use "Get Instanced Struct Value" to read it as a specific chunk type
	static bool FindDataChunk_BP(const FDataMap& dataMap, UPARAM(meta = (Categories = "Data")) FGameplayTag key, FInstancedStruct& outChunk);

	UFUNCTION(BlueprintPure, Category = "Data", meta = (DisplayName = "Get Float Data"))
	/// Value of any FFloatChunk (or derived) under key
	static bool GetFloatData_BP(const FDataMap& dataMap, UPARAM(meta = (Categories = "Data")) FGameplayTag key, float& outValue);
};

UINTERFACE(NotBlueprintable, meta = (CannotImplementInterfaceInBlueprint))
class AO_API UDataMapInterface : public UInterface
{
	GENERATED_BODY()

};

/// <summary>
///DataMapInteface is a means to enforce a data structure across any objects that can contain DataChunks
///It also serves to abstract any objects implementing the interface to a base object pointer with only functionality critical to the data system
///
/// Implementers provide two overrides: DataMap() and MutableDataMap()
/// Replicated implementers also override OnDataMapChanged() to mark their property dirty for push model replication
/// </summary>
class AO_API IDataMapInterface
{
	GENERATED_BODY()
public:

	///Allows evaluations to retrieve chunks of data from any type of object implementing interface
	/// Game thread only: the evaluation thread reads snapshots, never a live DataMap
	virtual const FDataMap& DataMap() const = 0;

	/// Game thread. Replaces DataMapCopy(), see FDataMap::Snapshot
	FDataMap Snapshot() const { return DataMap().Snapshot(); }

	template<typename T = FDataChunk>
	const T* DataChunk(FGameplayTag key) const { return DataMap().Find<T>(key); }

	///Returns true if a chunk is found for key and that chunk has a valid value
	bool HasData(FGameplayTag key) const { return DataMap().Has(key); }

	virtual FString Description() const { return DataMap().Description(); }

	/// Changing data should only be done by authority, as replicated maps are overwritten by the server
	/// AI data is changed by the host (who is authority), so these are plain calls rather than Server RPCs
	/// Every change calls OnDataMapChanged() once

	template<typename T>
	T* SetData(const T& chunk)
	{
		T* result = MutableDataMap().Set(chunk);
		OnDataMapChanged();
		return result;
	}

	void SetData(const FInstancedStruct& chunk)
	{
		if (MutableDataMap().Set(chunk)) { OnDataMapChanged(); }
	}

	///@Param inDataMap: Each chunk will be added to this DataMapInterface, replacing chunks with the same key
	void AppendData(const FDataMap& inDataMap)
	{
		MutableDataMap().Append(inDataMap);
		OnDataMapChanged();
	}

	void RemoveData(FGameplayTag key)
	{
		if (MutableDataMap().Remove(key)) { OnDataMapChanged(); }
	}

	/// Modify a chunk in place. Treats the map as changed whether or not the caller actually writes to it
	template<typename T = FDataChunk>
	T* EditData(FGameplayTag key)
	{
		T* chunk = MutableDataMap().FindMutable<T>(key);
		if (chunk) { OnDataMapChanged(); }
		return chunk;
	}

	template<typename T>
	T* FindOrAddData(FGameplayTag key)
	{
		T* chunk = MutableDataMap().FindOrAdd<T>(key);
		if (chunk) { OnDataMapChanged(); }
		return chunk;
	}

	/// For changes to several chunks at once (modifiers). Treats the map as changed
	FDataMap& EditDataMap()
	{
		OnDataMapChanged();
		return MutableDataMap();
	}

	/// Called whenever the map changes through this interface
	virtual void OnDataMapChanged() {}

private:

	/// Force implementers to provide an editable datamap
	/// Only reachable through the functions above, so every change notifies OnDataMapChanged()
	virtual FDataMap& MutableDataMap() = 0;
};
