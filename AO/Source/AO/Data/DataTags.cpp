// Copyright Jordan Cain. All Rights Reserved.

#include "DataTags.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Data_Actor, "Data.Actor", "Direct reference to the subject's actor");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Data_Location, "Data.Location", "The subject's world location");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Data_Velocity, "Data.Velocity", "The subject's velocity");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Data_ViewPoint, "Data.ViewPoint", "The subject's eye location and view direction");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Data_Role, "Data.Role", "The roles an actor fills (Player, AI, Companion...)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Data_Archetype, "Data.Archetype", "Hero or Minion");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Data_Race, "Data.Race", "Highest level of faction");

UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Data_Mass, "Data.Mass", "Weight of the object it belongs to");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Data_Morale, "Data.Morale", "Effectively a health indicator");
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_CompanionCount, "Data.CompanionCount");
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_EngagedCombatants, "Data.EngagedCombatants");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Data_LevelOfAlarm, "Data.LevelOfAlarm", "0 = no perception of danger, highest = danger close and imminent");

UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Data_ContextualHistory, "Data.ContextualHistory", "Timeline of contextual tags conditions may query");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Data_BehaviorTags, "Data.BehaviorTags", "Tags describing the current behavior");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Data_Time_Occurrence, "Data.Time.Occurrence", "When an occurrence was raised");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Data_InteractibleObjects, "Data.InteractibleObjects", "Interaction points for activities");

UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Task_MoveTo_Speed, "Data.Task.MoveTo.Speed");
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Task_MoveTo_Goal, "Data.Task.MoveTo.Goal");
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Task_MoveTo_PathLength, "Data.Task.MoveTo.PathLength");
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Task_MoveTo_Duration, "Data.Task.MoveTo.Duration");
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Task_FaceDirection_Location, "Data.Task.FaceDirection.Location");
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Task_Repeat_Count, "Data.Task.Repeat.Count");
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Task_Repeat_Interval, "Data.Task.Repeat.Interval");
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Task_EQS_Result, "Data.Task.EQS.Result");
