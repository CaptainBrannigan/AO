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
	data.Add(FDataMapEntry(&NewObject<UActorLocation>(this)->Initialize(this)));///

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
	contextData.DataMapInterfaceForSubject(ESubject::Context)->AppendData(DataMap());/// This allows us to specify context data to be evaluated against in Level on Activity

	for (auto subject : SubjectMapForActivity)/// Utilize the subjects established on the Activity in Level for the EventContext data
	{
		contextData.AddSubject(subject.Key, TScriptInterface<IDataMapInterface>(subject.Value.Get()));
	}


	Global::Log(DATATRIVIAL, PURPOSE, *this, "Activity", TEXT("%s"), *contextData.Description());
	return contextData;
}

void AAIActivity::AddData_Implementation(UDataChunk* inData, bool overwriteValue)
{
	AddDataLocal(*inData, overwriteValue);
}

void AAIActivity::AppendData_Implementation(const TArray<FDataMapEntry>& inDataMap, bool overwriteValue)
{
	AppendDataLocal(inDataMap, overwriteValue);
}

void AAIActivity::RemoveData_Implementation(TSubclassOf<UDataChunk> inClass)
{
	RemoveDataLocal(inClass);
}

