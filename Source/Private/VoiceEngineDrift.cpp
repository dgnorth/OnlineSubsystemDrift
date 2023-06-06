// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "VoiceEngineDrift.h"

#include "OnlineSubsystem.h"
#include "VoiceInterfaceDrift.h"

#include "Engine.h"
#include "SoundDefinitions.h"
#include "Runtime/Engine/Classes/Sound/SoundWaveProcedural.h"
#include "OnlineSubsystemUtils.h"


#ifndef DEFAULT_NUM_VOICE_CHANNELS
#define DEFAULT_NUM_VOICE_CHANNELS 1
#endif


/** Largest size preallocated for compressed data */
#define MAX_COMPRESSED_VOICE_BUFFER_SIZE 8 * 1024
/** Largest size preallocated for uncompressed data */
#define MAX_UNCOMPRESSED_VOICE_BUFFER_SIZE 22 * 1024
/** Largest size allowed to carry over into next buffer */
#define MAX_VOICE_REMAINDER_SIZE 1 * 1024

FRemoteTalkerDataDrift::FRemoteTalkerDataDrift() :
	LastSeen(0.0),
	AudioComponent(nullptr),
	VoiceDecoder(nullptr)
{
	VoiceDecoder = FVoiceModule::Get().CreateVoiceDecoder();
	check(VoiceDecoder.IsValid());
}

FRemoteTalkerDataDrift::~FRemoteTalkerDataDrift()
{
	VoiceDecoder = nullptr;
}

FVoiceEngineDrift::FVoiceEngineDrift(IOnlineSubsystem* InSubsystem) :
	OnlineSubsystem(InSubsystem),
	VoiceCapture(nullptr),
	VoiceEncoder(nullptr),
	OwningUserIndex(INVALID_INDEX),
	UncompressedBytesAvailable(0),
	CompressedBytesAvailable(0),
	AvailableVoiceResult(EVoiceCaptureState::UnInitialized),
	bPendingFinalCapture(false),
	bIsCapturing(false),
	SerializeHelper(nullptr)
{
}

FVoiceEngineDrift::~FVoiceEngineDrift()
{
	if (bIsCapturing)
	{
		VoiceCapture->Stop();
	}

	VoiceCapture = nullptr;
	VoiceEncoder = nullptr;

	delete SerializeHelper;
}

void FVoiceEngineDrift::VoiceCaptureUpdate() const
{
	if (bPendingFinalCapture)
	{
		uint32 CompressedSize;
		const EVoiceCaptureState::Type RecordingState = VoiceCapture->GetCaptureState(CompressedSize);

		// If no data is available, we have finished capture the last (post-StopRecording) half-second of voice data
		if (RecordingState == EVoiceCaptureState::NotCapturing)
		{
			UE_LOG(LogVoiceEncode, Log, TEXT("Internal voice capture complete."));

			bPendingFinalCapture = false;

			// If a new recording session has begun since the call to 'StopRecording', kick that off
			if (bIsCapturing)
			{
				StartRecording();
			}
			else
			{
				// Marks that recording has successfully stopped
				StoppedRecording();
			}
		}
	}
}

void FVoiceEngineDrift::StartRecording() const
{
	UE_LOG(LogVoiceEncode, VeryVerbose, TEXT("VOIP StartRecording"));
	if (VoiceCapture.IsValid())
	{
		if (!VoiceCapture->Start())
		{
			UE_LOG(LogVoiceEncode, Warning, TEXT("Failed to start voice recording"));
		}
	}
}

void FVoiceEngineDrift::StopRecording() const
{
	UE_LOG(LogVoiceEncode, VeryVerbose, TEXT("VOIP StopRecording"));
	if (VoiceCapture.IsValid())
	{
		VoiceCapture->Stop();
	}
}

void FVoiceEngineDrift::StoppedRecording() const
{
	UE_LOG(LogVoiceEncode, VeryVerbose, TEXT("VOIP StoppedRecording"));
}

