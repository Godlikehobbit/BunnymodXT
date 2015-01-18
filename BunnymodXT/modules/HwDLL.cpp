#include "../stdafx.hpp"

#include "../sptlib-wrapper.hpp"
#include <SPTLib/MemUtils.hpp>
#include <SPTLib/Hooks.hpp>
#include "HwDLL.hpp"
#include "ClientDLL.hpp"
#include "../patterns.hpp"
#include "../cvars.hpp"

// Linux hooks.
#ifndef _WIN32
extern "C" void __cdecl Cbuf_Execute()
{
	return HwDLL::HOOKED_Cbuf_Execute();
}
#endif

void HwDLL::Hook(const std::wstring& moduleName, void* moduleHandle, void* moduleBase, size_t moduleLength, bool needToIntercept)
{
	Clear(); // Just in case.

	m_Handle = moduleHandle;
	m_Base = moduleBase;
	m_Length = moduleLength;
	m_Name = moduleName;
	m_Intercepted = needToIntercept;

	MemUtils::ptnvec_size ptnNumber;
	void *pCbuf_Execute, *pCvar_RegisterVariable, *pCvar_DirectSet, *pCbuf_InsertText, *pCmd_AddMallocCommand, *pSeedRandomNumberGenerator, *pRandomFloat, *pRandomLong, *pSCR_DrawFPS;
	std::shared_future<MemUtils::ptnvec_size> fCbuf_Execute, fCvar_RegisterVariable, fCvar_DirectSet, fCbuf_InsertText, fCmd_AddMallocCommand, fSeedRandomNumberGenerator, fRandomFloat, fRandomLong, fSCR_DrawFPS, fHost_Tell_f;
	std::vector< std::shared_future<MemUtils::ptnvec_size> > futures;

	pCbuf_Execute = MemUtils::GetSymbolAddress(moduleHandle, "Cbuf_Execute");
	if (pCbuf_Execute)
	{
		ORIG_Cbuf_Execute = reinterpret_cast<_Cbuf_Execute>(pCbuf_Execute);
		EngineDevMsg("[hw dll] Found Cbuf_Execute at %p.\n", pCbuf_Execute);

		cls = MemUtils::GetSymbolAddress(moduleHandle, "cls");
		if (cls)
			EngineDevMsg("[hw dll] Found cls at %p.\n", cls);
		else
		{
			EngineDevWarning("[hw dll] Couldn't get the address of cls!\n");
			ORIG_Cbuf_Execute = nullptr;
		}

		sv = MemUtils::GetSymbolAddress(moduleHandle, "sv");
		if (sv)
			EngineDevMsg("[hw dll] Found sv at %p.\n", sv);
		else
		{
			EngineDevWarning("[hw dll] Couldn't get the address of sv!\n");
			ORIG_Cbuf_Execute = nullptr;
		}

		cmd_text = reinterpret_cast<cmdbuf_t*>(MemUtils::GetSymbolAddress(moduleHandle, "cmd_text"));
		if (cmd_text)
			EngineDevMsg("[hw dll] Found cmd_text at %p.\n", cmd_text);
		else
		{
			EngineDevWarning("[hw dll] Couldn't get the address of cmd_text!\n");
			ORIG_Cbuf_Execute = nullptr;
		}

		host_frametime = reinterpret_cast<double*>(MemUtils::GetSymbolAddress(moduleHandle, "host_frametime"));
		if (host_frametime)
			EngineDevMsg("[hw dll] Found host_frametime at %p.\n", sv);
		else
		{
			EngineDevWarning("[hw dll] Couldn't get the address of host_frametime!\n");
			ORIG_Cbuf_Execute = nullptr;
		}

#define FIND(f) \
	ORIG_##f = reinterpret_cast<_##f>(MemUtils::GetSymbolAddress(moduleHandle, #f)); \
	if (ORIG_##f) \
		EngineDevMsg("[hw dll] Found " #f " at %p.\n", ORIG_##f); \
		else \
		{ \
			EngineDevWarning("[hw dll] Couldn't get the address of " #f "!\n"); \
			ORIG_Cbuf_Execute = nullptr; \
		}

		FIND(Con_Printf)
		FIND(Cvar_RegisterVariable)
		FIND(Cvar_DirectSet)
		FIND(Cbuf_InsertText)
		FIND(Cmd_AddMallocCommand)
		FIND(SeedRandomNumberGenerator)
		FIND(RandomFloat)
		FIND(RandomLong)

#undef FIND
	}
	else
	{
		fCbuf_Execute = std::async(MemUtils::FindUniqueSequence, moduleBase, moduleLength, Patterns::ptnsCbuf_Execute, &pCbuf_Execute);
		fCvar_RegisterVariable = std::async(MemUtils::FindUniqueSequence, moduleBase, moduleLength, Patterns::ptnsCvar_RegisterVariable, &pCvar_RegisterVariable);
		fCvar_DirectSet = std::async(MemUtils::FindUniqueSequence, moduleBase, moduleLength, Patterns::ptnsCvar_DirectSet, &pCvar_DirectSet);
		fCbuf_InsertText = std::async(MemUtils::FindUniqueSequence, moduleBase, moduleLength, Patterns::ptnsCbuf_InsertText, &pCbuf_InsertText);
		fCmd_AddMallocCommand = std::async(MemUtils::FindUniqueSequence, moduleBase, moduleLength, Patterns::ptnsCmd_AddMallocCommand, &pCmd_AddMallocCommand);
		fSeedRandomNumberGenerator = std::async(MemUtils::FindUniqueSequence, moduleBase, moduleLength, Patterns::ptnsSeedRandomNumberGenerator, &pSeedRandomNumberGenerator);
		fRandomFloat = std::async(MemUtils::FindUniqueSequence, moduleBase, moduleLength, Patterns::ptnsRandomFloat, &pRandomFloat);
		fRandomLong = std::async(MemUtils::FindUniqueSequence, moduleBase, moduleLength, Patterns::ptnsRandomLong, &pRandomLong);
		fSCR_DrawFPS = std::async(MemUtils::FindUniqueSequence, moduleBase, moduleLength, Patterns::ptnsSCR_DrawFPS, &pSCR_DrawFPS);
		void *Host_Tell_f;
		fHost_Tell_f = std::async(MemUtils::FindUniqueSequence, moduleBase, moduleLength, Patterns::ptnsHost_Tell_f, &Host_Tell_f);
		futures.push_back(fCbuf_Execute);
		futures.push_back(fCvar_RegisterVariable);
		futures.push_back(fCvar_DirectSet);
		futures.push_back(fCbuf_InsertText);
		futures.push_back(fCmd_AddMallocCommand);
		futures.push_back(fSeedRandomNumberGenerator);
		futures.push_back(fRandomFloat);
		futures.push_back(fRandomLong);
		futures.push_back(fSCR_DrawFPS);
		futures.push_back(fHost_Tell_f);

		void *Host_AutoSave_f;
		ptnNumber = MemUtils::FindUniqueSequence(moduleHandle, moduleLength, Patterns::ptnsHost_AutoSave_f, &Host_AutoSave_f);
		if (ptnNumber != MemUtils::INVALID_SEQUENCE_INDEX)
		{
			EngineDevMsg("[hw dll] Found Host_AutoSave_f at %p (using the %s pattern).\n", Host_AutoSave_f, Patterns::ptnsHost_AutoSave_f[ptnNumber].build.c_str());

			auto f = reinterpret_cast<uintptr_t>(Host_AutoSave_f);
			sv = *reinterpret_cast<void**>(f + 19);
			ORIG_Con_Printf = reinterpret_cast<_Con_Printf>(
				*reinterpret_cast<ptrdiff_t*>(f + 33)
				+ (f + 37)
				);
			cls = *reinterpret_cast<void**>(f + 69);
			EngineDevMsg("[hw dll] Found sv at %p.\n", sv);
			EngineDevMsg("[hw dll] Found Con_Printf at %p.\n", ORIG_Con_Printf);
			EngineDevMsg("[hw dll] Found cls at %p.\n", cls);

			ptnNumber = fCbuf_Execute.get();
			if (ptnNumber != MemUtils::INVALID_SEQUENCE_INDEX)
			{
				ORIG_Cbuf_Execute = reinterpret_cast<_Cbuf_Execute>(pCbuf_Execute);
				EngineDevMsg("[hw dll] Found Cbuf_Execute at %p (using the %s pattern).\n", pCbuf_Execute, Patterns::ptnsCbuf_Execute[ptnNumber].build.c_str());

				switch (ptnNumber)
				{
				case 0:
					cmd_text = reinterpret_cast<cmdbuf_t*>(*reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(pCbuf_Execute) + 11) - offsetof(cmdbuf_t, cursize));
					break;

				case 1:
					cmd_text = reinterpret_cast<cmdbuf_t*>(*reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(pCbuf_Execute) + 2) - offsetof(cmdbuf_t, cursize));
					break;
				}
				EngineDevMsg("[hw dll] Found cmd_text at %p.\n", cmd_text);
			}
			else
			{
				EngineDevWarning("[hw dll] Could not find Cbuf_Execute!\n");
			}

#define FIND(a) \
	ptnNumber = f##a.get(); \
	if (ptnNumber != MemUtils::INVALID_SEQUENCE_INDEX) \
	{ \
		ORIG_##a = reinterpret_cast<_##a>(p##a); \
		EngineDevMsg("[hw dll] Found " #a " at %p (using the %s pattern).\n", p##a, Patterns::ptns##a[ptnNumber].build.c_str()); \
	} \
	else \
	{ \
		EngineDevWarning("[hw dll] Could not find " #a "!\n"); \
		ORIG_Cbuf_Execute = nullptr; \
	}

			FIND(Cvar_RegisterVariable)
			FIND(Cvar_DirectSet)
			FIND(Cbuf_InsertText)
			FIND(Cmd_AddMallocCommand)

			ptnNumber = fHost_Tell_f.get();
			if (ptnNumber != MemUtils::INVALID_SEQUENCE_INDEX)
			{
				EngineDevMsg("[hw dll] Found Host_Tell_f at %p (using the %s pattern).\n", Host_Tell_f, Patterns::ptnsHost_Tell_f[ptnNumber].build.c_str());

				uintptr_t offCmd_Argc, offCmd_Args, offCmd_Argv;
				switch (ptnNumber)
				{
				// SteamPipe.
				case 0:
					offCmd_Argc = 28;
					offCmd_Args = 42;
					offCmd_Argv = 145;
					break;
				// NGHL.
				case 1:
					offCmd_Argc = 24;
					offCmd_Args = 38;
					offCmd_Argv = 143;
					break;
				}

				auto f = reinterpret_cast<uintptr_t>(Host_Tell_f);
				ORIG_Cmd_Argc = reinterpret_cast<_Cmd_Argc>(
					*reinterpret_cast<uintptr_t*>(f + offCmd_Argc)
					+ (f + offCmd_Argc + 4)
				);
				ORIG_Cmd_Args = reinterpret_cast<_Cmd_Args>(
					*reinterpret_cast<uintptr_t*>(f + offCmd_Args)
					+ (f + offCmd_Args + 4)
				);
				ORIG_Cmd_Argv = reinterpret_cast<_Cmd_Argv>(
					*reinterpret_cast<uintptr_t*>(f + offCmd_Argv)
					+ (f + offCmd_Argv + 4)
				);
				EngineDevMsg("[hw dll] Found Cmd_Argc at %p.\n", ORIG_Cmd_Argc);
				EngineDevMsg("[hw dll] Found Cmd_Args at %p.\n", ORIG_Cmd_Args);
				EngineDevMsg("[hw dll] Found Cmd_Argv at %p.\n", ORIG_Cmd_Argv);
			}
			else
			{
				EngineDevWarning("[hw dll] Could not find Host_Tell_f!\n");
				ORIG_Cmd_AddMallocCommand = nullptr;
			}

			ptnNumber = fSeedRandomNumberGenerator.get();
			if (ptnNumber != MemUtils::INVALID_SEQUENCE_INDEX)
			{
				ORIG_SeedRandomNumberGenerator = reinterpret_cast<_SeedRandomNumberGenerator>(pSeedRandomNumberGenerator);
				EngineDevMsg("[hw dll] Found SeedRandomNumberGenerator at %p (using the %s pattern).\n", pSeedRandomNumberGenerator, Patterns::ptnsSeedRandomNumberGenerator[ptnNumber].build.c_str());

				ORIG_time = reinterpret_cast<_time>(
					*reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(pSeedRandomNumberGenerator) + 3)
					+ reinterpret_cast<uintptr_t>(pSeedRandomNumberGenerator) + 7
				);
				EngineDevMsg("[hw dll] ORIG_time is %p.\n", ORIG_time);
			}
			else
			{
				EngineDevWarning("[hw dll] Could not find SeedRandomNumberGenerator!\n");
				ORIG_Cbuf_Execute = nullptr;
			}

			//FIND(RandomFloat)
			//FIND(RandomLong)

			ptnNumber = fSCR_DrawFPS.get();
			if (ptnNumber != MemUtils::INVALID_SEQUENCE_INDEX)
			{
				EngineDevMsg("[hw dll] Found SCR_DrawFPS at %p (using the %s pattern).\n", pSCR_DrawFPS, Patterns::ptnsSCR_DrawFPS[ptnNumber].build.c_str());

				host_frametime = *reinterpret_cast<double**>(reinterpret_cast<uintptr_t>(pSCR_DrawFPS) + 21);
				EngineDevMsg("[hw dll] Found host_frametime at %p.\n", host_frametime);
			}
			else
			{
				EngineDevWarning("[hw dll] Could not find SCR_DrawFPS!\n");
				ORIG_Cbuf_Execute = nullptr;
			}

#undef FIND
		}
		else
		{
			EngineDevMsg("[hw dll] Could not find Host_AutoSave_f!\n");
		}
	}

	if (ORIG_Cbuf_Execute && !ORIG_time)
		ORIG_time = time;

	if (needToIntercept)
		MemUtils::Intercept(moduleName, {
			{ reinterpret_cast<void**>(&ORIG_Cbuf_Execute), reinterpret_cast<void*>(HOOKED_Cbuf_Execute) },
			{ reinterpret_cast<void**>(&ORIG_SeedRandomNumberGenerator), reinterpret_cast<void*>(HOOKED_SeedRandomNumberGenerator) },
			{ reinterpret_cast<void**>(&ORIG_time), reinterpret_cast<void*>(HOOKED_time) },
			{ reinterpret_cast<void**>(&ORIG_RandomFloat), reinterpret_cast<void*>(HOOKED_RandomFloat) },
			{ reinterpret_cast<void**>(&ORIG_RandomLong), reinterpret_cast<void*>(HOOKED_RandomLong) }
		});

	for (auto f : futures)
		if (f.valid())
			f.wait();
}

void HwDLL::Unhook()
{
	if (m_Intercepted)
		MemUtils::RemoveInterception(m_Name, {
			{ reinterpret_cast<void**>(&ORIG_Cbuf_Execute), reinterpret_cast<void*>(HOOKED_Cbuf_Execute) },
			{ reinterpret_cast<void**>(&ORIG_SeedRandomNumberGenerator), reinterpret_cast<void*>(HOOKED_SeedRandomNumberGenerator) },
			{ reinterpret_cast<void**>(&ORIG_time), reinterpret_cast<void*>(HOOKED_time) },
			{ reinterpret_cast<void**>(&ORIG_RandomFloat), reinterpret_cast<void*>(HOOKED_RandomFloat) },
			{ reinterpret_cast<void**>(&ORIG_RandomLong), reinterpret_cast<void*>(HOOKED_RandomLong) }
	});

	Clear();
}

void HwDLL::Clear()
{
	ORIG_Cbuf_Execute = nullptr;
	ORIG_SeedRandomNumberGenerator = nullptr;
	ORIG_time = nullptr;
	ORIG_RandomFloat = nullptr;
	ORIG_RandomLong = nullptr;
	ORIG_Cbuf_InsertText = nullptr;
	ORIG_Con_Printf = nullptr;
	ORIG_Cvar_RegisterVariable = nullptr;
	ORIG_Cvar_DirectSet = nullptr;
	ORIG_Cmd_AddMallocCommand = nullptr;
	ORIG_Cmd_Argc = nullptr;
	ORIG_Cmd_Args = nullptr;
	ORIG_Cmd_Argv = nullptr;
	registeredVarsAndCmds = false;
	cls = nullptr;
	sv = nullptr;
	cmd_text = nullptr;
	host_frametime = nullptr;
	executing = false;
	loading = false;
	insideCbuf_Execute = false;
	finishingLoad = false;
	dontPauseNextCycle = false;
	insideSeedRNG = false;
}

void HwDLL::Cmd_BXT_TAS_LoadScript()
{
	return HwDLL::GetInstance().Cmd_BXT_TAS_LoadScript_f();
}
void HwDLL::Cmd_BXT_TAS_LoadScript_f()
{
	ORIG_Cvar_DirectSet(bxt_tas.GetPointer(), "1");
}

void HwDLL::RegisterCVarsAndCommandsIfNeeded()
{
	if (!registeredVarsAndCmds)
	{
		registeredVarsAndCmds = true;
		if (ORIG_Cvar_RegisterVariable)
		{
			ORIG_Cvar_RegisterVariable(bxt_tas.GetPointer());
			ORIG_Cvar_RegisterVariable(_bxt_taslog.GetPointer());
		}
		if (ORIG_Cmd_AddMallocCommand)
			ORIG_Cmd_AddMallocCommand("bxt_tas_loadscript", Cmd_BXT_TAS_LoadScript, 2); // 2 - Cmd_AddGameCommand.
	}
}

bool HwDLL::CheckUnpause()
{
	size_t current_cmd;
	for (size_t off = 0; off < cmd_text->cursize; ++off)
	{
		current_cmd = off;
		unsigned quotes = 0;
		for (; off < cmd_text->cursize; ++off)
		{
			char c = cmd_text->data[off];
			quotes += (c == '"');
			if (!(quotes & 1) && c == ';')
				break;
			if (c == '\n')
				break;
		}

		if (off - current_cmd == 7 && !std::strncmp(cmd_text->data + current_cmd, "unpause", 7))
			return true;
	}

	return false;
}

void HwDLL::InsertCommands()
{
	ORIG_Cbuf_InsertText("echo We are executing!\n");
}

HOOK_DEF_0(HwDLL, void, __cdecl, Cbuf_Execute)
{
	RegisterCVarsAndCommandsIfNeeded();

	int state = *reinterpret_cast<int*>(cls);
	int paused = *(reinterpret_cast<int*>(sv) + 1);

	// If cls.state == 4 and the game isn't paused, execute "pause" right now.
	// This case happens when loading a savegame.
	if (state == 4 && !paused && bxt_tas.GetBool())
	{
		ORIG_Cbuf_InsertText("pause\n");
		finishingLoad = true;
	}

	// All map load / change commands call Cbuf_Execute inside them, while we already are inside one.
	if (insideCbuf_Execute)
	{
		loading = true;
		executing = false;
	}
	if (!loading && state == 5)
		executing = true;
	if (loading && state == 3)
		loading = false;
	// Manually unpause in WON.
	if (finishingLoad && state == 5 && paused && !CheckUnpause())
		ORIG_Cbuf_InsertText("pause\n");

	static unsigned counter = 1;
	auto c = counter++;
	std::string buf(cmd_text->data, cmd_text->cursize); // TODO: ifdef this so it doesn't waste performance.
	if (_bxt_taslog.GetBool())
		ORIG_Con_Printf("Cbuf_Execute() #%u begin; cls.state: %d; sv.paused: %d; time: %f; loading: %s; executing: %s; host_frametime: %f; buffer: %s\n", c, state, paused, *reinterpret_cast<double*>(reinterpret_cast<uintptr_t>(sv)+16), (loading ? "true" : "false"), (executing ? "true" : "false"), *host_frametime, buf.c_str());

	insideCbuf_Execute = true;
	ORIG_Cbuf_Execute(); // executing might change inside if we had some kind of load command in the buffer.

	// Insert our commands after any commands that might have been on this frame
	// and call Cbuf_Execute again to execute them.
	if (executing)
	{
		finishingLoad = false;
		// For stopping Cbuf_Execute. Goes first because InsertCommands() inserts into beginning.
		if (cmd_text->cursize)
			ORIG_Cbuf_InsertText("wait\n");
		InsertCommands();

		buf.assign(cmd_text->data, cmd_text->cursize);
		if (_bxt_taslog.GetBool())
			ORIG_Con_Printf("Cbuf_Execute() #%u executing; buffer: %s\n", c, buf.c_str());

		// Setting to true once again because it might have been reset in Cbuf_Execute.
		insideCbuf_Execute = true;
		ORIG_Cbuf_Execute();
	}
	insideCbuf_Execute = false;

	buf.assign(cmd_text->data, cmd_text->cursize);
	if (_bxt_taslog.GetBool())
		ORIG_Con_Printf("Cbuf_Execute() #%u end; host_frametime: %f; buffer: %s\n", c, *host_frametime, buf.c_str());

	// If cls.state == 3 and the game isn't paused, execute "pause" on the next cycle.
	// This case happens when starting a map.
	if (!dontPauseNextCycle && state == 3 && !paused && bxt_tas.GetBool())
	{
		ORIG_Cbuf_InsertText("pause\n");
		finishingLoad = true;
		dontPauseNextCycle = true;
	}
	else
		dontPauseNextCycle = false;
}

HOOK_DEF_0(HwDLL, void, __cdecl, SeedRandomNumberGenerator)
{
	insideSeedRNG = true;
	EngineMsg("Calling SeedRandomNumberGenerator!\n");
	ORIG_SeedRandomNumberGenerator();
	insideSeedRNG = false;
}

HOOK_DEF_1(HwDLL, time_t, __cdecl, time, time_t*, Time)
{
	if (insideSeedRNG)
	{
		EngineMsg("Called time from SeedRandomNumberGenerator!\n");
		return 0;
	}

	return ORIG_time(Time);
}

HOOK_DEF_2(HwDLL, long double, __cdecl, RandomFloat, float, a1, float, a2)
{
	//ORIG_Con_Printf("Calling RandomFloat.\n");
	return ORIG_RandomFloat(a1, a2);
}

HOOK_DEF_2(HwDLL, long, __cdecl, RandomLong, long, a1, long, a2)
{
	//ORIG_Con_Printf("Calling RandomLong.\n");
	return ORIG_RandomLong(a1, a2);
}