#include "forceScript.h"
#include "float2.h"
#include "float3.h"
#include "float4.h"
#include "float2x2.h"
#include "float3x3.h"
#include "float4x4.h"
#include <TFE_System/system.h>
#include <TFE_FrontEndUI/frontEndUi.h>
#include <TFE_Jedi/Serialization/serialization.h>
#include <stdint.h>
#include <cstring>
#include <algorithm>
#include <assert.h>

// TFE_ForceScript wraps Anglescript, so these includes should only exist here.
#include <angelscript.h>
#include <scriptstdstring/scriptstdstring.h>
#include <scriptarray/scriptarray.h>
#include <scriptbuilder/scriptbuilder.h>

// DF3DS_RET1_FORCESCRIPT_INIT_TRACE_V54
#if defined(__3DS__)
extern "C" void df3ds_ret1_breadcrumb(const char* text);
#define DF3DS_RET1_FS_TRACE(msg) df3ds_ret1_breadcrumb(msg)
#else
#define DF3DS_RET1_FS_TRACE(msg) ((void)0)
#endif

namespace TFE_ForceScript
{

	// DF3DS RET1 V17 — ForceScript <-> AngelScript ARM ABI bridge.
	// TFE s32/u32 are 32-bit long types on this arm-none-eabi target while
	// AngelScript's public API explicitly uses native int/asUINT pointers.
	// Preserve TFE's internal types and adapt only at the API boundary.
	#ifndef DF3DS_RET1_FORCESCRIPT_AS_ABI_V17
	#define DF3DS_RET1_FORCESCRIPT_AS_ABI_V17 1

	static_assert(sizeof(s32) == sizeof(int),
		"DF3DS ForceScript bridge requires 32-bit s32/int width parity");
	static_assert(sizeof(u32) == sizeof(asUINT),
		"DF3DS ForceScript bridge requires 32-bit u32/asUINT width parity");

	static int df3dsAsGetGlobalVar(
		asIScriptModule* mod,
		asUINT index,
		const char** name,
		const char** nameSpace,
		s32* typeId)
	{
		int nativeTypeId = 0;
		const int result = mod->GetGlobalVar(
			index,
			name,
			nameSpace,
			typeId ? &nativeTypeId : nullptr);

		if (typeId)
			*typeId = static_cast<s32>(nativeTypeId);

		return result;
	}

	static int df3dsAsGetVar(
		asIScriptContext* context,
		asUINT varIndex,
		asUINT stackLevel,
		const char** name,
		s32* typeId,
		asETypeModifiers* typeModifiers,
		bool* isVarOnHeap,
		s32* stackOffset)
	{
		int nativeTypeId = 0;
		int nativeStackOffset = 0;

		const int result = context->GetVar(
			varIndex,
			stackLevel,
			name,
			typeId ? &nativeTypeId : nullptr,
			typeModifiers,
			isVarOnHeap,
			stackOffset ? &nativeStackOffset : nullptr);

		if (typeId)
			*typeId = static_cast<s32>(nativeTypeId);

		if (stackOffset)
			*stackOffset = static_cast<s32>(nativeStackOffset);

		return result;
	}

	static int df3dsAsGetArgOnStack(
		asIScriptContext* context,
		asUINT stackLevel,
		asUINT arg,
		s32* typeId,
		u32* flags,
		void** address)
	{
		int nativeTypeId = 0;
		asUINT nativeFlags = 0;

		const int result = context->GetArgOnStack(
			stackLevel,
			arg,
			typeId ? &nativeTypeId : nullptr,
			flags ? &nativeFlags : nullptr,
			address);

		if (typeId)
			*typeId = static_cast<s32>(nativeTypeId);

		if (flags)
			*flags = static_cast<u32>(nativeFlags);

		return result;
	}

	#endif

	const asPWORD ThreadId = 1002;
	
	struct ScriptThread
	{
		asIScriptContext* asContext;
		std::string name;
		std::string funcName;
		f32 delay;
	};

	struct ModuleDef
	{
		std::string name;
		std::string path;
		bool allowReadFromArchive;
		u32 accessMask;

		asIScriptModule* mod;
	};
	std::vector<ModuleDef> s_modules;

	static asIScriptEngine* s_engine = nullptr;
	static std::vector<ScriptThread> s_scriptThreads;
	static std::vector<s32> s_freeThreads;
	ScriptMessageCallback s_msgCallback = nullptr;

	static s32 s_typeId[FSTYPE_COUNT] = { 0 };

	void serializeVariable(Stream* stream, s32 typeId, void*& varAddr, const char* name, bool allocateObjects = false);

	// Script message callback.
	void messageCallback(const asSMessageInfo* msg, void* param)
	{
		LogWriteType type = LOG_ERROR;
		if (msg->type == asMSGTYPE_WARNING) { type = LOG_WARNING; }
		else if (msg->type == asMSGTYPE_INFORMATION) { type = LOG_MSG; }

		if (s_msgCallback)
		{
			s_msgCallback(type, msg->section, msg->row, msg->col, msg->message);
			return;
		}
		TFE_System::logWrite(type, "Script", "%s (%d, %d) : %s", msg->section, msg->row, msg->col, msg->message);
	}
		