bool FVoiceEngineDrift::Init(int32 MaxLocalTalkers, int32 MaxRemoteTalkers)
{
	bool bSuccess = false;

	if (!OnlineSubsystem->IsDedicated())
	{
		bool bHasVoiceEnabled = false;
		if (GConfig->GetBool(TEXT("OnlineSubsystem"),TEXT("bHasVoiceEnabled"), bHasVoiceEnabled, GEngineIni) && bHasVoiceEnabled)
		{
			FVoiceModule& VoiceModule = FVoiceModule::Get();
			if (VoiceModule.IsVoiceEnabled())
			{
				VoiceCapture = VoiceModule.CreateVoiceCapture(TEXT(""));
				VoiceEncoder = VoiceModule.CreateVoiceEncoder();

				bSuccess = VoiceCapture.IsValid() && VoiceEncoder.IsValid();
				if (bSuccess)
				{
					CompressedVoiceBuffer.Empty(MAX_COMPRESSED_VOICE_BUFFER_SIZE);
					DecompressedVoiceBuffer.Empty(MAX_UNCOMPRESSED_VOICE_BUFFER_SIZE);

					for (int32 TalkerIdx=0; TalkerIdx < MaxLocalTalkers; TalkerIdx++)
					{
						PlayerVoiceData[TalkerIdx].VoiceRemainderSize = 0;
						PlayerVoiceData[TalkerIdx].VoiceRemainder.Empty(MAX_VOICE_REMAINDER_SIZE);
					}
				}
				else
				{
					UE_LOG(LogVoiceEncode, Warning, TEXT("Voice capture initialization failed!"));
				}
			}
		}
		else
		{
			UE_LOG(LogVoiceEncode, Warning, TEXT("Voice interface disabled by config"));
		}
	}

	return bSuccess;
}

uint32 FVoiceEngineDrift::StartLocalVoiceProcessing(uint32 LocalUserNum)
{
	uint32 Return = ONLINE_FAIL;
	if (IsOwningUser(LocalUserNum))
	{
		if (!bIsCapturing)
		{
			// Update the current recording state, if VOIP data was still being read
			VoiceCaptureUpdate();

			if (!IsRecording())
			{
				StartRecording();
			}

			bIsCapturing = true;
		}

		Return = ONLINE_SUCCESS;
	}
	else
	{
		UE_CLOG(OwningUserIndex != -1, LogVoiceEncode, Error, TEXT("StartLocalVoiceProcessing(): Device is currently owned by another user"));
	}

	return Return;
}

uint32 FVoiceEngineDrift::StopLocalVoiceProcessing(uint32 LocalUserNum)
{
	uint32 Return = ONLINE_FAIL;
	if (IsOwningUser(LocalUserNum))
	{
		if (bIsCapturing)
		{
			bIsCapturing = false;
			bPendingFinalCapture = true;

			// Make a call to begin stopping the current VOIP recording session
			StopRecording();

			// Now check/update the status of the recording session
			VoiceCaptureUpdate();
		}

		Return = ONLINE_SUCCESS;
	}
	else
	{
		UE_CLOG(OwningUserIndex != -1, LogVoiceEncode, Error, TEXT("StopLocalVoiceProcessing: Ignoring stop request for non-owning user"));
	}

	return Return;
}

uint32 FVoiceEngineDrift::UnregisterRemoteTalker(const FUniqueNetId& UniqueId)
{
	const auto RemoteTalkerId = FUniqueNetIdDrift::ParseDriftId(UniqueId);
	FRemoteTalkerDataDrift* RemoteData = RemoteTalkerBuffers.Find(RemoteTalkerId);
	if (RemoteData != nullptr)
	{
		// Dump the whole talker
		if (RemoteData->AudioComponent)
		{
			RemoteData->AudioComponent->Stop();
			RemoteData->AudioComponent = nullptr;
		}

		RemoteTalkerBuffers.Remove(RemoteTalkerId);
	}

	return ONLINE_SUCCESS;
}

