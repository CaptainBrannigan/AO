

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h" 
#include "PurposeAbilityComponent.h"
#include "PurposeEvaluationThread.h"
#include "Data/DatamapInterface.h"
#include "Manager.generated.h"

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
	GENERATED_BODY();
public:

	uint8 entityID = 0;

	bool operator==(const FPurposeEntity& Other) const
	{
		return Equals(Other);
	}

	bool Equals(const FPurposeEntity& Other) const
	{
		return entityID == Other.entityID;
	}
};
#if UE_BUILD_DEBUG
uint32 GetTypeHash(const FHashMeIfYouCan& Thing);
#else // optimize by inlining in shipping and development builds
FORCEINLINE uint32 GetTypeHash(const FPurposeEntity& Thing)
{
	uint32 Hash = FCrc::MemCrc32(&Thing, sizeof(FPurposeEntity));
	return Hash;
}
#endif

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
