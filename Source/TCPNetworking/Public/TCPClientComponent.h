#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"

// Native sockets
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "TCPClientComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDataReceivedSignature, const TArray<uint8>&, ReceivedData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnServerDisconnectedSignature, const FString&, IP, const int32&, Port);

UCLASS()
class TCPNETWORKING_API UTCPClientComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	/** @brief Bindable delegate for receiving arrived data from server */
	UPROPERTY(BlueprintAssignable, Category = "TCP Client")
	FOnDataReceivedSignature OnDataReceived;

	/** @brief Bindable delegate for detecting server disconnection */
	UPROPERTY(BlueprintAssignable, Category = "TCP Client")
	FOnServerDisconnectedSignature OnServerDisconnected;

	/** @brief Flag for reconnecting if the server can't be reached (from ConnectToSocket)
	 * or disconnected (after OnServerDisconnected is broadcasted) */
	UPROPERTY(EditAnywhere, Category = "TCP Client Properties")
	bool bReconnectOnFailure;

	/** @brief Maximum amount of attempts before stopping trying */
	UPROPERTY(EditAnywhere, Category = "TCP Client Properties")
	int32 MaxReconAttemptCount;

	/** @brief Buffer size for received bytes */
	UPROPERTY(EditAnywhere, Category = "TCP Client Properties")
	int32 ReceiveBufferSize;

private:
	int32 SocketFD;
	bool bIsConnected;

	/** @brief Server IP */
	UPROPERTY(EditAnywhere, Category = "TCP Client Properties")
	FString ServerIP;

	/** @brief Server Port */
	UPROPERTY(EditAnywhere, Category = "TCP Client Properties")
	int32 ServerPort;

public:
	UTCPClientComponent();

	UFUNCTION(BlueprintCallable, Category = "TCP Client")
	void ConnectToServer(const FString& IP, const int32& Port);
	
	/** @brief Sends data on the server if connected */
	UFUNCTION(BlueprintCallable, Category = "TCP Client")
	bool SendData(const TArray<uint8>& Data);

	UFUNCTION(BlueprintCallable, Category = "TCP Client")
	bool IsConnected() const;

	/** @brief Checks for pending data and closes the client socket */
	UFUNCTION(BlueprintCallable, Category = "TCP Client")
	void CloseConnection();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		enum ELevelTick TickType,
		FActorComponentTickFunction * ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** @brief Creates non-blocking socket, after that tries to connect to the server
	 * each second until AttemptCount exceeds MaxReconAttemptCount.
	 * Reconnects could be disabled by setting bReconnectOnFailure to false
	 */
	bool ConnectToSocket(const FString& ServerIP, int32 ServerPort);
	int32 TryReceiveData(TArray<uint8>& OutData, int32 BufferSize);
};