	void yield(f32 delay)
	{
		asIScriptContext* context = asGetActiveContext();
		if (context)
		{
			const s32 id = (s32)((intptr_t)context->GetUserData(ThreadId));
			assert(id >= 0 && id < (s32)s_scriptThreads.size());
			s_scriptThreads[id].delay = delay;
			context->Suspend();
		}
	}

	void abort(const std::string& msg)
	{
		asIScriptContext* context = asGetActiveContext();
		if (context)
		{
			const s32 id = (s32)((intptr_t)context->GetUserData(ThreadId));
			assert(id >= 0 && id < (s32)s_scriptThreads.size());
			TFE_System::logWrite(LOG_ERROR, "Force Script", "Script '%s', Entry Point '%s' Aborted! - '%s'.",
				s_scriptThreads[id].name.c_str(), s_scriptThreads[id].funcName.c_str(), msg.c_str());
			context->Abort();
		}
	}

	void resume(s32 id)
	{
		s_scriptThreads[id].delay = 0.0f;
	}

	s32 getObjectTypeId(FS_BuiltInType type)
	{
		return s_typeId[type];
	}
	
	void init()
	{		DF3DS_RET1_FS_TRACE("FS00 ENTER");

				DF3DS_RET1_FS_TRACE("FS01 engine-create BEGIN");
		// Create the script engine.
		s_engine = asCreateScriptEngine();
		DF3DS_RET1_FS_TRACE("FS02 engine-create PASS");

				DF3DS_RET1_FS_TRACE("FS03 message-callback BEGIN");
		// Set the message callback to receive information on errors in human readable form.
		s32 res = s_engine->SetMessageCallback(asFUNCTION(messageCallback), 0, asCALL_CDECL);
		DF3DS_RET1_FS_TRACE("FS04 message-callback PASS");
				DF3DS_RET1_FS_TRACE("FS05 assert BEGIN");
		assert(res >= 0);
		DF3DS_RET1_FS_TRACE("FS06 assert PASS");

				DF3DS_RET1_FS_TRACE("FS07 register-string BEGIN");
		// Register std::string as the script string type.
		RegisterStdString(s_engine);
		DF3DS_RET1_FS_TRACE("FS08 register-string PASS");
				DF3DS_RET1_FS_TRACE("FS09 GetStdStringObjectId BEGIN");
		s_typeId[FSTYPE_STRING] = GetStdStringObjectId();
		DF3DS_RET1_FS_TRACE("FS10 GetStdStringObjectId PASS");

				DF3DS_RET1_FS_TRACE("FS11 register-array BEGIN");
		// Register the default array type.
		RegisterScriptArray(s_engine, true);
		DF3DS_RET1_FS_TRACE("FS12 register-array PASS");
				DF3DS_RET1_FS_TRACE("FS13 GetScriptArrayObjectId BEGIN");
		s_typeId[FSTYPE_ARRAY] = GetScriptArrayObjectId();
		DF3DS_RET1_FS_TRACE("FS14 GetScriptArrayObjectId PASS");

				DF3DS_RET1_FS_TRACE("FS15 s_engine-RegisterGlobalFunct BEGIN");
		// Language features.
		res = s_engine->RegisterGlobalFunction("void yield(float = 0.0)", asFUNCTION(yield), asCALL_CDECL);
		DF3DS_RET1_FS_TRACE("FS16 s_engine-RegisterGlobalFunct PASS");  DF3DS_RET1_FS_TRACE("FS17 assert BEGIN");
 assert(res >= 0);
 DF3DS_RET1_FS_TRACE("FS18 assert PASS");
				DF3DS_RET1_FS_TRACE("FS19 s_engine-RegisterGlobalFunct BEGIN");
		res = s_engine->RegisterGlobalFunction("void abort(const string &in)", asFUNCTION(abort), asCALL_CDECL);
		DF3DS_RET1_FS_TRACE("FS20 s_engine-RegisterGlobalFunct PASS");  DF3DS_RET1_FS_TRACE("FS21 assert BEGIN");
 assert(res >= 0);
 DF3DS_RET1_FS_TRACE("FS22 assert PASS");

				DF3DS_RET1_FS_TRACE("FS23 registerScriptMath_float2 BEGIN");
		registerScriptMath_float2(s_engine);
		DF3DS_RET1_FS_TRACE("FS24 registerScriptMath_float2 PASS");
				DF3DS_RET1_FS_TRACE("FS25 registerScriptMath_float3 BEGIN");
		registerScriptMath_float3(s_engine);
		DF3DS_RET1_FS_TRACE("FS26 registerScriptMath_float3 PASS");
				DF3DS_RET1_FS_TRACE("FS27 registerScriptMath_float4 BEGIN");
		registerScriptMath_float4(s_engine);
		DF3DS_RET1_FS_TRACE("FS28 registerScriptMath_float4 PASS");
				DF3DS_RET1_FS_TRACE("FS29 registerScriptMath_float2x2 BEGIN");
		registerScriptMath_float2x2(s_engine);
		DF3DS_RET1_FS_TRACE("FS30 registerScriptMath_float2x2 PASS");
				DF3DS_RET1_FS_TRACE("FS31 registerScriptMath_float3x3 BEGIN");
		registerScriptMath_float3x3(s_engine);
		DF3DS_RET1_FS_TRACE("FS32 registerScriptMath_float3x3 PASS");
				DF3DS_RET1_FS_TRACE("FS33 registerScriptMath_float4x4 BEGIN");
		registerScriptMath_float4x4(s_engine);
		DF3DS_RET1_FS_TRACE("FS34 registerScriptMath_float4x4 PASS");
				DF3DS_RET1_FS_TRACE("FS35 getFloat2ObjectId BEGIN");
		s_typeId[FSTYPE_FLOAT2] = getFloat2ObjectId();
		DF3DS_RET1_FS_TRACE("FS36 getFloat2ObjectId PASS");
				DF3DS_RET1_FS_TRACE("FS37 getFloat3ObjectId BEGIN");
		s_typeId[FSTYPE_FLOAT3] = getFloat3ObjectId();
		DF3DS_RET1_FS_TRACE("FS38 getFloat3ObjectId PASS");
				DF3DS_RET1_FS_TRACE("FS39 getFloat4ObjectId BEGIN");
		s_typeId[FSTYPE_FLOAT4] = getFloat4ObjectId();
		DF3DS_RET1_FS_TRACE("FS40 getFloat4ObjectId PASS");
				DF3DS_RET1_FS_TRACE("FS41 getFloat2x2ObjectId BEGIN");
		s_typeId[FSTYPE_FLOAT2x2] = getFloat2x2ObjectId();
		DF3DS_RET1_FS_TRACE("FS42 getFloat2x2ObjectId PASS");
				DF3DS_RET1_FS_TRACE("FS43 getFloat3x3ObjectId BEGIN");
		s_typeId[FSTYPE_FLOAT3x3] = getFloat3x3ObjectId();
		DF3DS_RET1_FS_TRACE("FS44 getFloat3x3ObjectId PASS");
				DF3DS_RET1_FS_TRACE("FS45 getFloat4x4ObjectId BEGIN");
		s_typeId[FSTYPE_FLOAT4x4] = getFloat4x4ObjectId();
		DF3DS_RET1_FS_TRACE("FS46 getFloat4x4ObjectId PASS");

				DF3DS_RET1_FS_TRACE("FS47 clear BEGIN");
		s_modules.clear();
		DF3DS_RET1_FS_TRACE("FS48 clear PASS");
	}

