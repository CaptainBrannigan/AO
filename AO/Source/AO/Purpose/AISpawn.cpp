// Copyright Jordan Cain. All Rights Reserved.


#include "AISpawn.h"
#include "Purpose/DataChunks/ActorRole.h"
#include "Purpose/DataChunks/ActorLocation.h"

// Sets default values
AAISpawner::AAISpawner()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	data.Set(FRoleChunk(EActorRole::AI));
	data.Set(FActorLocationChunk());/// Binds to whoever receives the data (InitializeForOwner), so spawned AI report their own location
}

// Called when the game starts or when spawned
void AAISpawner::BeginPlay()
{
	Super::BeginPlay();
	data.InitializeForOwner(this, nullptr);/// The spawner's own location, until a spawned AI rebinds its copy to itself
}

void AAISpawner::InitializeTriggersForSpawn()
{
#if WITH_EDITOR

	for (TObjectPtr<ATriggerBase> trigger : spawnZones)
	{
		trigger->SetFolderPath("SpawnZones");
		FString displayName = "SpawnZone-" + trigger->GetClass()->GetName();
		if (const FRoleChunk* roleChunk = DataChunk<FRoleChunk>(TAG_Data_Role))
		{
			displayName += "-" + roleChunk->ValueAsString();
		}
		trigger->Rename(*displayName);
		trigger->SetActorLabel(*displayName);
		trigger->SetActorEnableCollision(false);
		trigger->GetCollisionComponent()->SetCanEverAffectNavigation(false);
		trigger->GetCollisionComponent()->bNavigationRelevant = false;
		trigger->GetCollisionComponent()->SetAreaClassOverride(nullptr);
		trigger->GetCollisionComponent()->SetGenerateOverlapEvents(false);
		trigger->GetCollisionComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		trigger->GetCollisionComponent()->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
		trigger->GetCollisionComponent()->SetEnableGravity(false);
		trigger->GetCollisionComponent()->SetMobility(EComponentMobility::Static);
		trigger->GetCollisionComponent()->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;
		trigger->GetCollisionComponent()->SetSimulatePhysics(false);
		trigger->GetCollisionComponent()->bReplicatePhysicsToAutonomousProxy = false;
		trigger->GetCollisionComponent()->bApplyImpulseOnDamage = false;
		trigger->GetCollisionComponent()->bIgnoreRadialImpulse = true;
	}
	
#endif
}