uint32 FVoiceEngineDrift::GetVoiceDataReadyFlags() const
{
	// First check and update the internal state of VOIP recording
	VoiceCaptureUpdate();
	if (OwningUserIndex != INVALID_INDEX && IsRecording())
	{
		// Check if there is new data available via the Voice API
		if (AvailableVoiceResult == EVoiceCaptureState::Ok && UncompressedBytesAvailable > 0)
		{
			return 1 << OwningUserIndex;
		}
	}

	return 0;
}

uint32 FVoiceEngineDrift::ReadLocalVoiceData(uint32 LocalUserNum, uint8* Data, uint32* Size)
{
	check(*Size > 0);

	// Before doing anything, check/update the current recording state
	VoiceCaptureUpdate();

	// Return data even if not capturing, possibly have data during stopping
	if (IsOwningUser(LocalUserNum) && IsRecording())
	{
		DecompressedVoiceBuffer.Empty(MAX_UNCOMPRESSED_VOICE_BUFFER_SIZE);
		CompressedVoiceBuffer.Empty(MAX_COMPRESSED_VOICE_BUFFER_SIZE);

		uint32 NewVoiceDataBytes = 0;
		EVoiceCaptureState::Type VoiceResult = VoiceCapture->GetCaptureState(NewVoiceDataBytes);
		if (VoiceResult != EVoiceCaptureState::Ok && VoiceResult != EVoiceCaptureState::NoData)
		{
			UE_LOG(LogVoiceEncode, Warning, TEXT("ReadLocalVoiceData: GetAvailableVoice failure: VoiceResult: %s"), EVoiceCaptureState::ToString(VoiceResult));
			return ONLINE_FAIL;
		}

 		if (NewVoiceDataBytes == 0)
 		{
 			UE_LOG(LogVoiceEncode, VeryVerbose, TEXT("ReadLocalVoiceData: No Data: VoiceResult: %s"), EVoiceCaptureState::ToString(VoiceResult));
 			*Size = 0;
 			return ONLINE_SUCCESS;
 		}

		// Make space for new and any previously remaining data
		uint32 TotalVoiceBytes = NewVoiceDataBytes + PlayerVoiceData[LocalUserNum].VoiceRemainderSize;
		if (TotalVoiceBytes > MAX_UNCOMPRESSED_VOICE_BUFFER_SIZE)
		{
			UE_LOG(LogVoiceEncode, Warning, TEXT("Exceeded uncompressed voice buffer size, clamping"))
			TotalVoiceBytes = MAX_UNCOMPRESSED_VOICE_BUFFER_SIZE;
		}

		DecompressedVoiceBuffer.AddUninitialized(TotalVoiceBytes);

		if (PlayerVoiceData[LocalUserNum].VoiceRemainderSize > 0)
		{
			FMemory::Memcpy(DecompressedVoiceBuffer.GetData(), PlayerVoiceData[LocalUserNum].VoiceRemainder.GetData(), PlayerVoiceData[LocalUserNum].VoiceRemainderSize);
		}

		// Get new uncompressed data
		VoiceResult = VoiceCapture->GetVoiceData(DecompressedVoiceBuffer.GetData() + PlayerVoiceData[LocalUserNum].VoiceRemainderSize,
			NewVoiceDataBytes, NewVoiceDataBytes);

		TotalVoiceBytes = NewVoiceDataBytes + PlayerVoiceData[LocalUserNum].VoiceRemainderSize;

		if (VoiceResult == EVoiceCaptureState::Ok && TotalVoiceBytes > 0)
		{
			CompressedBytesAvailable = MAX_COMPRESSED_VOICE_BUFFER_SIZE;
			CompressedVoiceBuffer.AddUninitialized(MAX_COMPRESSED_VOICE_BUFFER_SIZE);

			PlayerVoiceData[LocalUserNum].VoiceRemainderSize =
				VoiceEncoder->Encode(DecompressedVoiceBuffer.GetData(), TotalVoiceBytes, CompressedVoiceBuffer.GetData(), CompressedBytesAvailable);

			// Save off any unencoded remainder
			if (PlayerVoiceData[LocalUserNum].VoiceRemainderSize > 0)
			{
				if (PlayerVoiceData[LocalUserNum].VoiceRemainderSize > MAX_VOICE_REMAINDER_SIZE)
				{
					UE_LOG(LogVoiceEncode, Warning, TEXT("Exceeded voice remainder buffer size, clamping"))
						PlayerVoiceData[LocalUserNum].VoiceRemainderSize = MAX_VOICE_REMAINDER_SIZE;
				}

				PlayerVoiceData[LocalUserNum].VoiceRemainder.AddUninitialized(MAX_VOICE_REMAINDER_SIZE);
				FMemory::Memcpy(PlayerVoiceData[LocalUserNum].VoiceRemainder.GetData(), DecompressedVoiceBuffer.GetData() + (TotalVoiceBytes - PlayerVoiceData[LocalUserNum].VoiceRemainderSize), PlayerVoiceData[LocalUserNum].VoiceRemainderSize);
			}

			static double LastGetVoiceCallTime = 0.0;
			double CurTime = FPlatformTime::Seconds();
			double TimeSinceLastCall = LastGetVoiceCallTime > 0 ? (CurTime - LastGetVoiceCallTime) : 0.0;
			LastGetVoiceCallTime = CurTime;

			UE_LOG(LogVoiceEncode, Log, TEXT("ReadLocalVoiceData: GetVoice: Result: %s, Available: %i, LastCall: %0.3f"), EVoiceCaptureState::ToString(VoiceResult), CompressedBytesAvailable, TimeSinceLastCall);
			if (CompressedBytesAvailable > 0)
			{
				*Size = FMath::Min<int32>(*Size, CompressedBytesAvailable);
				FMemory::Memcpy(Data, CompressedVoiceBuffer.GetData(), *Size);

				UE_LOG(LogVoiceEncode, VeryVerbose, TEXT("ReadLocalVoiceData: Size: %d"), *Size);
				return ONLINE_SUCCESS;
			}
			else
			{
				*Size = 0;
				CompressedVoiceBuffer.Empty(MAX_COMPRESSED_VOICE_BUFFER_SIZE);

				UE_LOG(LogVoiceEncode, Warning, TEXT("ReadLocalVoiceData: GetVoice failure: VoiceResult: %s"), EVoiceCaptureState::ToString(VoiceResult));
				return ONLINE_FAIL;
			}
		}
	}

	return ONLINE_FAIL;
}