	void destroy()
	{
		// Clean up
		stopAllFunc();
		s_modules.clear();
		if (s_engine)
		{
			s_engine->ShutDownAndRelease();
			s_engine = nullptr;
		}
	}

	void overrideCallback(ScriptMessageCallback callback)
	{
		s_msgCallback = callback;
	}

	typedef void(*SerializeTypeFunc)(Stream*, void** ptr, const asITypeInfo*, bool);

	std::map<std::string, SerializeTypeFunc> s_serializeTypeMap;
	SerializeTypeFunc getSerializeTypeFunc(const char* name);

	void serializeArray(Stream* stream, CScriptArray** array, const asITypeInfo* typeInfo, bool allocate)
	{
		if (allocate && array && serialization_getMode() == SMODE_READ)
		{
			*array = CScriptArray::Create((asITypeInfo*)typeInfo);
		}
		
		// Array size.
		u32 arrayLen = serialization_getMode() == SMODE_WRITE && array ? (*array)->GetSize() : 0;
		SERIALIZE(SaveVersionLevelScriptV1, arrayLen, 0);
		if (serialization_getMode() == SMODE_READ) { (*array)->Resize(arrayLen); }
		if (arrayLen == 0) { return; }

		// Array data.
		s32 elementTypeId = (*array)->GetElementTypeId();
		s32 size = s_engine->GetSizeOfPrimitiveType(elementTypeId);
		asITypeInfo* elementType = s_engine->GetTypeInfoById(elementTypeId);
		assert(elementType);
		SerializeTypeFunc elemFunc = getSerializeTypeFunc(elementType->GetName());

		if (elemFunc)
		{
			for (u32 i = 0; i < arrayLen; i++)
			{
				void* ptrAt = (*array)->At(i);
				elemFunc(stream, &ptrAt, elementType, allocate);
			}
		}
		else if (size > 0 || (elementType->GetFlags() & asOBJ_POD))
		{
			size = size ? size : elementType->GetSize();
			for (u32 i = 0; i < arrayLen; i++)
			{
				SERIALIZE_BUF(SaveVersionLevelScriptV1, (*array)->At(i), size);
			}
		}
		else
		{
			TFE_System::logWrite(LOG_ERROR, "Force Script", "No serialization function for type: %s", elementType->GetName());
			assert(0);
		}
	}

	void serializeString(Stream* stream, std::string** str, const asITypeInfo* typeInfo, bool allocate)
	{
		if (str)
		{
			SERIALIZE_STRING(SaveVersionLevelScriptV1, *(*str));
		}
	}

	void registerSerializeTypeFunc(const char* name, SerializeTypeFunc func)
	{
		s_serializeTypeMap[name] = func;
	}

	SerializeTypeFunc getSerializeTypeFunc(const char* name)
	{
		SerializeTypeFunc func = s_serializeTypeMap[name];
		if (!func)
		{
			TFE_System::logWrite(LOG_ERROR, "Force Script", "No serialization function for type: %s", name);
		}
		return func;
	}

	void initSerliazableTypes()
	{
		if (!s_serializeTypeMap.empty()) { return; }
		registerSerializeTypeFunc("array", (SerializeTypeFunc)serializeArray);
		registerSerializeTypeFunc("string", (SerializeTypeFunc)serializeString);
	}

