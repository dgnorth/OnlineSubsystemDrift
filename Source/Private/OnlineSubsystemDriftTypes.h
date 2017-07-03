// Copyright 2016-2017 Directive Games Limited - All Rights Reserved.

#pragma once

#include "OnlineSubsystemTypes.h"
#include "IPAddress.h"

class FOnlineSubsystemDrift;

/**
* Implementation of session information
*/
class FOnlineSessionInfoDrift : public FOnlineSessionInfo
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
    FUniqueNetIdString SessionId;
    
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
        return SessionId.ToString();
    }

    virtual FString ToDebugString() const override
    {
        return Url.IsEmpty() ? TEXT("INVALID") : Url;
    }

    virtual const FUniqueNetId& GetSessionId() const override
    {
        return SessionId;
    }
};

using DriftID = uint32;

class FUniqueNetIdDrift : public FUniqueNetId
{
public:
    FUniqueNetIdDrift()
    : driftId_{ 0 }
    {
    }

    explicit FUniqueNetIdDrift(const FUniqueNetId& Src)
    {
        if (Src.GetSize() == sizeof(driftId_))
        {
            driftId_ = static_cast<const FUniqueNetIdDrift&>(Src).driftId_;
        }
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
    : driftId_(FCString::Atoi(*Str))
    {
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
        return ::PointerHash(&other.driftId_, sizeof(other.driftId_));
    }

    uint32 GetId() const
    {
        return driftId_;
    }

private:
    uint32 driftId_;
};
