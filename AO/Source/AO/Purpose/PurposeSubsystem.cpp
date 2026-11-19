


#include "PurposeSubsystem.h"
#include "Data/GlobalLog.h"

#pragma region WorldSubsystem

bool UPurposeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	Super::ShouldCreateSubsystem(Outer);
    return false;
}

void UPurposeSubsystem::PostInitialize()
{
	Super::PostInitialize();
}

void UPurposeSubsystem::PreDeinitialize()
{
	Super::PreDeinitialize();
}

void UPurposeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UPurposeSubsystem::OnWorldEndPlay(UWorld& InWorld)
{
	Super::OnWorldEndPlay(InWorld);
}

void UPurposeSubsystem::OnWorldComponentsUpdated(UWorld& World)
{
	Super::OnWorldComponentsUpdated(World);
}

bool UPurposeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	Super::DoesSupportWorldType(WorldType);
    return false;
}
#pragma endregion

#pragma region Purpose

void UPurposeSubsystem::EstablishFactionPOI(EFactionID inFaction, const FManagementData inManagementData)
{
	FFactionManagers& factionGroup = inFaction == primaryGroup.id ? primaryGroup : secondaryGroup;

	bool newLocation = true;
	for (auto manager : factionGroup.managers)
	{
		float minDistanceForNew = 300.0f;/// If the inLocation is within a minimum distance of an existing manager it isn't new
		if (FVector::Distance(manager.Value->GetOwner()->GetActorLocation(), inManagementData.location) < minDistanceForNew)
		{
			newLocation = false;
			break;
		}
	}

	if (!newLocation) { return; }

	TWeakObjectPtr<UWorld> world = GetWorld();

	FPurposeEntity newEntity(inFaction, factionGroup.managers.Num() + 1);/// Num()+1 is required for the id calculation

	if (!factionGroup.managers.Contains(newEntity))/// This should never occur, but in case of manager removal
	{
		Global::LogError(LogPurpose, "PurposeSubsystem", "FactionPOI", 
						 TEXT("Entity at index %d already exists for faction %s. Resolve duplicate id's"), factionGroup.managers.Num() + 1, Global::EnumValueOnly<EFactionID>(inFaction));
		return;
	}

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	spawnParams.Name = FName(Global::EnumValueOnly<EFactionID>(inFaction) + ".manager." + FString::FromInt(newEntity.id)); ///TODO: Establish naming convention based on entity id for managers

	/// Refactor: Manager PurposeSystem; Spawn in managers via async task during level loading.
	TObjectPtr<AManager> newManager = world->SpawnActor<AManager>(AManager::StaticClass(), FTransform(inManagementData.location), spawnParams);
	TObjectPtr<UPurposeAbilityComponent> newPurposeComponent = 
		Cast<UPurposeAbilityComponent>(newManager->AddComponentByClass(UPurposeAbilityComponent::StaticClass(), false, FTransform(), false));

	newManager->managerData = inManagementData;

	newPurposeComponent->InitializeDataForOwner();///Refactor: Purpose Manager; needs to be called after data is added
	
	factionGroup.managers.Add(newEntity, newPurposeComponent);
}

#pragma endregion