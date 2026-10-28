#pragma once
#include "CoreMinimal.h"
#include <iostream>
enum class EAutomationTestFlags : uint32 { EditorContext = 1, EngineFilter = 2 };
constexpr EAutomationTestFlags operator|(EAutomationTestFlags A, EAutomationTestFlags B) { return (EAutomationTestFlags)((uint32)A | (uint32)B); }
struct FShimTest
{
	std::string Name; int32 Failures = 0; int32 Checks = 0;
	virtual ~FShimTest() {}
	virtual bool RunTest(const FString&) = 0;
	void Fail(const TCHAR* What, const std::string& Detail) { ++Failures; std::cout << "  ECHEC " << Name << " : " << FString(What).Narrow() << " " << Detail << "\n"; }
	static std::string Str(const FName& V) { return V.ToString().Narrow(); }
	static std::string Str(float V) { return std::to_string(V); }
	static std::string Str(double V) { return std::to_string(V); }
	static std::string Str(int32 V) { return std::to_string(V); }
	template <typename E> static std::enable_if_t<std::is_enum_v<E>, std::string> Str(E V) { return std::to_string((int)V); }
	template <typename A, typename B> bool TestEqual(const TCHAR* What, const A& Actual, const B& Expected)
	{ ++Checks; bool Ok = Actual == (A)Expected; if (!Ok) Fail(What, "obtenu " + Str(Actual) + ", attendu " + Str((A)Expected)); return Ok; }
	bool TestTrue(const TCHAR* What, bool V) { ++Checks; if (!V) Fail(What, "(faux)"); return V; }
	bool TestFalse(const TCHAR* What, bool V) { ++Checks; if (V) Fail(What, "(vrai)"); return !V; }
};
inline std::vector<FShimTest*>& ShimTests() { static std::vector<FShimTest*> T; return T; }
#define IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class, PrettyName, Flags) \
	struct Class : FShimTest { Class() { Name = PrettyName; ShimTests().push_back(this); } bool RunTest(const FString& Parameters) override; }; \
	static Class Class##Instance;
