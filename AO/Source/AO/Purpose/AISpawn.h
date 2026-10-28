// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameFramework/Actor.h"
#include "Data/Data.h"
#include "Data/DataMapInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/TriggerBase.h"
#include "Components/ShapeComponent.h"
#include "AISpawn.generated.h"

USTRUCT(BlueprintType)
//
struct FAISpawnParameters
{
	GENERATED_BODY()

	FAISpawnParameters()
	{
	}

	UPROPERTY(EditAnywhere, DisplayName = "Data of AI Group", Category = "Data")
	FDataMap data;


};

UCLASS()
class AO_API AAISpawner : public AActor, public IDataMapInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AAISpawner();

	///TODO: AI Bot Spawn; Bot spawning is removed temporarily due to reliance on Lyra systems
	//UPROPERTY(EditAnywhere, DisplayName = "Type of AI", Category = "Group Spawn Params")
	/////The controller of the pawn that will be spawned as AI
	//TSubclassOf<class ALyraPlayerBotController> controller = nullptr;

	UPROPERTY(EditAnywhere, DisplayName = "Default data of AI", Category = "Group Spawn Params")
	///This data is representative of the subject "ESubject::Candidate"
	/// Spawned AI receive a copy (AppendData) and bind it to themselves with UPurposeAbilityComponent::InitializeDataForOwner
	FDataMap data;

	UPROPERTY(EditAnywhere, DisplayName = "Group Size", Category = "Group Spawn Params")
	///How many of role are spawned into level
	int numAI = 0;

	UPROPERTY(EditAnywhere, DisplayName = "When to spawn in AI", Category = "Group Spawn Params")
	///Time in seconds from beginning of level to spawn in AI
	/// if 0, AI will be spawned in immediately
	int spawnTime = 0;

	UPROPERTY(EditAnywhere, DisplayName = "Group Spawn Zones", Category = "Group Spawn Params")
	//The areas designated by a shape in which to spawn AI
	//Can be used with 		
		//FMath::RandPointInCircle
		//FMath::RandPointInBox
	TArray<TObjectPtr<class ATriggerBase>> spawnZones;

	UPROPERTY(EditAnywhere, DisplayName = "Player Triggered Load Zones", Category = "Group Spawn Params")
	//These areas can be utilized to load/unload AI actors based on player presence
	TArray<TObjectPtr<class ATriggerBase>> loadZones;

	/// We must allow management system to randomize skeletal meshes
	/// As it must be done server side to replicate
	/// Potential meshes are only used if there is not a skeletal mesh set already
	TArray<TObjectPtr<USkeletalMesh>> PotentialMeshesForActor() { return potentialMeshesForActor; }

	TSubclassOf<class UAnimInstance> AnimBPForActor() { return animBPForActor; }

protected:

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable)
	/// To be called strictly as an Editor Function when new spawnParams are created
	///Will nullify unneeded logic, such as collision and navmesh
	///Will also sort triggers in outliner, and rename them to match their role
	void InitializeTriggersForSpawn();

	UPROPERTY(EditAnywhere, Category = "ActorData")
	TArray<TObjectPtr<USkeletalMesh>> potentialMeshesForActor;

	UPROPERTY(EditAnywhere, Category = "ActorData")
	TSubclassOf<class UAnimInstance> animBPForActor = nullptr;

private:



#pragma region Datamap Interface
public:

	const FDataMap& DataMap() const final { return data; }

private:

	/// Force implementers to provide an editable datamap
	FDataMap& MutableDataMap() final { return data; }

#pragma endregion
};

