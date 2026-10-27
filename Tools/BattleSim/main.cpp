#include "Misc/AutomationTest.h"
int main()
{
	int Failed = 0, Checks = 0;
	for (FShimTest* T : ShimTests())
	{
		T->RunTest(FString());
		Checks += T->Checks;
		std::cout << (T->Failures ? "ECHEC " : "ok    ") << T->Name << " (" << T->Checks << " verifications)\n";
		Failed += T->Failures ? 1 : 0;
	}
	std::cout << ShimTests().size() - Failed << "/" << ShimTests().size() << " tests reussis, " << Checks << " verifications\n";
	return Failed ? 1 : 0;
}
