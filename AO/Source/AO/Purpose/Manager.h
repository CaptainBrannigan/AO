

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h" 
#include "PurposeAbilityComponent.h"
#include "PurposeEvaluationThread.h"
#include "Data/DatamapInterface.h"
#include "Manager.generated.h"
UENUM(BlueprintType)
enum class EFactionID : uint8
{
	None UMETA(DisplayName = "None")///Default to ensure data is set with HasData()
	, SOCOM UMETA(DisplayName = "SOCOM")//1
	, SAS UMETA(DisplayName = "SAS")//2
	, SASR UMETA(DisplayName = "SASR")//3
	, JTF2 UMETA(DisplayName = "JTF2")//4
	, GROM UMETA(DisplayName = "GROM")//5
	, KSK UMETA(DisplayName = "KSK")//6
	, FSK UMETA(DisplayName = "FSK")//7
	, SOG UMETA(DisplayName = "SOG")//8
	, UDT UMETA(DisplayName = "UDT")//9
	, Spetsnaz UMETA(DisplayName = "Spetsnaz")//10
};

USTRUCT(BlueprintType)
/// <summary>
/// Used to allow separation of purpose entities
///		Factions
///			Individual squads within a faction
///				Individual AI within a squad
/// This Entity identification is generated once upon entity addition to the purpose system
/// </summary>
struct FPurposeEntity
{
	GENERATED_BODY()

	static constexpr uint32 MemberBits = 12;
	static constexpr uint32 ManagerBits = 12;
	static constexpr uint32 FactionBits = 8;

	static constexpr uint32 ManagerShift = MemberBits;
	static constexpr uint32 FactionShift = MemberBits + ManagerBits;

	static constexpr uint32 MemberMask = (1u << MemberBits) - 1;	/// 0xFFF
	static constexpr uint32 ManagerMask = (1u << ManagerBits) - 1;	/// 0xFFF
	static constexpr uint32 FactionMask = (1u << FactionBits) - 1;	/// 0xFF

	UPROPERTY()
	uint32 id = 0;	/// 0 is invalid: faction None, manager 0

	FPurposeEntity()
	{
	}
	FPurposeEntity(EFactionID faction, uint32 managerIndex)
	{
		check(managerIndex <= ManagerMask);
		id = (uint32(faction) << FactionShift) | (managerIndex << ManagerShift);
	}

	FPurposeEntity(const FPurposeEntity& managerEntity, uint32 memberIndex)
		: FPurposeEntity(managerEntity.GetFaction(), managerEntity.GetManager())
	{
		check(memberIndex <= MemberMask);
		id |= memberIndex;
	}

	EFactionID GetFaction() const { return EFactionID((id >> FactionShift) & FactionMask); }
	uint32 GetManager() const { return (id >> ManagerShift) & ManagerMask; }
	uint32 GetMember() const { return id & MemberMask; }

	bool IsValid() const { return id != 0; }
	bool IsManager() const { return GetManager() != 0 && GetMember() == 0; }

	bool operator==(const FPurposeEntity& other) const { return id == other.id; }
	friend uint32 GetTypeHash(const FPurposeEntity& entity) { return entity.id; }
};

USTRUCT(BlueprintType)
/// <summary>
/// Data used by POIs in PCG generation to provide managers with world data
/// </summary>
struct FManagementData
{
	GENERATED_BODY();
public:

	FVector location = FVector(0);
	float locationBounds = 0.0f;
	int AIAllowanceForManager = 0;
};

UCLASS()
/// <summary>
/// The core management actor for the Purpose system. Manages:
///		Creation of AI
///		Routing of Occurrences
///		Clustering of AI and relevance of occurrences
///	Utilizes a sub manager chain to establish identities of groups:
///		Director (Overarching, neutral manager of all occurrences)
///			Faction (Managers that separate into faction types, dictates relationship of subs to an occurrence's Instigator)
///				Squad (Managers that hold clusters of AI within a faction)
/// Managers do not know their own group relationship to other managers at their level, only maintaining that of those below them
/// </summary>
class AO_API AManager : public AActor, public IDataMapInterface
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AManager();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

#pragma region Purpose System 
public:
	void Occurrence();

	FManagementData managerData;
protected:

	/// <summary>
	/// Sub manager array
	/// </summary>
	TMap<FPurposeEntity, TObjectPtr<UPurposeAbilityComponent>> managedEntities;
#pragma endregion

#pragma region DataMapInterface

public:

	UPROPERTY(VisibleAnywhere)
	FDataMap dataMap;

	///Allows evaluations to retrieve chunks of data from any type of object implementing interface
	/// Game thread only: the evaluation thread reads snapshots, never a live DataMap
	virtual const FDataMap& DataMap() const
	{
		return dataMap;
	}

	/// Called whenever the map changes through this interface
	virtual void OnDataMapChanged() {}

private:

	/// Force implementers to provide an editable datamap
	/// Only reachable through the functions above, so every change notifies OnDataMapChanged()
	virtual FDataMap& MutableDataMap()
	{
		return dataMap;
	}

#pragma endregion
};
