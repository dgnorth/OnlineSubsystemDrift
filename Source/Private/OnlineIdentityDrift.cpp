// Copyright 2016-2017 Directive Games Limited - All Rights Reserved.

#include "OnlineSubsystemDriftPrivatePCH.h"
#include "OnlineIdentityDrift.h"
#include "OnlineSubsystemDrift.h"

#include "DriftAPI.h"

bool FUserOnlineAccountDrift::GetAuthAttribute(const FString& AttrName, FString& OutAttrValue) const
{
    const FString* FoundAttr = AdditionalAuthData.Find(AttrName);
    if (FoundAttr != nullptr)
    {
        OutAttrValue = *FoundAttr;
        return true;
    }
    return false;
}

bool FUserOnlineAccountDrift::GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const
{
    const FString* FoundAttr = UserAttributes.Find(AttrName);
    if (FoundAttr != nullptr)
    {
        OutAttrValue = *FoundAttr;
        return true;
    }
    return false;
}

bool FUserOnlineAccountDrift::SetUserAttribute(const FString& AttrName, const FString& AttrValue)
{
    const FString* FoundAttr = UserAttributes.Find(AttrName);
    if (FoundAttr == nullptr || *FoundAttr != AttrValue)
    {
        UserAttributes.Add(AttrName, AttrValue);
        return true;
    }
    return false;
}

bool FOnlineIdentityDrift::Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
    FString ErrorStr;
    TSharedPtr<FUserOnlineAccountDrift> UserAccountPtr;

    if (LocalUserNum < 0 || LocalUserNum >= MAX_LOCAL_PLAYERS)
    {
        ErrorStr = FString::Printf(TEXT("Invalid LocalUserNum=%d"), LocalUserNum);
    }
    else
    {
        TSharedPtr<const FUniqueNetId>* UserId = UserIds.Find(LocalUserNum);
        if (UserId == nullptr)
        {
            if (auto Drift = DriftSubsystem->GetDrift())
            {
                Drift->AuthenticatePlayer();
                return true;
            }
            else
            {
                ErrorStr = TEXT("Failed to access Drift plugin");
            }
        }
        else
        {
            const FUniqueNetIdDrift* UniqueIdStr = (FUniqueNetIdDrift*)(UserId->Get());
            TSharedRef<FUserOnlineAccountDrift>* TempPtr = UserAccounts.Find(*UniqueIdStr);
            check(TempPtr);
            UserAccountPtr = *TempPtr;
        }
    }

    if (!ErrorStr.IsEmpty())
    {
        UE_LOG_ONLINE(Warning, TEXT("Login request failed. %s"), *ErrorStr);

        TriggerOnLoginCompleteDelegates(LocalUserNum, false, FUniqueNetIdDrift{}, ErrorStr);
        return false;
    }

    TriggerOnLoginCompleteDelegates(LocalUserNum, true, *UserAccountPtr->GetUserId(), ErrorStr);
    return true;
}


void FOnlineIdentityDrift::OnAuthenticated(bool success, const FPlayerAuthenticatedInfo& info)
{
    FString ErrorStr;
    int32 LocalUserNum = 0;  // TODO: Support multi-player

    if (success)
    {
        FUniqueNetIdDrift* NewUserId = new FUniqueNetIdDrift(info.playerId);
        TSharedRef<FUserOnlineAccountDrift> UserAccountRef(new FUserOnlineAccountDrift(MakeShareable(NewUserId), info.playerName));
        
        UserAccounts.Add(static_cast<FUniqueNetIdDrift>(*UserAccountRef->GetUserId()), UserAccountRef);
        
        UserIds.Add(LocalUserNum, UserAccountRef->GetUserId());
        
        TriggerOnLoginCompleteDelegates(LocalUserNum, true, *UserAccountRef->GetUserId(), *ErrorStr);
        TriggerOnLoginStatusChangedDelegates(LocalUserNum, ELoginStatus::NotLoggedIn, ELoginStatus::LoggedIn, *UserAccountRef->GetUserId());
        return;
    }
    else
    {
        ErrorStr = info.error;
    }
    
    if (!ErrorStr.IsEmpty())
    {
        UE_LOG_ONLINE(Warning, TEXT("Login request failed. %s"), *info.error);

        TriggerOnLoginCompleteDelegates(LocalUserNum, false, FUniqueNetIdDrift(), info.error);
    }
}


void FOnlineIdentityDrift::OnServerRegistered(bool success)
{
    if (success)
    {
        TriggerOnLoginCompleteDelegates(0, true, FUniqueNetIdDrift{}, TEXT(""));
        return;
    }

    UE_LOG_ONLINE(Warning, TEXT("Server registration failed"));
    TriggerOnLoginCompleteDelegates(0, false, FUniqueNetIdDrift{}, TEXT("Unknown failure"));
}


bool FOnlineIdentityDrift::Logout(int32 LocalUserNum)
{
    // TODO: Actually logout?

    TSharedPtr<const FUniqueNetId> UserId = GetUniquePlayerId(LocalUserNum);
    if (UserId.IsValid())
    {
        UserAccounts.Remove(FUniqueNetIdDrift(*UserId));
        UserIds.Remove(LocalUserNum);
        TriggerOnLogoutCompleteDelegates(LocalUserNum, true);

        return true;
    }
    else
    {
        UE_LOG_ONLINE(Warning, TEXT("No logged in user found for LocalUserNum=%d."), LocalUserNum);
        TriggerOnLogoutCompleteDelegates(LocalUserNum, false);
    }
    return false;
}

