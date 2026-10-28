// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Condition_ContextualHistory.h"
#include "Condition_ContextualHistory_EngagedByCountForSubject.generated.h"

UCLASS(CollapseCategories)
/// <summary>
/// The highest recent Contextual.Combat.Engaged.N count on the subject's contextual history (written by FContextualHistoryModifier_Engaged)
/// Ported from Warzone, which ignored maxTimeSinceEngagement (hardcoded 5s) and parsed the count from the tag's last character
/// </summary>
///Design: GAS GameplayEffects Stacking Engaged; With Engaged as a stacking duration GameplayEffect this is the effect's stack count,
/// read from the subject's ASC at snapshot into a float chunk, so a plain Condition_Float replaces this condition
class AO_API UCondition_ContextualHistory_EngagedByCountForSubject : public UCondition_ContextualHistory
{
	GENERATED_BODY()
public:

	UCondition_ContextualHistory_EngagedByCountForSubject();

	UPROPERTY(EditAnywhere, Category = "Condition Data", DisplayName = "Max Time Since Engagement", meta = (DisplayPriority = "14"))
	/// Any Combat.Engaged tag whose time since is greater than this will not be considered relevant
	float maxTimeSinceEngagement = 5.0f;

protected:

	float Evaluate(const FSubjectDataView& data) const final;
};
