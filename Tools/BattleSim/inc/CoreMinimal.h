// Faux moteur minimal pour compiler et lancer la logique du combat hors d'Unreal.
#pragma once
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <algorithm>
#include <functional>
#include <cmath>
#include <cstdint>
#include <cwchar>
#include <cstdarg>
#include <cstdio>
#include <optional>
#include <initializer_list>
#include <type_traits>

typedef int32_t int32; typedef uint8_t uint8; typedef int64_t int64; typedef uint32_t uint32; typedef wchar_t TCHAR;
#define TEXT(x) L##x
#define UCLASS(...)
#define USTRUCT(...)
#define UENUM(...)
#define UMETA(...)
#define UPROPERTY(...)
#define UFUNCTION(...)
#define UPARAM(...)
struct UScriptStruct { };
#define GENERATED_BODY() public: static UScriptStruct* StaticStruct() { static UScriptStruct S; return &S; }
#define UE_LOG(...) do {} while (0)
#define check(x) do {} while (0)
#define INDEX_NONE (-1)
#define WITH_DEV_AUTOMATION_TESTS 1
template <typename T> std::remove_reference_t<T>&& MoveTemp(T&& V) { return static_cast<std::remove_reference_t<T>&&>(V); }

class FString
{
public:
	std::wstring S;
	FString() {}
	FString(const TCHAR* C) : S(C ? C : L"") {}
	FString(const std::wstring& W) : S(W) {}
	const TCHAR* operator*() const { return S.c_str(); }
	bool IsEmpty() const { return S.empty(); }
	int32 Len() const { return (int32)S.size(); }
	TCHAR& operator[](int32 I) { return S[I]; }
	const TCHAR& operator[](int32 I) const { return S[I]; }
	bool FindChar(TCHAR C, int32& Out) const { size_t P = S.find(C); Out = P == std::wstring::npos ? INDEX_NONE : (int32)P; return P != std::wstring::npos; }
	FString operator+(const FString& O) const { return FString(S + O.S); }
	FString& operator+=(const FString& O) { S += O.S; return *this; }
	friend FString operator+(const TCHAR* A, const FString& B) { return FString(std::wstring(A) + B.S); }
	bool operator==(const FString& O) const { return S == O.S; }
	bool operator<(const FString& O) const { return S < O.S; }
	bool StartsWith(const FString& P) const { return S.rfind(P.S, 0) == 0; }
	FString Replace(const TCHAR* From, const TCHAR* To) const
	{
		std::wstring R = S, F = From, T = To; size_t Pos = 0;
		while (!F.empty() && (Pos = R.find(F, Pos)) != std::wstring::npos) { R.replace(Pos, F.size(), T); Pos += T.size(); }
		return FString(R);
	}
	FString TrimStartAndEnd() const
	{
		size_t A = S.find_first_not_of(L" \t\r\n"), B = S.find_last_not_of(L" \t\r\n");
		return A == std::wstring::npos ? FString() : FString(S.substr(A, B - A + 1));
	}
	static FString FromInt(int32 V) { return FString(std::to_wstring(V)); }
	static FString Printf(const TCHAR* Fmt, ...)
	{
		std::wstring F = Fmt, Out; // %s (TCHAR*) -> %ls
		for (size_t I = 0; I < F.size(); ++I) { Out += F[I]; if (F[I] == L'%' && I + 1 < F.size() && F[I + 1] == L's') Out += L'l'; }
		wchar_t Buf[4096]; va_list Args; va_start(Args, Fmt); vswprintf(Buf, 4096, Out.c_str(), Args); va_end(Args);
		return FString(Buf);
	}
	std::string Narrow() const { std::string R; for (wchar_t C : S) R += C < 128 ? (char)C : '?'; return R; }
};
struct FChar
{
	static TCHAR ToUpper(TCHAR C) { return (TCHAR)towupper(C); }
	static bool IsWhitespace(TCHAR C) { return iswspace(C) != 0; }
};

class FName
{
public:
	FString Str;
	FName() {}
	FName(const TCHAR* C) : Str(C) {}
	explicit FName(const FString& S) : Str(S) {}
	bool IsNone() const { return Str.IsEmpty() || Str.S == L"None"; }
	FString ToString() const { return IsNone() ? FString(L"None") : Str; }
	bool operator==(const FName& O) const { return (IsNone() && O.IsNone()) || Str.S == O.Str.S; }
	bool operator!=(const FName& O) const { return !(*this == O); }
	bool operator<(const FName& O) const { return Str.S < O.Str.S; }
};
static const FName NAME_None;

class FText
{
public:
	FString S;
	static FText FromString(const FString& In) { FText T; T.S = In; return T; }
	static FText FromName(const FName& N) { return FromString(N.ToString()); }
	static const FText& GetEmpty() { static FText E; return E; }
	FString ToString() const { return S; }
	bool IsEmpty() const { return S.IsEmpty(); }
};