	void serializeVariable(Stream* stream, s32 typeId, void*& varAddr, const char* name, bool allocateObjects)
	{
		const s32 size = s_engine->GetSizeOfPrimitiveType(typeId);
		const asITypeInfo* type = s_engine->GetTypeInfoById(typeId);
		if (typeId & asTYPEID_OBJHANDLE)
		{
			TFE_System::logWrite(LOG_WARNING, "Force Script", "Serializing object handles is not supported.");
			return;
		}
		else if (typeId & asTYPEID_SCRIPTOBJECT)
		{
			TFE_System::logWrite(LOG_WARNING, "Force Script", "Serializing script objects is not supported.");
			return;
		}
		else if (size > 0)
		{
			// Primitive type.
			if (varAddr) { SERIALIZE_BUF(SaveVersionLevelScriptV1, varAddr, size); }
		}
		else if (type && type->GetFlags() & asOBJ_POD)
		{
			// POD object type (float2, Sector, etc.).
			const u32 typeSize = type->GetSize();
			if (allocateObjects && serialization_getMode() == SMODE_READ)
			{
				void** varHandle = (void**)varAddr;
				*varHandle = asAllocMem(typeSize);
				varAddr = *varHandle;
			}
			else if (allocateObjects && !varAddr)
			{
				// We need to write something here...
				u8 empty[256];
				memset(empty, 0, typeSize);
				SERIALIZE_BUF(SaveVersionLevelScriptV1, empty, typeSize);
			}

			if (varAddr) { SERIALIZE_BUF(SaveVersionLevelScriptV1, varAddr, typeSize); }
		}
		else if (type)
		{
			// Complex script type (array, string, dictionary, etc.).
			const char* typeName = type->GetName();
			assert(typeName);

			SerializeTypeFunc func = getSerializeTypeFunc(typeName);
			if (func)
			{
				void** addr = nullptr;
				if (allocateObjects && varAddr && serialization_getMode() == SMODE_READ)
				{
					addr = (void**)varAddr;
				}
				else if (varAddr)
				{
					addr = &varAddr;
				}

				func(stream, addr, type, allocateObjects);
			}
			else
			{
				TFE_System::logWrite(LOG_ERROR, "Force Script", "No serialization function for type: %s", typeName);
				assert(0);
			}
		}
		else
		{
			TFE_System::logWrite(LOG_ERROR, "Force Script", "Type of variable \"%s\" has no type info, cannot serialize.", name);
			assert(0);
		}
	}

	enum ScriptSerializeOptFlags
	{
		OPT_NONE = 0,
		OPT_SYS_FUNC  = FLAG_BIT(0),
		OPT_INIT_FUNC = FLAG_BIT(1),
		OPT_OBJ_REG   = FLAG_BIT(2)
	};

