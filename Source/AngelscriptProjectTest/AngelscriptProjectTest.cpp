#include "Modules/ModuleManager.h"

class FAngelscriptProjectTestModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FAngelscriptProjectTestModule, AngelscriptProjectTest)