uint32 FVoiceEngineDrift::SubmitRemoteVoiceData(const FUniqueNetId& RemoteTalkerId, uint8* Data, uint32* Size)
{
	UE_LOG(LogVoiceDecode, VeryVerbose, TEXT("SubmitRemoteVoiceData(%s) Size: %d received!"), *RemoteTalkerId.ToDebugString(), *Size);

	const auto TalkerId = FUniqueNetIdDrift::ParseDriftId(RemoteTalkerId);
	FRemoteTalkerDataDrift& QueuedData = RemoteTalkerBuffers.FindOrAdd(TalkerId);

	// new voice packet.
	QueuedData.LastSeen = FPlatformTime::Seconds();

	uint32 BytesWritten = MAX_UNCOMPRESSED_VOICE_BUFFER_SIZE;

	DecompressedVoiceBuffer.Empty(MAX_UNCOMPRESSED_VOICE_BUFFER_SIZE);
	DecompressedVoiceBuffer.AddUninitialized(MAX_UNCOMPRESSED_VOICE_BUFFER_SIZE);
	QueuedData.VoiceDecoder->Decode(Data, *Size, DecompressedVoiceBuffer.GetData(), BytesWritten);

	// If there is no data, return
	if (BytesWritten <= 0)
	{
		*Size = 0;
		return ONLINE_SUCCESS;
	}

	bool bAudioComponentCreated = false;
	// Generate a streaming wave audio component for voice playback
	if (!IsValid(QueuedData.AudioComponent))
	{
		if (SerializeHelper == nullptr)
		{
			SerializeHelper = new FVoiceSerializeHelper(this);
		}

		QueuedData.AudioComponent = CreateVoiceAudioComponent(VOICE_SAMPLE_RATE, DEFAULT_NUM_VOICE_CHANNELS);
		if (QueuedData.AudioComponent)
		{
			QueuedData.AudioComponent->OnAudioFinishedNative.AddRaw(this, &FVoiceEngineDrift::OnAudioFinished);
		}
	}

	if (QueuedData.AudioComponent != nullptr)
	{
		if (!QueuedData.AudioComponent->IsActive())
		{
			QueuedData.AudioComponent->Play();
		}

		USoundWaveProcedural* SoundStreaming = CastChecked<USoundWaveProcedural>(QueuedData.AudioComponent->Sound);

		if (0)
		{
			if (!bAudioComponentCreated && SoundStreaming->GetAvailableAudioByteCount() == 0)
			{
				UE_LOG(LogVoiceDecode, Log, TEXT("VOIP audio component was starved!"));
			}
		}

		SoundStreaming->QueueAudio(DecompressedVoiceBuffer.GetData(), BytesWritten);
	}

	return ONLINE_SUCCESS;
}