	void serialize(Stream* stream)
	{
		if (serialization_getVersion() < SaveVersionLevelScriptV1)
		{
			return;
		}
		initSerliazableTypes();

		s32 moduleCount = serialization_getMode() == SMODE_WRITE ? (s32)s_modules.size() : 0;
		s32 scriptThreadCount = serialization_getMode() == SMODE_WRITE ? (s32)s_scriptThreads.size() : 0;
		s32 freeThreadCount = serialization_getMode() == SMODE_WRITE ? (s32)s_freeThreads.size() : 0;
		SERIALIZE(SaveVersionLevelScriptV1, moduleCount, 0);
		SERIALIZE(SaveVersionLevelScriptV1, scriptThreadCount, 0);
		SERIALIZE(SaveVersionLevelScriptV1, freeThreadCount, 0);

		/////////////////////////////////////////////////////////
		// Serialize Script Modules
		/////////////////////////////////////////////////////////
		std::vector<ModuleDef> modDef;
		if (serialization_getMode() == SMODE_READ)
		{
			modDef.resize(moduleCount);
		}
		else
		{
			modDef = s_modules;
		}
		
		std::string nameStr, nameSpaceStr;
		for (s32 m = 0; m < moduleCount; m++)
		{
			ModuleDef* curModDef = &modDef[m];

			SERIALIZE_STRING(SaveVersionLevelScriptV1, curModDef->name);
			SERIALIZE_STRING(SaveVersionLevelScriptV1, curModDef->path);
			SERIALIZE(SaveVersionLevelScriptV1, curModDef->allowReadFromArchive, false);
			SERIALIZE(SaveVersionLevelScriptV1, curModDef->accessMask, 0);

			u32 globalCount = 0;
			if (serialization_getMode() == SMODE_READ)
			{
				curModDef->mod = (asIScriptModule*)createModule(curModDef->name.c_str(), curModDef->path.c_str(), curModDef->allowReadFromArchive, curModDef->accessMask);
			}
			else
			{
				globalCount = curModDef->mod->GetGlobalVarCount();
			}
			asIScriptModule* mod = curModDef->mod;
			SERIALIZE(SaveVersionLevelScriptV1, globalCount, 0);

			for (u32 g = 0; g < globalCount; g++)
			{
				s32 typeId = 0;
				if (serialization_getMode() == SMODE_WRITE)
				{
					const char* name;
					const char* nameSpace;
					df3dsAsGetGlobalVar(mod, g, &name, &nameSpace, &typeId);
					nameStr = name;
					nameSpaceStr = nameSpace;
				}
				SERIALIZE_STRING(SaveVersionLevelScriptV1, nameStr);
				SERIALIZE_STRING(SaveVersionLevelScriptV1, nameSpaceStr);
				SERIALIZE(SaveVersionLevelScriptV1, typeId, 0);

				if (serialization_getMode() == SMODE_READ)
				{
					const char* readName;
					const char* readNameSpace;
					s32 readTypeId = 0;
					// This needs to be called to initialize the type info.
					df3dsAsGetGlobalVar(mod, g, &readName, &readNameSpace, &readTypeId);
					assert(readTypeId == typeId);
				}

				s32 varIndex = g;
				if (serialization_getMode() == SMODE_READ)
				{
					varIndex = mod->GetGlobalVarIndexByName(nameStr.c_str());
					assert(varIndex >= 0);
				}
				void* varAddr = mod->GetAddressOfGlobalVar(varIndex);
				serializeVariable(stream, typeId, varAddr, nameStr.c_str());
			}
		}

		/////////////////////////////////////////////////////////
		// Serialize Script Threads
		/////////////////////////////////////////////////////////
		if (serialization_getMode() == SMODE_READ)
		{
			s_freeThreads.resize(freeThreadCount);
			s_scriptThreads.resize(scriptThreadCount);
		}
		SERIALIZE_BUF(SaveVersionLevelScriptV1, s_freeThreads.data(), sizeof(s32) * freeThreadCount);

		ScriptThread* thread = s_scriptThreads.data();
		for (s32 i = 0; i < scriptThreadCount; i++, thread++)
		{
			SERIALIZE(SaveVersionLevelScriptV1, thread->delay, 0.0f);
			if (serialization_getMode() == SMODE_WRITE)
			{
				asIScriptContext* context = thread->asContext;
				u32 callstackSize = context ? context->GetCallstackSize() : 0;
				SERIALIZE(SaveVersionLevelScriptV1, callstackSize, 0);
				if (!callstackSize)
				{
					assert(!context);
					continue;
				}

				for (u32 c = 0; c < callstackSize; c++)
				{
					u32 stackFramePtr = 0, programPtr = 0, stackPtr = 0, stackIndex = 0;
					asIScriptFunction* scriptFunc = nullptr;
					// Serialize the stack data.
					s32 res = context->GetCallStateRegisters(c, (asDWORD*)&stackFramePtr, &scriptFunc, (asDWORD*)&programPtr, (asDWORD*)&stackPtr, (asDWORD*)&stackIndex);
					if (res != asSUCCESS)
					{
						TFE_System::logWrite(LOG_ERROR, "Force Script", "Cannot serialize script context, error = %x.", res);
					}
					SERIALIZE(SaveVersionLevelScriptV1, stackFramePtr, 0);
					SERIALIZE(SaveVersionLevelScriptV1, programPtr, 0);
					SERIALIZE(SaveVersionLevelScriptV1, stackPtr, 0);
					SERIALIZE(SaveVersionLevelScriptV1, stackIndex, 0);
					// Serialize the function/module.
					const char* modName = scriptFunc->GetModuleName();
					const char* funcName = scriptFunc->GetName();
					SERIALIZE_CSTRING_WRITE(SaveVersionLevelScriptV1, modName);
					SERIALIZE_CSTRING_WRITE(SaveVersionLevelScriptV1, funcName);
					// Serialize local variables.
					u32 localCount = context->GetVarCount();
					SERIALIZE(SaveVersionLevelScriptV1, localCount, 0);
					for (u32 v = 0; v < localCount; v++)
					{
						const char* varName = nullptr;
						s32 typeId = 0, stackOffset = 0;
						u32 typeMod = 0;
						bool isOnHeap = false;
						res = df3dsAsGetVar(context, v, c, &varName, &typeId, (asETypeModifiers*)&typeMod, &isOnHeap, &stackOffset);
						if (res != asSUCCESS)
						{
							TFE_System::logWrite(LOG_ERROR, "Force Script", "Cannot serialize script context local variable, error = %x.", res);
						}

						void* varPtr = context->GetAddressOfVar(v, c);
						serializeVariable(stream, typeId, varPtr, varName, isOnHeap);
					}
					// Serialize stack arguments.
					u32 argCount = context->GetArgsOnStackCount(c);
					SERIALIZE(SaveVersionLevelScriptV1, argCount, 0);
					for (u32 a = 0; a < argCount; a++)
					{
						s32 typeId = 0;
						u32 flags = 0;
						void* addr = nullptr;
						df3dsAsGetArgOnStack(context, c, a, &typeId, &flags, &addr);
						serializeVariable(stream, typeId, addr, "");
					}

					// Additional context registers.
					asIScriptFunction* callingSysFunc = nullptr;
					asIScriptFunction* initFunc = nullptr;
					u32 origStackPtr = 0, argSize = 0;
					u64 valueReg = 0;
					void* objReg = nullptr;
					asITypeInfo* objRegType = nullptr;
					context->GetStateRegisters(c, &callingSysFunc, &initFunc, (asDWORD*)&origStackPtr, (asDWORD*)&argSize, (asQWORD*)&valueReg, &objReg, &objRegType);

					u32 optFlags = OPT_NONE;
					if (callingSysFunc) { optFlags |= OPT_SYS_FUNC; }
					if (initFunc) { optFlags |= OPT_INIT_FUNC; }
					if (objReg && objRegType) { optFlags |= OPT_OBJ_REG; }
					SERIALIZE(SaveVersionLevelScriptV1, optFlags, 0);

					if (optFlags & OPT_SYS_FUNC)
					{
						SERIALIZE_CSTRING_WRITE(SaveVersionLevelScriptV1, callingSysFunc->GetName());
					}
					if (optFlags & OPT_INIT_FUNC)
					{
						SERIALIZE_CSTRING_WRITE(SaveVersionLevelScriptV1, initFunc->GetName());
					}

					SERIALIZE(SaveVersionLevelScriptV1, origStackPtr, 0);
					SERIALIZE(SaveVersionLevelScriptV1, argSize, 0);
					SERIALIZE(SaveVersionLevelScriptV1, valueReg, 0);

					if (optFlags & OPT_OBJ_REG)
					{
						s32 typeId = objRegType->GetTypeId();
						serializeVariable(stream, typeId, objReg, "");
					}
				}
			}
			else
			{
				u32 callstackSize = 0;
				SERIALIZE(SaveVersionLevelScriptV1, callstackSize, 0);
				if (callstackSize == 0)
				{
					thread->asContext = nullptr;
					continue;
				}

				thread->asContext = s_engine->RequestContext();
				asIScriptContext* context = thread->asContext;
				assert(context);
				
				if (context->StartDeserialization() == asSUCCESS)
				{
					asIScriptModule* curMod = nullptr;
					s32 res = asSUCCESS;
					for (u32 c = 0; c < callstackSize; c++)
					{
						u32 stackFramePtr = 0, programPtr = 0, stackPtr = 0, stackIndex = 0;
						SERIALIZE(SaveVersionLevelScriptV1, stackFramePtr, 0);
						SERIALIZE(SaveVersionLevelScriptV1, programPtr, 0);
						SERIALIZE(SaveVersionLevelScriptV1, stackPtr, 0);
						SERIALIZE(SaveVersionLevelScriptV1, stackIndex, 0);
						// Serialize the function/module.
						char modName[256], funcName[256];
						SERIALIZE_CSTRING(SaveVersionLevelScriptV1, modName);
						SERIALIZE_CSTRING(SaveVersionLevelScriptV1, funcName);
						thread->name = modName;
						thread->funcName = funcName;

						asIScriptModule* mod = s_engine->GetModule(modName);
						asIScriptFunction* scriptFunc = mod->GetFunctionByName(funcName);
						assert(mod == curMod || !curMod);
						curMod = mod;

						res = context->PushFunction(scriptFunc, nullptr); assert(res >= 0);
						res = context->SetCallStateRegisters(c, stackFramePtr, scriptFunc, programPtr, stackPtr, stackIndex); assert(res >= 0);

						// Local Variables.
						u32 localCount = 0;
						SERIALIZE(SaveVersionLevelScriptV1, localCount, 0);
						if (localCount != context->GetVarCount(c))
						{
							TFE_System::logWrite(LOG_ERROR, "Force Script", "Failed to load saved game, script code no longer matches.");
							assert(0);
							return;
						}

						for (u32 v = 0; v < localCount; v++)
						{
							const char* varName = nullptr;
							s32 typeId = 0, stackOffset = 0;
							u32 typeMod = 0;
							bool isOnHeap = false;
							res = df3dsAsGetVar(context, v, c, &varName, &typeId, (asETypeModifiers*)&typeMod, &isOnHeap, &stackOffset);
							if (res != asSUCCESS)
							{
								TFE_System::logWrite(LOG_ERROR, "Force Script", "Cannot serialize script context local variable, error = %x.", res);
							}

							void* varPtr = context->GetAddressOfVar(v, c, isOnHeap);
							serializeVariable(stream, typeId, varPtr, varName, isOnHeap);
						}

						// Serialize stack arguments.
						u32 argCount = 0;
						SERIALIZE(SaveVersionLevelScriptV1, argCount, 0);
						assert(argCount == (u32)context->GetArgsOnStackCount(c));
						for (u32 a = 0; a < argCount; a++)
						{
							s32 typeId = 0;
							u32 flags = 0;
							void* addr = nullptr;
							
							res = df3dsAsGetArgOnStack(context, c, a, &typeId, &flags, &addr); assert(res >= 0);
							serializeVariable(stream, typeId, addr, "");
						}

						// Additional context registers.
						asIScriptFunction* callingSysFunc = nullptr;
						asIScriptFunction* initFunc = nullptr;
						u32 origStackPtr = 0, argSize = 0;
						u64 valueReg = 0;
												
						u32 optFlags = OPT_NONE;
						SERIALIZE(SaveVersionLevelScriptV1, optFlags, OPT_NONE);

						if (optFlags & OPT_SYS_FUNC)
						{
							char name[256];
							SERIALIZE_CSTRING(SaveVersionLevelScriptV1, name);
							callingSysFunc = curMod->GetFunctionByName(name);
						}
						if (optFlags & OPT_INIT_FUNC)
						{
							char name[256];
							SERIALIZE_CSTRING(SaveVersionLevelScriptV1, name);
							initFunc = curMod->GetFunctionByName(name);
						}

						SERIALIZE(SaveVersionLevelScriptV1, origStackPtr, 0);
						SERIALIZE(SaveVersionLevelScriptV1, argSize, 0);
						SERIALIZE(SaveVersionLevelScriptV1, valueReg, 0);

						void* objReg = nullptr;
						asITypeInfo* objRegType = nullptr;
						if (optFlags & OPT_OBJ_REG)
						{
							// TODO
							assert(0);
						}
						res = context->SetStateRegisters(c, callingSysFunc, initFunc, origStackPtr, argSize, valueReg, objReg, objRegType);
						assert(res >= 0);
					}

					// Finish
					const s32 serializationResult = context->FinishDeserialization();
					if (serializationResult != asSUCCESS)
					{
						TFE_System::logWrite(LOG_ERROR, "Force Script", "Failed to load saved game, script code no longer matches.");
						assert(0);
					}
				}
				else
				{
					TFE_System::logWrite(LOG_ERROR, "Force Script", "Failed to load saved game, script serialization failed to start.");
					assert(0);
				}
			}
		}

		// Temp: Not necessary, this is here to catch memory issues.
		nameStr.clear();
		nameSpaceStr.clear();
		modDef.clear();
	}

