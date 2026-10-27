#pragma once
#include "CoreMinimal.h"
struct FSubsystemCollectionBase {};
class UGameInstanceSubsystem : public UObject { public: virtual void Initialize(FSubsystemCollectionBase&) {} };
template <typename T> class TSoftObjectPtr { public: bool IsNull() const { return true; } T* LoadSynchronous() const { return nullptr; } };
