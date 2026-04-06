#pragma once

#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestEngineHelper.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

// ============================================================================
// Angelscript Test Macros
// ============================================================================
//
// Two-layer macro system for test engine management:
//
//   Layer 1 - Engine Creation:  ASTEST_CREATE_ENGINE_*
//   Layer 2 - Lifecycle:        ASTEST_BEGIN_* / ASTEST_END_*
//
// Usage:
//   FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
//   ASTEST_BEGIN_FULL
//   // ... test code ...
//   ASTEST_END_FULL
//   return true;
//
// See TESTING_GUIDE.md for detailed usage and decision tree.
// ============================================================================

// ============================================================================
// Layer 1: Engine Creation Macros
// ============================================================================

// FULL - Creates a fresh isolated Full engine each time.
// Use for: engine core self-tests, bind environment testing, hot-reload tests.
// Provides: FAngelscriptEngine& Engine
#define ASTEST_CREATE_ENGINE_FULL() \
	(*[this]() -> TUniquePtr<FAngelscriptEngine>& { \
		static thread_local TUniquePtr<FAngelscriptEngine> _FullEngine; \
		_FullEngine = AngelscriptTestSupport::CreateIsolatedFullEngine(); \
		check(_FullEngine.IsValid()); \
		return _FullEngine; \
	}())

// SHARE - Process-level singleton, reused across tests, no reset.
// Use for: lightweight compile-and-execute tests with no isolation needs.
// Provides: FAngelscriptEngine& Engine
#define ASTEST_CREATE_ENGINE_SHARE() \
	AngelscriptTestSupport::GetOrCreateSharedCloneEngine()

#define ASTEST_CREATE_ENGINE_SHARE_CLEAN() \
	AngelscriptTestSupport::AcquireCleanSharedCloneEngine()

#define ASTEST_CREATE_ENGINE_SHARE_FRESH() \
	AngelscriptTestSupport::AcquireFreshSharedCloneEngine()

// CLONE - Lightweight isolation, shares source engine read-only state.
// Use for: tests needing isolation without Full engine creation cost.
// Provides: FAngelscriptEngine& Engine
#define ASTEST_CREATE_ENGINE_CLONE() \
	(*[this]() -> TUniquePtr<FAngelscriptEngine>& { \
		static thread_local TUniquePtr<FAngelscriptEngine> _CloneEngine; \
		_CloneEngine = AngelscriptTestSupport::CreateIsolatedCloneEngine(); \
		check(_CloneEngine.IsValid()); \
		return _CloneEngine; \
	}())

// NATIVE - Raw asIScriptEngine without FAngelscriptEngine wrapper.
// Use for: testing AngelScript SDK APIs directly.
// Provides: asIScriptEngine* NativeEngine
#define ASTEST_CREATE_ENGINE_NATIVE() \
	asCreateScriptEngine(ANGELSCRIPT_VERSION)

// ============================================================================
// Layer 2: Lifecycle Macros (BEGIN / END pairs)
// ============================================================================
// Placement rule:
//   `ASTEST_END_*` closes the lifecycle scope opened by `ASTEST_BEGIN_*`.
//   Put the terminal `return` after `ASTEST_END_*` so the lifecycle pairing
//   remains explicit in source, even though RAII cleanup still runs on early
//   returns inside the scoped block.

// ---------- FULL lifecycle ----------
// Establishes EngineScope + auto-discards all modules on exit.
#define ASTEST_BEGIN_FULL \
	{ \
		FAngelscriptEngineScope _AutoEngineScope(Engine); \
		ON_SCOPE_EXIT \
		{ \
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules(); \
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules) \
			{ \
				Engine.DiscardModule(*_Module->ModuleName); \
			} \
		};

#define ASTEST_END_FULL \
	}