	void update(f32 dt)
	{
		if (dt == 0.0f) { dt = (f32)TFE_System::getDeltaTime(); }
		const s32 count = (s32)s_scriptThreads.size();
		ScriptThread* thread = s_scriptThreads.data();
		for (s32 i = 0; i < count; i++)
		{
			// Allow for holes to keep IDs consistent.
			// Fill holes with new threads.
			if (thread[i].asContext == nullptr) { continue; }

			thread[i].delay = std::max(0.0f, thread[i].delay - dt);
			if (thread[i].delay == 0.0f)
			{
				asIScriptContext* context = thread[i].asContext;
				const s32 res = context->Execute();
				if (res != asEXECUTION_SUSPENDED)
				{
					// Finally done!
					s_engine->ReturnContext(context);
					thread[i].asContext = nullptr;
					thread[i].delay = 0.0f;
					s_freeThreads.push_back(i);
				}
			}
		}
	}

	void stopAllFunc()
	{
		if (s_engine)
		{
			const s32 count = (s32)s_scriptThreads.size();
			ScriptThread* thread = s_scriptThreads.data();

			for (s32 i = 0; i < count; i++)
			{
				if (thread[i].asContext == nullptr) { continue; }

				asIScriptContext* context = thread[i].asContext;
				context->Abort();
				s_engine->ReturnContext(context);
			}
		}
		// Take this opportunity to defrag.
		s_scriptThreads.clear();
		s_freeThreads.clear();
	}

