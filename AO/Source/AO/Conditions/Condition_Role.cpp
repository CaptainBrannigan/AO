// Copyright Jordan Cain. All Rights Reserved.


#include "Condition_Role.h"

float UCondition_Role::Evaluate(const FSubjectDataView& data) const
{
	const FRoleChunk* primary = Primary<FRoleChunk>(data);
	const FRoleChunk* secondary = roles.Num() < 1 ? Secondary<FRoleChunk>(data) : nullptr;
	if (!primary || (roles.Num() < 1 && !secondary))
	{
		Global::Log(DATADEBUG, LogCondition, "UCondition_Role", "Evaluate", TEXT("Primary||Secondary invalid. Description: %s"), *description.ToString());
		return 0.0f;
	}

	/// Refactor: Condition Scoring Evaluation; Utilize a response curve and min-max values to create a 0-1 score
		/// This way we can adjust the score to reflect how many of those roles are present
	const TArray<EActorRole>& primaryValue = primary->Value();///Primary data is valid if we reach this point
	const TArray<EActorRole>& secondaryValue = roles.Num() < 1 ? secondary->Value() : roles;///Use specified value if set, otherwise use found value of secondary

	if (primaryValue.Num() < 1 || secondaryValue.Num() < 1) { return 0.0f; }

	float value = 0;/// Integer division truncates. While the value can only ever change by 1, the math is more safe as a float
	for (const EActorRole role : secondaryValue)
	{
		if (primaryValue.Contains(role))
		{
			value++;
		}
	}

	switch (conditional)
	{
		case EConditional_Role::IsA:
			return value / primaryValue.Num();/// For every role contained, score increases
		case EConditional_Role::IsNotA:
			return 1 - (value / primaryValue.Num());/// For every role contained, score decreases.
		default:
			return 0.0f;
	}
}
