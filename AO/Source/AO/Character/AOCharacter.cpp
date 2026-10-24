// Fill out your copyright notice in the Description page of Project Settings.


#include "AOCharacter.h"


// Sets default values
AAOCharacter::AAOCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AAOCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AAOCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AAOCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

