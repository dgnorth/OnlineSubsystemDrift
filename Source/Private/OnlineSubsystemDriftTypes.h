// Copyright 2016-2017 Directive Games Limited - All Rights Reserved.

#pragma once

#include "OnlineSubsystemTypes.h"
#include "IPAddress.h"
#include "OnlineSubsystemDrift.h"

class FOnlineSubsystemDrift;

/**
* Implementation of session information
*/
class ONLINESUBSYSTEMDRIFT_API FOnlineSessionInfoDrift : public FOnlineSessionInfo
{
protected:

    /** Hidden on purpose */
    FOnlineSessionInfoDrift(const FOnlineSessionInfoDrift& Src)
    {
    }

    /** Hidden on purpose */
    FOnlineSessionInfoDrift& operator=(const FOnlineSessionInfoDrift& Src)
    {
        return *this;
    }

PACKAGE_SCOPE:

    /** Constructor */
    FOnlineSessionInfoDrift();

    /**
    * Initialize a Drift session info with the address of this machine
    * and an id for the session
    */
    void Init(const FOnlineSubsystemDrift& Subsystem);

    /** Unique Id for this session */
	FUniqueNetIdStringPtr SessionId;
    
    /** Server URL */
    FString Url;

public:

    virtual ~FOnlineSessionInfoDrift() {}

    bool operator==(const FOnlineSessionInfoDrift& Other) const
    {
        return false;
    }

    virtual const uint8* GetBytes() const override
    {
        return NULL;
    }

    virtual int32 GetSize() const override
    {
        return sizeof(uint64) + sizeof(TSharedPtr<class FInternetAddr>);
    }

    virtual bool IsValid() const override
    {
        return !Url.IsEmpty();
    }

    virtual FString ToString() const override
    {
        return SessionId.IsValid() ? SessionId->ToString() : FString();
    }

    virtual FString ToDebugString() const override
    {
        return Url.IsEmpty() ? TEXT("INVALID") : Url;
    }

    virtual const FUniqueNetId& GetSessionId() const override
    {
        return SessionId.IsValid() ? *SessionId.Get() : *FUniqueNetIdString::EmptyId();
    }
};

using DriftID = uint32;

typedef TSharedPtr<class FUniqueNetIdDrift> FUniqueNetIdDriftPtr;
typedef TSharedRef<class FUniqueNetIdDrift> FUniqueNetIdDriftRef;

class FUniqueNetIdDrift : public FUniqueNetId
{
public:
    FName GetType() const override
	{
		return DRIFT_SUBSYSTEM;
	}

    const uint8* GetBytes() const override
    {
        return (uint8*)&driftId_;
    }
    
    int32 GetSize() const override
    {
        return sizeof(uint32);
    }

    bool IsValid() const override
    {
        return driftId_ != 0;
    }
    
    FString ToString() const override
    {
        return FString::Printf(TEXT("%d"), driftId_);
    }
    
    FString ToDebugString() const override
    {
        return ToString();
    }
    
    friend uint32 GetTypeHash(const FUniqueNetIdDrift& other)
    {
        return other.driftId_;
    }

	DriftID GetId() const
    {
        return driftId_;
    }

	static DriftID ParseDriftId(const FUniqueNetId& Src)
	{
		if (ensure(Src.GetType() == DRIFT_SUBSYSTEM))
		{
			return static_cast<const FUniqueNetIdDrift&>(Src).driftId_;
		}
		return 0;
	}

	static DriftID ParseDriftId(const FString& Str)
	{
		return FCString::Atoi(*Str);
	}

	static FUniqueNetIdDriftRef EmptyId()
	{
		static const auto DummyId = MakeShareable(new FUniqueNetIdDrift());
		return DummyId;
	}

	template<class T>
	static FUniqueNetIdDriftRef Create(const T& Src)
	{
		return MakeShareable(new FUniqueNetIdDrift(Src));
	}

private:
	FUniqueNetIdDrift()
		: driftId_{ 0 }
	{
	}

	explicit FUniqueNetIdDrift(const FUniqueNetId& Src)
		: driftId_{ ParseDriftId(Src) }
	{
	}

	explicit FUniqueNetIdDrift(const FUniqueNetIdDrift& other)
		: driftId_{ other.driftId_ }
	{
	}

	FUniqueNetIdDrift(uint32 driftId)
		: driftId_{ driftId }
	{
	}

	explicit FUniqueNetIdDrift(const FString& Str)
		: driftId_{ ParseDriftId(Str) }
	{
	}

private:
	DriftID driftId_;
};
