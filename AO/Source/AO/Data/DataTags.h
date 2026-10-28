// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "NativeGameplayTags.h"

/// Keys for DataChunks that are used from C++
/// Every chunk key lives under "Data." so the key pickers (meta = (Categories = "Data")) only show chunk names
/// Designer-only keys (Data.Patrol.LeaderCount, etc.) can be added in Project Settings > Gameplay Tags, no C++ needed

/// Actor
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Actor);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Location);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Velocity);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_ViewPoint);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Role);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Archetype);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Race);

/// Floats
///Design: GAS Attributes FloatChunk; These float chunks are good candidates for GameplayAttributes
/// Morale reads like health, Mass/CompanionCount/EngagedCombatants/LevelOfAlarm are per-actor stats
/// See FAttributeChunk (FloatChunk.h) for how a float chunk can point at an attribute instead of storing its value
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Mass);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Morale);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_CompanionCount);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_EngagedCombatants);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_LevelOfAlarm);

/// Context
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_ContextualHistory);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_BehaviorTags);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Time_Occurrence);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_InteractibleObjects);

/// Task data, shared between tasks via FTaskAndDataPair and appended to the behavior's context data
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Task_MoveTo_Speed);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Task_MoveTo_Goal);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Task_MoveTo_PathLength);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Task_MoveTo_Duration);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Task_FaceDirection_Location);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Task_Repeat_Count);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Task_Repeat_Interval);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Task_EQS_Result);
