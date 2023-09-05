#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "../ext/minhook/MinHook.h"

#include "usercmd.h"

// get interface function
template <typename T>
static T* GetInterface(const char* name, const char* library)
{
	const HINSTANCE handle = GetModuleHandle(library);

	if (!handle)
		return nullptr;

	using Fn = T * (*)(const char*, int*);
	const Fn CreateInterface = reinterpret_cast<Fn>(GetProcAddress(handle, "CreateInterface"));

	return CreateInterface(name, nullptr);
}

// interfaces
static void* g_Client = nullptr;
static void* g_ClientMode = nullptr;

using CreateMove = bool(__thiscall*)(void*, float, UserCmd*);
static CreateMove CreateMoveOriginal = nullptr;

// offsets
constexpr unsigned int dwLocalPlayer = 0xDEB99C;
constexpr unsigned int m_fFlags = 0x104;

bool __stdcall CreateMoveHook(float frameTime, UserCmd* cmd)
{
	const bool result = CreateMoveOriginal(g_ClientMode, frameTime, cmd);

	if (!cmd || !cmd->commandNumber)
		return result;

	static unsigned int client = reinterpret_cast<unsigned int>(GetModuleHandle("client.dll"));

	const unsigned int localPlayer = *reinterpret_cast<unsigned int*>(client + dwLocalPlayer);

	if (localPlayer)
	{
		if (!(*reinterpret_cast<int*>(localPlayer + m_fFlags) & 1))
		{
			cmd->buttons &= ~IN_JUMP;
		}
	}

	return false;

	return result;
}

DWORD WINAPI Setup(LPVOID instance)
{
	g_Client = GetInterface<void>("VClient018", "client.dll");

	g_ClientMode = **reinterpret_cast<void***>((*reinterpret_cast<unsigned int**>(g_Client))[10] + 5);

	MH_Initialize();

	MH_CreateHook(
		(*static_cast<void***>(g_ClientMode))[24],
		&CreateMoveHook,
		reinterpret_cast<void**>(&CreateMoveOriginal)
	);

	MH_EnableHook(MH_ALL_HOOKS);

	while (!GetAsyncKeyState(VK_INSERT))
		Sleep(200);

	MH_DisableHook(MH_ALL_HOOKS);
	MH_RemoveHook(MH_ALL_HOOKS);
	MH_Uninitialize();

	FreeLibraryAndExitThread(static_cast<HMODULE>(instance), 0);
}

BOOL WINAPI DllMain(
	HINSTANCE instance,
	DWORD reason,
	LPVOID reserved
)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(instance);

		const HANDLE thread = CreateThread(
			nullptr,
			NULL,
			Setup,
			instance,
			NULL,
			nullptr
		);

		if (thread)
			CloseHandle(thread);
	}

	return TRUE;
}