// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Data/GlobalLog.h"
#include "AnimInstance_Base.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI)
class UAnimInstance_Base : public UAnimInstance
{
	GENERATED_BODY()
public:

	///Play an animation montage
	///Return the instance so that the animation's delegates can be bound to
	FAnimMontageInstance* PlayMontage(TObjectPtr<UAnimMontage> MontageToPlay, float InPlayRate = 1.f, EMontagePlayReturnType ReturnValueType = EMontagePlayReturnType::MontageLength, float InTimeToStartMontageAt = 0.f, bool bStopAllMontages = true);


protected:

};