	void* getEngine()
	{
		return s_engine;
	}

	ModuleHandle getModule(const char* moduleName)
	{
		return s_engine->GetModule(moduleName);
	}

	void deleteModule(const char* moduleName)
	{
		asIScriptModule* mod = s_engine->GetModule(moduleName);
		if (!mod) { return; }
		mod->Discard();

		s32 count = (s32)s_modules.size();
		for (s32 i = 0; i < count; i++)
		{
			if (mod == s_modules[i].mod)
			{
				s_modules.erase(s_modules.begin() + i);
				break;
			}
		}
	}
				
	ModuleHandle createModule(const char* moduleName, const char* filePath, bool allowReadFromArchive, u32 accessMask)
	{
		CScriptBuilder builder;
		builder.SetReadMode(!allowReadFromArchive); // true to read from disk, false to read from the TFE filesystem.
		s32 res = builder.StartNewModule(s_engine, moduleName);
		if (res < 0)
		{
			return nullptr;
		}
		// Set Access Mask
		asIScriptModule* mod = builder.GetModule();
		if (!mod)
		{
			return nullptr;
		}
		mod->SetAccessMask(accessMask);
		// Load the source and build.
		res = builder.AddSectionFromFile(filePath);
		if (res < 0)
		{
			return nullptr;
		}
		res = builder.BuildModule();
		if (res < 0)
		{
			return nullptr;
		}
		mod = builder.GetModule();
		if (mod)
		{
			s_modules.push_back({ moduleName, filePath, allowReadFromArchive, accessMask, mod });
		}
		return mod;
	}
					
	ModuleHandle createModule(const char* moduleName, const char* sectionName, const char* srcCode, u32 accessMask)
	{
		CScriptBuilder builder;
		s32 res = builder.StartNewModule(s_engine, moduleName);
		if (res < 0)
		{
			return nullptr;
		}
		// Set Access Mask
		asIScriptModule* mod = builder.GetModule();
		if (!mod)
		{
			return nullptr;
		}
		mod->SetAccessMask(accessMask);
		// Add the source and build.
		res = builder.AddSectionFromMemory(sectionName, srcCode);
		if (res < 0)
		{
			return nullptr;
		}
		res = builder.BuildModule();
		if (res < 0)
		{
			return nullptr;
		}
		return builder.GetModule();
	}

