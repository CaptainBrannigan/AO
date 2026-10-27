// Copyright Jordan Cain. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Data/GlobalLog.h"
#include "Data/Data.h"
#include "GameInstance_Base.generated.h"

/**
 *
 */
UCLASS()
class AO_API UGameInstance_Base : public UGameInstance
{
	GENERATED_BODY()
public:
//
//	/** function to allow custom GameInstances an opportunity to set up what it needs */
//	void Init() final;
//
//	/** function to allow custom GameInstances an opportunity to do cleanup when shutting down */
//	void Shutdown() final;
//
//	/** Callback from the world context when the world changes */
////	void OnWorldChanged(UWorld* OldWorld, UWorld* NewWorld) final { Super::OnWorldChanged; }
//
//	/** Starts the GameInstance state machine running */
//	void StartGameInstance() final;
//
//	/** Called when the game instance is started either normally or through PIE. */
//	void OnStart() final;

#pragma region Session
	//public:
	//
	//	bool JoinSession(ULocalPlayer* LocalPlayer, int32 SessionIndexInSearchResults) final { return false; }
	//	bool JoinSession(ULocalPlayer* LocalPlayer, const FOnlineSessionSearchResult& SearchResult) final { return false; }

		/// GameMode calls back to the GameInstance when a map is loaded and gamemode exists
	//void LoadComplete(const float LoadTime, const FString& MapName) final;
	//
#pragma endregion
//
#pragma region Player
public:

	//	ULocalPlayer* CreateInitialPlayer(FString& OutError) final { return nullptr; }
	//
	//	/**
	//	 * Adds a LocalPlayer to the local and global list of Players.
	//	 *
	//	 * @param	NewPlayer	the player to add
	//	 * @param	ControllerId id of the controller associated with the player
	//	 */
	//	int32 AddLocalPlayer(ULocalPlayer* NewPlayer, int32 ControllerId) final { return int32(); }
	//
	//	/**
	//	 * Removes a player.
	//	 * @param Player - The player to remove.
	//	 * @return whether the player was successfully removed. Removal is not allowed while connected to a server.
	//	 */
	//	bool RemoveLocalPlayer(ULocalPlayer* ExistingPlayer) final { return false; }

#pragma endregion

#pragma region AI
public:


#pragma endregion

#pragma region Networking
	//public:
	//
	//	/**
	//	 * Turns on/off listen server capability for a game instance
	//	 * By default this will set up the persistent URL state so it persists across server travels and spawn the appropriate network listener
	//	 *
	//	 * @param bEnable turn on or off the listen server
	//	 * @param PortOverride will use this specific port, or if 0 will use the URL default port
	//	 * @return true if desired settings were applied, games can override to deny changes in certain states
	//	 */
	//	bool EnableListenServer(bool bEnable, int32 PortOverride = 0) final { return false; }
	//
	//	/** handle a game specific net control message (NMT_GameSpecific)
	//	 * this allows games to insert their own logic into the control channel
	//	 * the meaning of both data parameters is game-specific
	//	 */
	//	void HandleGameNetControlMessage(class UNetConnection* Connection, uint8 MessageByte, const FString& MessageStr) final {}
	//
	//   /** Handle setting up encryption keys. Games that override this MUST call the delegate when their own (possibly async) processing is complete. */
	//	void ReceivedNetworkEncryptionToken(const FString& EncryptionToken, const FOnEncryptionKeyResponse& Delegate) final {}
	//
	//	/** Called when a client receives the EncryptionAck control message from the server, will generally enable encryption. */
	//	void ReceivedNetworkEncryptionAck(const FOnEncryptionKeyResponse& Delegate) final {}
	//
	//	/** Call to preload any content before loading a map URL, used during seamless travel as well as map loading */
	//	void PreloadContentForURL(FURL InURL) final {}
	//
	//	/** Call to create the game mode for a given map URL */
	//	class AGameModeBase* CreateGameModeForURL(FURL InURL, UWorld* InWorld) final { return nullptr; }
	//
	//	/** Call to modify the saved url that will be used as a base for future map travels */
	//	void SetPersistentTravelURL(FURL InURL) final {}
	//
	//	/** Return the game mode subclass to use for a given map, options, and portal. By default return passed in one */
	//	TSubclassOf<AGameModeBase> OverrideGameModeClass(TSubclassOf<AGameModeBase> GameModeClass, const FString& MapName, const FString& Options, const FString& Portal) const final { return nullptr; }
	//
	//	/** return true to delay an otherwise ready-to-join PendingNetGame performing LoadMap() and finishing up
	//	 * useful to wait for content downloads, etc
	//	 */
	//	bool DelayPendingNetGameTravel() final
	//	{
	//		return false;
	//	}
	//
	//
	//	/** @return OnlineSession class to use for this game instance  */
	//	TSubclassOf<UOnlineSession> GetOnlineSessionClass() final { return nullptr; }
	//
	//	/**
	//	 * Retrieves the name of the online subsystem for the platform used by this instance.
	//	 * This will be used as the value of the PlayerOnlinePlatformName parameter in
	//	 * the NMT_Login message when this client connects to a server.
	//	 * Normally this will be the same as the DefaultPlatformService config value,
	//	 * but games may override it if they need non-default behavior (for example,
	//	 * if they are using multiple online subsystems at the same time).
	//	 */
	//	FName GetOnlinePlatformName() const final { return FName(); }
	//
	//	/**
	//	 * Helper function for traveling to a session that has already been joined via the online platform
	//	 * Grabs the URL from the session info and travels
	//	 *
	//	 * @param ControllerId controller initiating the request
	//	 * @param InSessionName name of session to travel to
	//	 *
	//	 * @return true if able or attempting to travel, false otherwise
	//	 */
	//	bool ClientTravelToSession(int32 ControllerId, FName InSessionName) final { return false; }
	//
	//	/**
	//	 * Calls HandleDisconnect on either the OnlineSession if it exists or the engine, to cause a travel back to the default map. The instance must have a world.
	//	 */
	//	void ReturnToMainMenu() final {}
	//
	//	/** Registers an object to keep alive as long as this game instance is alive */
	//	void RegisterReferencedObject(UObject* ObjectToReference) final {}
	//
	//	/** Remove a referenced object, this will allow it to GC out */
	//	void UnregisterReferencedObject(UObject* ObjectToReference) final {}

#pragma endregion
};
