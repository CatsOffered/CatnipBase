#pragma once
#include "Netvars.h"
#include "SDK/vector.h"
#include "SDK/mathlib.h"

#define NV_RET(type, var) return *(type*)(this + var)

class CEntityWrapper;
class IClientEntity;
class ClientClass;

class CBaseEntity
{
public:
	inline void* Renderable() const { return (void**)this + 1; }
	inline void* Networkable() const { return (void**)this + 2; }

	bool		SetupBones(matrix3x4_t* pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime);
	ClientClass* GetClientClass();
	bool		IsDormant(void);
	int			entindex(void) const;

	inline const Vector& Origin() { NV_RET(Vector, Netvars::m_vecOrigin); }
	inline const QAngle& Rotation() { NV_RET(QAngle, Netvars::m_angRotation); }
	inline const Vector& Mins() { NV_RET(Vector, Netvars::m_vecMins); }
	inline const Vector& Maxs() { NV_RET(Vector, Netvars::m_vecMaxs); }
	inline const Vector& ViewOffset() { NV_RET(Vector, Netvars::m_vecViewOffset); }
	inline int Team() { NV_RET(int, Netvars::m_iTeamNum); }
};