template <typename T> class TArray
{
public:
	std::vector<T> V;
	TArray() {}
	TArray(std::initializer_list<T> L) : V(L) {}
	TArray& operator=(std::initializer_list<T> L) { V = L; return *this; }
	int32 Num() const { return (int32)V.size(); }
	bool IsEmpty() const { return V.empty(); }
	bool IsValidIndex(int32 I) const { return I >= 0 && I < Num(); }
	T& operator[](int32 I) { return V[I]; }
	const T& operator[](int32 I) const { return V[I]; }
	int32 Add(const T& X) { V.push_back(X); return Num() - 1; }
	int32 AddUnique(const T& X) { int32 I = Find(X); return I != INDEX_NONE ? I : Add(X); }
	T& AddDefaulted_GetRef() { V.emplace_back(); return V.back(); }
	int32 Find(const T& X) const { for (int32 I = 0; I < Num(); ++I) if (V[I] == X) return I; return INDEX_NONE; }
	bool Contains(const T& X) const { return Find(X) != INDEX_NONE; }
	template <typename P> bool ContainsByPredicate(P Pred) const { return IndexOfByPredicate(Pred) != INDEX_NONE; }
	template <typename P> int32 IndexOfByPredicate(P Pred) const { for (int32 I = 0; I < Num(); ++I) if (Pred(V[I])) return I; return INDEX_NONE; }
	template <typename P> T* FindByPredicate(P Pred) { int32 I = IndexOfByPredicate(Pred); return I == INDEX_NONE ? nullptr : &V[I]; }
	template <typename P> const T* FindByPredicate(P Pred) const { int32 I = IndexOfByPredicate(Pred); return I == INDEX_NONE ? nullptr : &V[I]; }
	template <typename P> TArray FilterByPredicate(P Pred) const { TArray R; for (const T& X : V) if (Pred(X)) R.Add(X); return R; }
	template <typename P> int32 RemoveAll(P Pred) { size_t B = V.size(); V.erase(std::remove_if(V.begin(), V.end(), Pred), V.end()); return (int32)(B - V.size()); }
	int32 Remove(const T& X) { return RemoveAll([&X](const T& Y) { return Y == X; }); }
	void RemoveAt(int32 I) { V.erase(V.begin() + I); }
	void Insert(const T& X, int32 I) { V.insert(V.begin() + I, X); }
	void Reset() { V.clear(); }
	void Empty() { V.clear(); }
	T& Last() { return V.back(); }
	const T& Last() const { return V.back(); }
	void Append(const TArray& O) { V.insert(V.end(), O.V.begin(), O.V.end()); }
	template <typename P> void Sort(P Pred) { std::stable_sort(V.begin(), V.end(), Pred); }
	typename std::vector<T>::iterator begin() { return V.begin(); }
	typename std::vector<T>::iterator end() { return V.end(); }
	typename std::vector<T>::const_iterator begin() const { return V.begin(); }
	typename std::vector<T>::const_iterator end() const { return V.end(); }
};

template <typename K, typename T> class TMap
{
public:
	std::map<K, T> M;
	T& FindOrAdd(const K& Key) { return M[Key]; }
	T* Find(const K& Key) { auto It = M.find(Key); return It == M.end() ? nullptr : &It->second; }
};

template <typename T> class TOptional
{
	std::optional<T> O;
public:
	TOptional& operator=(const T& X) { O = X; return *this; }
	bool IsSet() const { return O.has_value(); }
	const T& GetValue() const { return *O; }
	void Reset() { O.reset(); }
};

struct FMath
{
	template <typename T> static T Clamp(T X, T A, T B) { return X < A ? A : (X > B ? B : X); }
	template <typename T> static T Min(T A, T B) { return A < B ? A : B; }
	template <typename T> static T Max(T A, T B) { return A > B ? A : B; }
	template <typename T> static T Abs(T A) { return A < 0 ? -A : A; }
	static int32 FloorToInt(double X) { return (int32)std::floor(X); }
	static float Pow(float A, float B) { return std::pow(A, B); }
	static double Pow(double A, double B) { return std::pow(A, B); }
	static int32 Rand() { return std::rand(); }
};

struct FRandomStream
{
	uint32 Seed = 0;
	FRandomStream() {}
	FRandomStream(int32 InSeed) { Initialize(InSeed); }
	void Initialize(int32 InSeed) { Seed = (uint32)InSeed; }
	uint32 Next() { Seed = Seed * 196314165u + 907633515u; return Seed; }
	float FRand() { return (Next() >> 8) / 16777216.f; }
	int32 RandRange(int32 Min, int32 Max) { int32 Range = Max - Min + 1; return Range <= 0 ? Min : Min + (int32)(FRand() * Range); }
	uint32 GetUnsignedInt() { return Next(); }
};

// ---------- UObject
class UObject { public: virtual ~UObject() {} };
inline UObject* GetTransientPackage() { return nullptr; }
template <typename T> T* NewObject(UObject* Outer = nullptr) { static std::vector<std::unique_ptr<UObject>> Pool; T* O = new T(); Pool.emplace_back(O); return O; }
template <typename T> class TObjectPtr
{
	T* P = nullptr;
public:
	TObjectPtr() {}
	TObjectPtr(T* In) : P(In) {}
	T* Get() const { return P; }
	T* operator->() const { return P; }
	explicit operator bool() const { return P != nullptr; }
	operator T*() const { return P; }
};
class UBlueprintFunctionLibrary : public UObject {};
class UPrimaryDataAsset : public UObject {};

// ---------- délégués natifs
#define DECLARE_MULTICAST_DELEGATE_TwoParams(Name, T1, T2) \
	struct Name { std::vector<std::function<void(T1, T2)>> F; \
		void Broadcast(T1 A, T2 B) { auto Copy = F; for (auto& X : Copy) X(A, B); } \
		template <typename O, typename M> void AddUObject(O* Obj, M Method) { F.push_back([Obj, Method](T1 A, T2 B) { (Obj->*Method)(A, B); }); } \
		template <typename L> void AddLambda(L Lambda) { F.push_back(Lambda); } \
		void Clear() { F.clear(); } };

// ---------- énumérations : noms (pour UPokemonDatabase::GetTypeEffectiveness)
struct FShimEnum { std::vector<std::wstring> Names; FString GetNameStringByValue(int64 V) const { return V >= 0 && V < (int64)Names.size() ? FString(Names[V]) : FString(); } };
template <typename E> FShimEnum* StaticEnum();
