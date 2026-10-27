// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AT_Tick.generated.h"

/// Design: Task Ability; Consider requiring a UGameplayAbility pointer here so we don't have to rely on payload parameters
DECLARE_DELEGATE_OneParam(FOnTickTaskDelegate, float);

/**
 * Provides a GameplayAbility Tick functionality
 */
UCLASS()
class UAT_Tick : public UAbilityTask
{
	GENERATED_BODY()

	//UPROPERTY(BlueprintAssignable)
	FOnTickTaskDelegate OnTick;

public:

	UAT_Tick(const FObjectInitializer& ObjectInitializer)
		:Super(ObjectInitializer)
	{
		bTickingTask = true;
	}

	static UAT_Tick* TickingTaskForAbility(UGameplayAbility* OwningAbility, FName TaskInstanceName)
	{
		UAT_Tick* task = NewAbilityTask<UAT_Tick>(OwningAbility);
		return task;
	}

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;

	float customTickInterval = 0.0f;

private:

	float tickTimer = 0.0f;

};