#include "TCPClientComponent.h"

UTCPClientComponent::UTCPClientComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bReconnectOnFailure = true;
	MaxReconAttemptCount = 1e6;
	ReceiveBufferSize = 1024;
	SocketFD = -1;
	bIsConnected = false;
}

void UTCPClientComponent::ConnectToServer(const FString& IP, const int32& Port)
{
	if (bIsConnected)
	{
		UE_LOG(LogTemp, Warning, TEXT("UTCPClientComponent: Already connected to the server!"));
		return;
	}

	ServerIP = IP;
	ServerPort = Port;

	if (bReconnectOnFailure)
	{
		OnServerDisconnected.AddUniqueDynamic(this, &UTCPClientComponent::ConnectToServer);
	}

	ConnectToSocket(ServerIP, ServerPort);
}

bool UTCPClientComponent::SendData(const TArray<uint8>& Data)
{
	if (!bIsConnected || SocketFD == -1)
	{
		UE_LOG(LogTemp, Error, TEXT("UTCPClientComponent: Not connected to server"));
		return false;
	}

	ssize_t SentBytes = send(SocketFD, Data.GetData(), Data.Num(), 0);
	if (SentBytes == -1)
	{
		UE_LOG(LogTemp, Error, TEXT("UTCPClientComponent: Failed to send data"));
		return false;
	}

	return true;
}

bool UTCPClientComponent::IsConnected() const
{
	return bIsConnected && SocketFD != -1;
}

void UTCPClientComponent::CloseConnection()
{
	if (SocketFD != -1)
	{
		close(SocketFD);
		SocketFD = -1;
		bIsConnected = false;
		UE_LOG(LogTemp, Log, TEXT("UTCPClientComponent: Connection closed by client"));
	}
}

void UTCPClientComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UTCPClientComponent::TickComponent(
	float DeltaTime,
	enum ELevelTick TickType,
	FActorComponentTickFunction * ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// If socket is connected, check for pending data and receive if needed
	if (bIsConnected && SocketFD != -1)
	{
		TArray<uint8> ReceivedData;
		if (TryReceiveData(ReceivedData, ReceiveBufferSize) > 0)
		{
			OnDataReceived.Broadcast(ReceivedData);
		}
	}
}

void UTCPClientComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	CloseConnection();
}

bool UTCPClientComponent::ConnectToSocket(const FString& IP, int32 Port)
{
	// Close connection if connected
	CloseConnection();

	// Socket creation
	SocketFD = socket(AF_INET, SOCK_STREAM, 0);
	if (SocketFD == -1)
	{
		UE_LOG(LogTemp, Error, TEXT("UTCPClientComponent: Failed to create socket"));
		return false;
	}
	
	/** 
	 * Switch to non-blocking mode
	*/
	// Test if the socket is in non-blocking mode:
	if(!(fcntl(SocketFD, F_GETFL) & O_NONBLOCK))
	{
		UE_LOG(LogTemp, Log, TEXT("UTCPClientComponent: Socket is not non-blocking. Changing to non-blocking..."));
		// socket is non-blocking
		// Put the socket in non-blocking mode:
		if(fcntl(SocketFD, F_SETFL, fcntl(SocketFD, F_GETFL) | O_NONBLOCK) < 0)
		{
			UE_LOG(LogTemp, Log, TEXT("UTCPClientComponent: Could not set socket to non-blocking. returning."));
			return false;
		}
	}

	// Setting up the connection
	struct sockaddr_in ServerAddress;
	FMemory::Memzero(&ServerAddress, sizeof(ServerAddress));
	ServerAddress.sin_family = AF_INET;
	ServerAddress.sin_port = htons(Port);
	inet_pton(AF_INET, TCHAR_TO_UTF8(*IP), &ServerAddress.sin_addr);

	// Connecting to the server
	int32 AttemptCount = 0;
	
	while (AttemptCount < MaxReconAttemptCount)
	{
		AttemptCount++;
		if(connect(SocketFD, (struct sockaddr*)&ServerAddress, sizeof(ServerAddress)) == 0)
		{
			break;
		}
		if (errno == EINPROGRESS)  // Connection is still in progress
		{
			UE_LOG(LogTemp, Log, TEXT("UTCPClientComponent: Connection in progress..."));
		}
		else
		{
			// Error occurred
			UE_LOG(LogTemp, Error, TEXT("UTCPClientComponent: Failed to connect to the server, error: %s"), UTF8_TO_TCHAR(strerror(errno)));
			if (!bReconnectOnFailure)
			{
				return false;
			}
		}

		FPlatformProcess::Sleep(1.0f);
	}

	bIsConnected = true;
	UE_LOG(LogTemp, Log, TEXT("UTCPClientComponent: Connected to %s:%d from %i attempt"), *IP, Port, AttemptCount);
	return true;
}

int32 UTCPClientComponent::TryReceiveData(TArray<uint8>& OutData, const int32 BufferSize)
{
	if (!bIsConnected || SocketFD == -1)
	{
		UE_LOG(LogTemp, Error, TEXT("UTCPClientComponent: Not connected to server"));
		return 0;
	}

	uint8 Buffer[BufferSize];
	ssize_t ReceivedBytes = recv(SocketFD, Buffer, sizeof(Buffer), 0);

	if (ReceivedBytes == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			// No readable data.
			// EAGAIN (or EWOULDBLOCK) happens because our socket is non-blocking: recv calls wait for a message to arrive
			return 0;
		}
		UE_LOG(LogTemp, Error, TEXT("UTCPClientComponent: Failed to receive data from server, error: %s"), UTF8_TO_TCHAR(strerror(errno)));
		return 0;
	}
	// From https://www.man7.org/linux/man-pages/man2/recv.2.html
	// we get 0 when stream socket disconnected 
	else if (ReceivedBytes == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("UTCPClientComponent: Connection closed by server"));
		bIsConnected = false;
		OnServerDisconnected.Broadcast(ServerIP, ServerPort);
		return 0;
	}
	// Add received bytes
	OutData.Append(Buffer, ReceivedBytes);
	
	return ReceivedBytes;
}
