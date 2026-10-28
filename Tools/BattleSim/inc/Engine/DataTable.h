#pragma once
#include "CoreMinimal.h"
struct FTableRowBase { virtual ~FTableRowBase() {} };
class UDataTable : public UObject
{
public:
	UScriptStruct* RowStruct = nullptr;
	std::map<FString, std::shared_ptr<FTableRowBase>> Rows;
	std::vector<FName> Order;
	template <typename T> void AddRow(FName Name, const T& Row) { if (!Rows.count(Name.Str)) Order.push_back(Name); Rows[Name.Str] = std::make_shared<T>(Row); }
	template <typename T> T* FindRow(FName Name, const TCHAR*, bool = true) const { auto It = Rows.find(Name.Str); return It == Rows.end() ? nullptr : static_cast<T*>(It->second.get()); }
	TArray<FName> GetRowNames() const { TArray<FName> R; for (auto& N : Order) R.Add(N); return R; }
};
