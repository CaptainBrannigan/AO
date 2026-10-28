// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/DataMapInterface.h"
#include "PurposeEvaluationThread.h"
#include "AIActivity.generated.h"

UCLASS()
/// When a level opens and is populated, Level Directors can seek for these objects
/// In order to provide Managers with pre set GoalAssets for the level
/// AI Activities utilize a DataMap as ESubject::Context
/// This allows us to specify in level data relevant to the activity that may be considered as Conditions
class AO_API AAIActivity : public AActor, public IDataMapInterface
{
	GENERATED_BODY()
	
public:	
	/// Sets default values for this actor's properties
	AAIActivity();


protected:
	/// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	/** Overridable function called whenever this actor is being removed from a level */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
public:	
	/// Called every frame
	/// Default bActorCanEverTick = false
	virtual void Tick(float DeltaTime) override;

	const FDataMap& DataMap() const final { return data; }

	/// Design: Purpose Activity; Have Activity use data to specify when occurrence for Activity is called
		/// Such as a time of day
		/// Time in Level
		/// etc.
	FContextData Activity();

	UPROPERTY(EditAnywhere)
	/// Establish an activity for groups of AI to participate in
	/// This "activity" can be available for one or multiple roles
	FPurpose eventForActivity;

	///// Todo: Purpose Activity Management; Should we use a subject map + data chunks?
	//	/// We could then specify the subjects of this activity if necessary
	//	/// The activity object would be the default ESubject::Context
	//UPROPERTY(EditAnywhere, Instanced)
	///// These data chunks allow us to create an in editor context
	///// Data relevant to this activity can be stored here
	//TArray<TObjectPtr<UDataChunk>> dataChunks;

	UPROPERTY(EditAnywhere, Category = "Data")
	///This data is representative of the subject "ESubject::Context"
	FDataMap data;

private:

	/// Force implementers to provide an editable datamap
	FDataMap& MutableDataMap() final { return data; }

public:

	UPROPERTY(EditAnywhere)
	///This data is representative of the subject "ESubject::Context"
	//TMap<ESubject, TScriptInterface<IDataMapInterface>> SubjectMapForActivity;
	TMap<ESubject, TObjectPtr<AActor>> SubjectMapForActivity = 
	{
	};

};
