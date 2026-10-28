// Copyright Jordan Cain. All Rights Reserved.


#include "AIActivity.h"
#include "Purpose/DataChunks/ActorRole.h"
#include "Purpose/DataChunks/ActorLocation.h"

// Sets default values
AAIActivity::AAIActivity()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

}

// Called when the game starts or when spawned
void AAIActivity::BeginPlay()
{
	Super::BeginPlay();
	SetData(FActorLocationChunk(this));
	data.InitializeForOwner(this, nullptr);/// Binds any chunks authored in the details panel to this activity
}

void AAIActivity::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
}

// Called every frame
void AAIActivity::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

FContextData AAIActivity::Activity()
{
	Global::Log(CALLTRACEESSENTIAL, PURPOSE, *this, "SeekActivitiesInLevel", TEXT(""));
	FContextData contextData;
	contextData.contextData.Append(DataMap());/// This allows us to specify context data to be evaluated against in Level on Activity
	/// (Previously appended through DataMapInterfaceForSubject(ESubject::Context), which is never an object, so the data was dropped)

	for (auto subject : SubjectMapForActivity)/// Utilize the subjects established on the Activity in Level for the EventContext data
	{
		contextData.AddSubject(subject.Key, TScriptInterface<IDataMapInterface>(subject.Value.Get()));
	}


	Global::Log(DATATRIVIAL, PURPOSE, *this, "Activity", TEXT("%s"), *contextData.Description());
	return contextData;
}