void FVoiceEngineDrift::TickTalkers(float DeltaTime)
{
	// Remove users that are done talking.
	const double CurTime = FPlatformTime::Seconds();
	for (FRemoteTalkerData::TIterator It(RemoteTalkerBuffers); It; ++It)
	{
		FRemoteTalkerDataDrift& RemoteData = It.Value();
		double TimeSince = CurTime - RemoteData.LastSeen;
		if (TimeSince >= 1.0)
		{
			// Dump the whole talker
			if (RemoteData.AudioComponent)
			{
				RemoteData.AudioComponent->Stop();
			}
		}
	}
}

void FVoiceEngineDrift::Tick(float DeltaTime)
{
	// Check available voice once a frame, this value changes after calling GetVoiceData()
	AvailableVoiceResult = VoiceCapture->GetCaptureState(UncompressedBytesAvailable);

	TickTalkers(DeltaTime);
}

void FVoiceEngineDrift::OnAudioFinished(UAudioComponent* AC)
{
	for (FRemoteTalkerData::TIterator It(RemoteTalkerBuffers); It; ++It)
	{
		FRemoteTalkerDataDrift& RemoteData = It.Value();
		if (!IsValid(RemoteData.AudioComponent) || AC == RemoteData.AudioComponent)
		{
			UE_LOG(LogVoiceDecode, Log, TEXT("Removing VOIP AudioComponent for Id: %d"), It.Key());
			RemoteData.AudioComponent = nullptr;
			break;
		}
	}
	UE_LOG(LogVoiceDecode, Verbose, TEXT("Audio Finished"));
}

FString FVoiceEngineDrift::GetVoiceDebugState() const
{
	FString Output;
	Output = FString::Printf(TEXT("IsRecording: %d\n DataReady: 0x%08x State:%s\n UncompressedBytes: %d\n CompressedBytes: %d\n"),
		IsRecording(),
		GetVoiceDataReadyFlags(),
		EVoiceCaptureState::ToString(AvailableVoiceResult),
		UncompressedBytesAvailable,
		CompressedBytesAvailable
		);

	// Add remainder size
	for (int32 Idx=0; Idx < MAX_SPLITSCREEN_TALKERS; Idx++)
	{
		Output += FString::Printf(TEXT("Remainder[%d] %d\n"), Idx, PlayerVoiceData[Idx].VoiceRemainderSize);
	}

	return Output;
}