	FunctionHandle findScriptFuncByDecl(ModuleHandle modHandle, const char* funcDecl)
	{
		if (!modHandle) { return nullptr; }

		// Find the function that is to be called. 
		asIScriptModule* mod = (asIScriptModule*)modHandle;
		if (!mod)
		{
			// The function couldn't be found. Instruct the script writer
			// to include the expected function in the script.
			TFE_System::logWrite(LOG_ERROR, "Script", "Cannot find module '%s'.\n", mod->GetName());
			return nullptr;
		}

		asIScriptFunction* func = mod->GetFunctionByDecl(funcDecl);
		if (!func)
		{
			// The function couldn't be found. Instruct the script writer
			// to include the expected function in the script.
			TFE_System::logWrite(LOG_ERROR, "Script", "The script must have the function with declaration '%s'. Please add it and try again.\n", funcDecl);
			return nullptr;
		}
		return func;
	}

	FunctionHandle findScriptFuncByName(ModuleHandle modHandle, const char* funcName)
	{
		if (!modHandle) { return nullptr; }

		// Find the function that is to be called. 
		asIScriptModule* mod = (asIScriptModule*)modHandle;
		if (!mod)
		{
			// The function couldn't be found. Instruct the script writer
			// to include the expected function in the script.
			TFE_System::logWrite(LOG_ERROR, "Script", "Cannot find module '%s'.\n", mod->GetName());
			return nullptr;
		}

		asIScriptFunction* func = mod->GetFunctionByName(funcName);
		if (!func)
		{
			// The function couldn't be found. Instruct the script writer
			// to include the expected function in the script.
			TFE_System::logWrite(LOG_ERROR, "Script", "The script must have the function with name '%s'. Please add it and try again.\n", funcName);
			return nullptr;
		}
		return func;
	}

	FunctionHandle findScriptFuncByNameNoCase(ModuleHandle modHandle, const char* funcName)
	{
		if (!modHandle) { return nullptr; }

		// Find the function that is to be called. 
		asIScriptModule* mod = (asIScriptModule*)modHandle;
		if (!mod)
		{
			// The function couldn't be found. Instruct the script writer
			// to include the expected function in the script.
			TFE_System::logWrite(LOG_ERROR, "Script", "Cannot find module '%s'.\n", mod->GetName());
			return nullptr;
		}

		// Manually search.
		u32 funcCount = mod->GetFunctionCount();
		for (u32 f = 0; f < funcCount; f++)
		{
			asIScriptFunction* func = mod->GetFunctionByIndex(f);
			const char* name = func->GetName();
			if (strcasecmp(name, funcName) == 0)
			{
				return func;
			}
		}

		// The function couldn't be found. Instruct the script writer
		// to include the expected function in the script.
		TFE_System::logWrite(LOG_ERROR, "Script", "The script must have the function with name '%s'. Please add it and try again.\n", funcName);
		return nullptr;
	}
				
	// Add a script function to be executed during the update.
	s32 execFunc(FunctionHandle funcHandle, s32 argCount, const ScriptArg* arg)
	{
		s32 id = -1;
		if (!funcHandle) { return id; }
		asIScriptFunction* func = (asIScriptFunction*)funcHandle;

		// Get a free context.
		asIScriptContext* context = s_engine->RequestContext();
		if (!context)
		{
			// ERROR
			return id;
		}
		// Then prepare it for execution.
		s32 res = context->Prepare(func);
		if (res < 0)
		{
			s_engine->ReturnContext(context);
			return id;
		}
		// Set arguments.
		argCount = std::min(argCount, (s32)func->GetParamCount());
		for (s32 i = 0; i < argCount; i++)
		{
			switch (arg[i].type)
			{
				case ARG_S32:
				{
					context->SetArgDWord(i, *((asDWORD*)&arg[i].iValue));
				} break;
				case ARG_U32:
				{
					context->SetArgDWord(i, arg[i].uValue);
				} break;
				case ARG_F32:
				{
					context->SetArgFloat(i, arg[i].fValue);
				} break;
				case ARG_BOOL:
				{
					context->SetArgByte(i, arg[i].bValue ? 1 : 0);
				} break;
				case ARG_OBJECT:
				{
					context->SetArgObject(i, arg[i].objPtr);
				} break;
				case ARG_STRING:
				{
					std::string stdStr(arg[i].strValue);
					context->SetArgObject(i, (void*)&stdStr);
				} break;
				case ARG_FLOAT2:
				{
					float2 f2(arg[i].float2Value.x, arg[i].float2Value.z);
					context->SetArgObject(i, (void*)&f2);
				} break;
				case ARG_FLOAT3:
				{
					float3 f3(arg[i].float3Value.x, arg[i].float3Value.y, arg[i].float3Value.z);
					context->SetArgObject(i, (void*)&f3);
				} break;
				case ARG_FLOAT4:
				{
					float4 f4(arg[i].float4Value.x, arg[i].float4Value.y, arg[i].float4Value.z, arg[i].float4Value.w);
					context->SetArgObject(i, (void*)&f4);
				} break;
			};
		}
		// Get the new thread ID and prepare it.
		// The function will be executed during the next update.
		if (!s_freeThreads.empty())
		{
			id = s_freeThreads.back();
			s_freeThreads.pop_back();
		}
		if (id < 0)
		{
			id = (s32)s_scriptThreads.size();
			s_scriptThreads.push_back({});
		}
		s_scriptThreads[id].asContext = context;
		s_scriptThreads[id].delay = 0.0f;
		s_scriptThreads[id].name = func->GetModuleName();
		s_scriptThreads[id].funcName = func->GetName();
		context->SetUserData((void*)((intptr_t)id), ThreadId);

		return id;
	}
}  // TFE_ForceScript
