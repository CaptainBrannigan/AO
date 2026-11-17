


#include "Manager.h"


// Sets default values
AManager::AManager()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AManager::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}


#include "Misc/Crc.h"

#if UE_BUILD_DEBUG
uint32 GetTypeHash(const FHashMeIfYouCan& Thing)
{
	uint32 Hash = FCrc::MemCrc32(&Thing, sizeof(FHashMeIfYouCan));
	return Hash;
}
#endif