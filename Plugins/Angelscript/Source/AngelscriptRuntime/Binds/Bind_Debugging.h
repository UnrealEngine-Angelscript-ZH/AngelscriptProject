#pragma once

#include "CoreMinimal.h"

// This hook can be used to forget what ensures we've seen already.
// Otherwise we will break on each ensure only once.
ANGELSCRIPTRUNTIME_API void AngelscriptForgetSeenEnsures();

// Makes it possible to turn off the actual debug break when you
// have a C++ debugger attached (errors are still logged on failing
// ensures however).
ANGELSCRIPTRUNTIME_API void AngelscriptDisableDebugBreaks();
ANGELSCRIPTRUNTIME_API void AngelscriptEnableDebugBreaks();