bool FOnlineIdentityDrift::AutoLogin(int32 LocalUserNum)
{
    if (IsRunningDedicatedServer())
    {
        if (GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
        {
            return LoginDedicatedServer();
        }
        return false;
    }
    
    FString Type, ID, Token;

    return Login(LocalUserNum, FOnlineAccountCredentials{Type, ID, Token});
}

bool FOnlineIdentityDrift::LoginDedicatedServer()
{
    if (auto Drift = DriftSubsystem->GetDrift())
    {
        return Drift->RegisterServer();
    }
    return false;
}

TSharedPtr<FUserOnlineAccount> FOnlineIdentityDrift::GetUserAccount(const FUniqueNetId& UserId) const
{
    TSharedPtr<FUserOnlineAccount> Result;

    FUniqueNetIdDrift DriftUserId(UserId);
    auto FoundUserAccount = UserAccounts.Find(DriftUserId);
    if (FoundUserAccount != nullptr)
    {
        Result = *FoundUserAccount;
    }
    return Result;
}

TArray<TSharedPtr<FUserOnlineAccount> > FOnlineIdentityDrift::GetAllUserAccounts() const
{
    TArray<TSharedPtr<FUserOnlineAccount> > Result;
    
    for (TMap<FUniqueNetIdDrift, TSharedRef<FUserOnlineAccountDrift>>::TConstIterator It(UserAccounts); It; ++It)
    {
        Result.Add(It.Value());
    }
    
    return Result;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityDrift::GetUniquePlayerId(int32 LocalUserNum) const
{
    const TSharedPtr<const FUniqueNetId>* FoundId = UserIds.Find(LocalUserNum);
    if (FoundId != nullptr)
    {
        return *FoundId;
    }
    return nullptr;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityDrift::CreateUniquePlayerId(uint8* Bytes, int32 Size)
{
    if (Bytes != nullptr && Size == sizeof(DriftID))
    {
        return MakeShareable(new FUniqueNetIdDrift(*reinterpret_cast<uint32*>(Bytes)));
    }
    return nullptr;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityDrift::CreateUniquePlayerId(const FString& Str)
{
    return MakeShareable(new FUniqueNetIdDrift(Str));
}

ELoginStatus::Type FOnlineIdentityDrift::GetLoginStatus(int32 LocalUserNum) const
{
    if (auto drift = DriftSubsystem->GetDrift())
    {
        return drift->GetConnectionState() == EDriftConnectionState::Connected ? ELoginStatus::LoggedIn : ELoginStatus::NotLoggedIn;
    }
    return ELoginStatus::NotLoggedIn;
}

ELoginStatus::Type FOnlineIdentityDrift::GetLoginStatus(const FUniqueNetId& UserId) const 
{
    return GetLoginStatus(0);
}

FString FOnlineIdentityDrift::GetPlayerNickname(int32 LocalUserNum) const
{
    TSharedPtr<const FUniqueNetId> UniqueId = GetUniquePlayerId(LocalUserNum);
    if (UniqueId.IsValid())
    {
        return GetPlayerNickname(*UniqueId);
    }

    return TEXT("");
}

FString FOnlineIdentityDrift::GetPlayerNickname(const FUniqueNetId& UserId) const
{
    auto UserAccount = GetUserAccount(UserId);
    if (UserAccount.IsValid())
    {
        return UserAccount->GetDisplayName();
    }

    return TEXT("DRIFT USER");
}

FString FOnlineIdentityDrift::GetAuthToken(int32 LocalUserNum) const
{
    // TODO: Implement?
    return FString();
}


void FOnlineIdentityDrift::RevokeAuthToken(const FUniqueNetId& UserId, const FOnRevokeAuthTokenCompleteDelegate& Delegate)
{
}


FOnlineIdentityDrift::FOnlineIdentityDrift(class FOnlineSubsystemDrift* InSubsystem)
: DriftSubsystem(InSubsystem)
{
    if (auto Drift = DriftSubsystem->GetDrift())
    {
        onAuthenticatedHandle = Drift->OnPlayerAuthenticated().AddRaw(this, &FOnlineIdentityDrift::OnAuthenticated);
        onServerRegisteredHandle = Drift->OnServerRegistered().AddRaw(this, &FOnlineIdentityDrift::OnServerRegistered);
    }
}

FOnlineIdentityDrift::FOnlineIdentityDrift()
{
}

FOnlineIdentityDrift::~FOnlineIdentityDrift()
{
    if (auto Drift = DriftSubsystem ? DriftSubsystem->GetDrift() : nullptr)
    {
        Drift->OnServerRegistered().Remove(onServerRegisteredHandle);
        onServerRegisteredHandle.Reset();

        Drift->OnPlayerAuthenticated().Remove(onAuthenticatedHandle);
        onAuthenticatedHandle.Reset();
    }
}

void FOnlineIdentityDrift::GetUserPrivilege(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate)
{
    Delegate.ExecuteIfBound(UserId, Privilege, (uint32)EPrivilegeResults::NoFailures);
}

FPlatformUserId FOnlineIdentityDrift::GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& UniqueNetId) const
{
    for (int i = 0; i < MAX_LOCAL_PLAYERS; ++i)
    {
        auto CurrentUniqueId = GetUniquePlayerId(i);
        if (CurrentUniqueId.IsValid() && (*CurrentUniqueId == UniqueNetId))
        {
            return i;
        }
    }

    return PLATFORMUSERID_NONE;
}

FString FOnlineIdentityDrift::GetAuthType() const
{
    return TEXT("");
}