// ---------- SHARE lifecycle ----------
// Establishes the current-engine scope, but leaves shared-engine module state intact.
#define ASTEST_BEGIN_SHARE \
	{ \
		FAngelscriptEngineScope _AutoEngineScope(Engine);

#define ASTEST_END_SHARE \
	}

// ---------- SHARE_CLEAN lifecycle ----------
// Establishes the current-engine scope after AcquireCleanSharedCloneEngine() reset semantics.
#define ASTEST_BEGIN_SHARE_CLEAN \
	{ \
		FAngelscriptEngineScope _AutoEngineScope(Engine);

#define ASTEST_END_SHARE_CLEAN \
	}

// ---------- SHARE_FRESH lifecycle ----------
// Establishes the current-engine scope after AcquireFreshSharedCloneEngine() reset semantics.
#define ASTEST_BEGIN_SHARE_FRESH \
	{ \
		FAngelscriptEngineScope _AutoEngineScope(Engine);

#define ASTEST_END_SHARE_FRESH \
	}

// ---------- CLONE lifecycle ----------
// Establishes EngineScope + auto-discards all modules on exit.
#define ASTEST_BEGIN_CLONE \
	{ \
		FAngelscriptEngineScope _AutoEngineScope(Engine); \
		ON_SCOPE_EXIT \
		{ \
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules(); \
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules) \
			{ \
				Engine.DiscardModule(*_Module->ModuleName); \
			} \
		};

#define ASTEST_END_CLONE \
	}

// ---------- NATIVE lifecycle ----------
// Validates engine pointer + auto ShutDownAndRelease on exit.
// Expects variable name: NativeEngine (asIScriptEngine*)
#define ASTEST_BEGIN_NATIVE \
	if (NativeEngine == nullptr) \
	{ \
		AddError(TEXT("Failed to create native AngelScript engine")); \
		return false; \
	} \
	{ \
		ON_SCOPE_EXIT { NativeEngine->ShutDownAndRelease(); };

#define ASTEST_END_NATIVE \
	}

// ============================================================================
// Helper Macros: Compile + Execute shortcuts
// ============================================================================

// Compile module + get function + execute int, return false on any failure.
// Requires: *this is FAutomationTestBase, Engine is FAngelscriptEngine&
#define ASTEST_COMPILE_RUN_INT(Engine, ModuleName, Source, FuncDecl, OutResult) \
	do { \
		asIScriptModule* _Module = AngelscriptTestSupport::BuildModule( \
			*this, Engine, ModuleName, Source); \
		if (_Module == nullptr) { return false; } \
		asIScriptFunction* _Function = AngelscriptTestSupport::GetFunctionByDecl( \
			*this, *_Module, FuncDecl); \
		if (_Function == nullptr) { return false; } \
		if (!AngelscriptTestSupport::ExecuteIntFunction( \
			*this, Engine, *_Function, OutResult)) { return false; } \
	} while (false)

// Same as above but for int64 return type.
#define ASTEST_COMPILE_RUN_INT64(Engine, ModuleName, Source, FuncDecl, OutResult) \
	do { \
		asIScriptModule* _Module = AngelscriptTestSupport::BuildModule( \
			*this, Engine, ModuleName, Source); \
		if (_Module == nullptr) { return false; } \
		asIScriptFunction* _Function = AngelscriptTestSupport::GetFunctionByDecl( \
			*this, *_Module, FuncDecl); \
		if (_Function == nullptr) { return false; } \
		if (!AngelscriptTestSupport::ExecuteInt64Function( \
			*this, Engine, *_Function, OutResult)) { return false; } \
	} while (false)

// Compile only (no execution). Sets OutModulePtr, returns false on failure.
#define ASTEST_BUILD_MODULE(Engine, ModuleName, Source, OutModulePtr) \
	do { \
		OutModulePtr = AngelscriptTestSupport::BuildModule( \
			*this, Engine, ModuleName, Source); \
		if (OutModulePtr == nullptr) { return false; } \
	} while (false)
