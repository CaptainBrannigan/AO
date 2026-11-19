

#pragma once

#include "CoreMinimal.h"
#include "PurposeEvaluationThread.h"
#include "Manager.h"
#include "Subsystems/WorldSubsystem.h"
#include "PurposeSubsystem.generated.h"



USTRUCT(BlueprintType)
struct FFactionManagers
{
	GENERATED_BODY()
	
	EFactionID id = EFactionID::None;

	TMap<FPurposeEntity, TObjectPtr<UPurposeAbilityComponent>> managers;
};

UCLASS()
/// <summary>
/// The director of purpose in a level
///		Directly manages the faction routing
/// </summary>
class AO_API UPurposeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
#pragma region WorldSubsystem
public:

	/** Calls DoesSupportWorldType to check if this subsystem should be created */
	/*ENGINE_API*/ virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** Called once all UWorldSubsystems have been initialized */
	/*ENGINE_API*/ virtual void PostInitialize() override;

	/** Called once all UWorldSubsystems are about to be deinitialized */
	virtual void PreDeinitialize() override;

	/** Called when world is ready to start gameplay before the game mode transitions to the correct state and call BeginPlay on all actors */
	/*ENGINE_API*/  virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Called when world is ready to stop gameplay before the game mode transitions to the correct state and call EndPlay on all actors */
	virtual void OnWorldEndPlay(UWorld& InWorld) override;

	/** Called after world components (e.g. line batcher and all level components) have been updated */
	virtual void OnWorldComponentsUpdated(UWorld& World) override;

protected:
	/**
	 * Override this to change what types of worlds support this subsystem.
	 * By default, world subsystems support game, PIE, and editor worlds but not preview worlds.
	 */
	/*ENGINE_API*/ virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

#pragma endregion


#pragma region Purpose

public:
	
	/// <summary>
	/// Primary group will be the group controlling the map against secondary
	/// </summary>
	FFactionManagers primaryGroup;

	/// <summary>
	/// Secondary group is the invading group against primary
	/// </summary>
	FFactionManagers secondaryGroup;

	/// <summary>
	/// Spawns in a manager at the location to establish the AI presence for a POI
	/// </summary>
	/// <param name="inFaction"></param>
	/// <param name="inManagementData: the data responsible for establishing AI in world"></param>
	/// Refactor: Purpose PCG; Instead of a simple fvector, provide a data object that lists fields relevant to that poi to be added to manager
	void EstablishFactionPOI(EFactionID inFaction, const FManagementData inManagementData);
#pragma endregion
};
