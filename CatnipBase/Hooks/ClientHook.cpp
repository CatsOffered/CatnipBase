#include "ClientHook.h"
#include "Base/Interfaces.h"
#include "Base/Sig.h"
#include "SDK/usercmd.h"
#include <intrin.h>

CClientHook::CClientHook() : BASEHOOK(CClientHook)
{
	RegisterEvent(EVENT_CREATEMOVE);
	RegisterEvent(EVENT_HLCREATEMOVE);
	RegisterEvent(EVENT_FRAMESTAGENOTIFY);
}

void CClientHook::Hook()
{
	m_hlhook.Hook(Interfaces::hlclient->Inst());
	m_hlhook.Set(Interfaces::hlclient->GetOffset(Off_CreateMove), Hooked_HLCreateMove);
	m_hlhook.Set(Interfaces::hlclient->GetOffset(Off_FrameStageNotify), Hooked_FrameStageNotify);

	m_clhook.Hook(Interfaces::client->Inst());
	m_clhook.Set(Interfaces::client->GetOffset(Off_CreateMove), Hooked_CreateMove);
}

void CClientHook::Unhook()
{
	m_hlhook.Unhook();
	m_clhook.Unhook();
}

void CClientHook::HLCreateMove(int sequence_number, float input_sample_frametime, bool active)
{
	// TODO: Hideous
	static auto original = m_hlhook.Get<HLCreateMoveFn_t>(Interfaces::hlclient->GetOffset(Off_CreateMove));
	original(Interfaces::hlclient->Inst(), sequence_number, input_sample_frametime, active);
}

void CClientHook::FrameStageNotify(ClientFrameStage_t curStage)
{
	static auto original = m_hlhook.Get<FrameStageNotifyFn_t>(Interfaces::hlclient->GetOffset(Off_FrameStageNotify));
	original(Interfaces::hlclient->Inst(), curStage);
}

bool CClientHook::CreateMove(float flInputSampleTime, CUserCmd* cmd)
{
	// !!	Compiler LITERALLY will not give the correct result.
	//		Always seems to call with completely wrong convention.
	//		Am I just being stupid?

	static auto original = m_clhook.Get<void*>(Interfaces::client->GetOffset(Off_CreateMove));
	//return ((CreateMoveFn_t)original)(Interfaces::client->Inst(), flInputSampleTime, cmd);

	m_clhook.Set(Interfaces::client->GetOffset(Off_CreateMove), original);
	bool result = Interfaces::client->CreateMove(flInputSampleTime, cmd);
	m_clhook.Set(Interfaces::client->GetOffset(Off_CreateMove), Hooked_CreateMove);
	return result;
}

void __stdcall CClientHook::Hooked_HLCreateMove(UNCRAP int sequence_number, float input_sample_frametime, bool active)
{
	bool bSendPacket;
	UINT_PTR* baseptr = (UINT_PTR*)_AddressOfReturnAddress() - 1;
	static int off = -1;

	if constexpr (Base::Win64)
		bSendPacket = AsmTools::GetR14();
	else if (Interfaces::engine->GetAppID() == AppId_CSGO)
	{
		static bool bSnap = false;
		if (!bSnap)
		{
			// https://i.imgur.com/yNbgIbl.png
			StackSnapshot snap;
			AsmTools::AnalyzeStackBeepBoop(&snap, &CClientHook::Hooked_HLCreateMove);
			off = snap.regs[RegIndex_B].off;

			printf("Stack off: %X\n", off);
			bSnap = true;
		}

		bSendPacket = *(bool*)((UINT_PTR)baseptr + off);
	}
	else
		bSendPacket = *(*(bool**)baseptr - 1);

	static auto hook = GETHOOK(CClientHook);
	auto ctx = hook->Context();
	ctx->active = active, ctx->input_sample_frametime = input_sample_frametime, ctx->bSendPacket = bSendPacket;

	int flags = hook->PushEvent(EVENT_HLCREATEMOVE);

	if (!(flags & Return_NoOriginal))
		hook->HLCreateMove(sequence_number, input_sample_frametime, active);

	if constexpr (Base::Win64)
		AsmTools::SetR14((void*)ctx->bSendPacket);
	else if (Interfaces::engine->GetAppID() == AppId_CSGO)
		*(bool*)((UINT_PTR)baseptr + off) = ctx->bSendPacket;
	else
		*(*(bool**)baseptr + off) = ctx->bSendPacket;
}

void __stdcall CClientHook::Hooked_FrameStageNotify(UNCRAP ClientFrameStage_t curStage)
{
	static auto hook = GETHOOK(CClientHook);

	auto ctx = hook->Context();
	ctx->curStage = curStage;

	int flags = hook->PushEvent(EVENT_FRAMESTAGENOTIFY);
	if (flags & Return_NoOriginal)
		return;

	hook->FrameStageNotify(curStage);
}

bool __stdcall CClientHook::Hooked_CreateMove(UNCRAP float flInputSampleTime, CUserCmd* cmd)
{
	static auto hook = GETHOOK(CClientHook);
	auto ctx = hook->Context();

	ctx->result = hook->CreateMove(flInputSampleTime, cmd);
	ctx->input_sample_frametime = flInputSampleTime;

	switch (Interfaces::engine->GetAppID())
	{
	case AppID_GMod:
		ctx->cmd = (CUserCmd*)((void**)cmd - 1); // Offset missing VMT
		break;
	default:
		ctx->cmd = cmd;
	}

	hook->PushEvent(EVENT_CREATEMOVE);

	if (ctx->cmd->tick_count % 14)
		ctx->bSendPacket = false;

    return ctx->result;
}
