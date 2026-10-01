// DF3DS V36 - non-ImGui Console/CVar compatibility owner.
#include <TFE_FrontEndUI/console.h>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <string>
#include <vector>

namespace TFE_Console
{
    struct CommandRecord
    {
        std::string name;
        std::string help;
        ConsoleFunc func;
        u32 argCount;
        bool repeat;
    };

    static std::vector<CVar> s_vars;
    static std::vector<CommandRecord> s_commands;

    static CVar* findCVar(const char* name)
    {
        if (!name)
            return nullptr;

        for (CVar& var : s_vars)
        {
            if (strcasecmp(name, var.name.c_str()) == 0)
                return &var;
        }
        return nullptr;
    }

    CVar* getCVar(const char* name)
    {
        return findCVar(name);
    }

    static CVar& makeCVar(
        const char* name,
        const char* help,
        u32 flags,
        CVarType type
    )
    {
        CVar var{};
        var.name = name ? name : "";
        var.helpString = help ? help : "";
        var.type = type;
        var.flags = flags;
        var.valuePtr = nullptr;
        s_vars.push_back(var);
        return s_vars.back();
    }

    bool init() { return true; }

    void destroy()
    {
        s_commands.clear();
        s_vars.clear();
    }

    void registerCVarInt(
        const char* name,
        u32 flags,
        s32* value,
        const char* help
    )
    {
        if (!value) return;

        CVar* var = findCVar(name);
        if (var)
        {
            const bool serialized =
                var->valuePtr == nullptr &&
                var->type == CVAR_INT;
            const s32 serializedValue = var->serializedInt;

            var->defaultInt = *value;
            if (serialized && !(flags & CVFLAG_DO_NOT_SERIALIZE))
                *value = serializedValue;

            var->helpString = help ? help : "";
            var->type = CVAR_INT;
            var->flags = flags;
            var->valueInt = value;
            return;
        }

        CVar& created = makeCVar(name, help, flags, CVAR_INT);
        created.valueInt = value;
        created.defaultInt = *value;
        created.serializedInt = *value;
    }

    void registerCVarFloat(
        const char* name,
        u32 flags,
        f32* value,
        const char* help
    )
    {
        if (!value) return;

        CVar* var = findCVar(name);
        if (var)
        {
            const bool serialized =
                var->valuePtr == nullptr &&
                var->type == CVAR_FLOAT;
            const f32 serializedValue = var->serializedFlt;

            var->defaultFlt = *value;
            if (serialized && !(flags & CVFLAG_DO_NOT_SERIALIZE))
                *value = serializedValue;

            var->helpString = help ? help : "";
            var->type = CVAR_FLOAT;
            var->flags = flags;
            var->valueFloat = value;
            return;
        }

        CVar& created = makeCVar(name, help, flags, CVAR_FLOAT);
        created.valueFloat = value;
        created.defaultFlt = *value;
        created.serializedFlt = *value;
    }

    void registerCVarBool(
        const char* name,
        u32 flags,
        bool* value,
        const char* help
    )
    {
        if (!value) return;

        CVar* var = findCVar(name);
        if (var)
        {
            const bool serialized =
                var->valuePtr == nullptr &&
                var->type == CVAR_BOOL;
            const bool serializedValue = var->serializedBool;

            var->defaultBool = *value;
            if (serialized && !(flags & CVFLAG_DO_NOT_SERIALIZE))
                *value = serializedValue;

            var->helpString = help ? help : "";
            var->type = CVAR_BOOL;
            var->flags = flags;
            var->valueBool = value;
            return;
        }

        CVar& created = makeCVar(name, help, flags, CVAR_BOOL);
        created.valueBool = value;
        created.defaultBool = *value;
        created.serializedBool = *value;
    }

    void registerCVarString(
        const char* name,
        u32 flags,
        char* value,
        u32 maxLen,
        const char* help
    )
    {
        if (!value) return;

        CVar* var = findCVar(name);
        if (var)
        {
            const bool serialized =
                var->valuePtr == nullptr &&
                var->type == CVAR_STRING;
            const std::string serializedValue =
                var->serializedString;

            var->defaultString = value;

            if (serialized && !(flags & CVFLAG_DO_NOT_SERIALIZE))
            {
                const size_t n = std::min(
                    serializedValue.size(),
                    static_cast<size_t>(maxLen)
                );
                std::memcpy(value, serializedValue.data(), n);
                value[n] = 0;
            }

            var->helpString = help ? help : "";
            var->type = CVAR_STRING;
            var->flags = flags;
            var->maxLen = maxLen;
            var->valueString = value;
            return;
        }

        CVar& created = makeCVar(name, help, flags, CVAR_STRING);
        created.valueString = value;
        created.maxLen = maxLen;
        created.defaultString = value;
        created.serializedString = value;
    }

    void addSerializedCVarInt(const char* name, s32 value)
    {
        if (findCVar(name)) return;
        CVar& var = makeCVar(name, "", 0, CVAR_INT);
        var.serializedInt = value;
        var.valuePtr = nullptr;
    }

    void addSerializedCVarFloat(const char* name, f32 value)
    {
        if (findCVar(name)) return;
        CVar& var = makeCVar(name, "", 0, CVAR_FLOAT);
        var.serializedFlt = value;
        var.valuePtr = nullptr;
    }

    void addSerializedCVarBool(const char* name, bool value)
    {
        if (findCVar(name)) return;
        CVar& var = makeCVar(name, "", 0, CVAR_BOOL);
        var.serializedBool = value;
        var.valuePtr = nullptr;
    }

    void addSerializedCVarString(
        const char* name,
        const char* value
    )
    {
        if (findCVar(name)) return;
        CVar& var = makeCVar(name, "", 0, CVAR_STRING);
        var.serializedString = value ? value : "";
        var.valuePtr = nullptr;
    }

    void registerCommand(
        const char* name,
        ConsoleFunc func,
        u32 argCount,
        const char* help,
        bool repeat
    )
    {
        if (!name) return;

        for (const CommandRecord& command : s_commands)
        {
            if (strcasecmp(command.name.c_str(), name) == 0)
                return;
        }

        s_commands.push_back({
            name,
            help ? help : "",
            func,
            argCount,
            repeat
        });
    }

    void execute(const ConsoleArgList& args, const char*)
    {
        if (args.empty()) return;

        for (const CommandRecord& command : s_commands)
        {
            if (strcasecmp(command.name.c_str(), args[0].c_str()) != 0)
                continue;

            if (
                command.func &&
                args.size() - 1 >= command.argCount
            )
            {
                command.func(args);
            }
            return;
        }
    }

    void handleCommand(const char*) {}
    void update() {}
    bool isOpen() { return false; }
    bool isAnimating() { return false; }
    void startOpen() {}
    void startClose() {}
    void addToHistory(const char*) {}

    u32 getCVarCount()
    {
        return static_cast<u32>(s_vars.size());
    }

    const CVar* getCVarByIndex(u32 index)
    {
        if (index >= s_vars.size())
            return nullptr;
        return &s_vars[index];
    }

    void setVariableValue(CVar* var, const char* value)
    {
        if (!var || !var->valuePtr || !value)
            return;

        char* endPtr = nullptr;

        switch (var->type)
        {
            case CVAR_INT:
                *var->valueInt = static_cast<s32>(
                    std::strtol(value, &endPtr, 10)
                );
                break;

            case CVAR_FLOAT:
                *var->valueFloat = static_cast<f32>(
                    std::strtod(value, &endPtr)
                );
                break;

            case CVAR_BOOL:
                if (strcasecmp(value, "true") == 0)
                    *var->valueBool = true;
                else if (strcasecmp(value, "false") == 0)
                    *var->valueBool = false;
                else
                    *var->valueBool =
                        std::strtol(value, &endPtr, 10) != 0;
                break;

            case CVAR_STRING:
            {
                const size_t n = std::min(
                    std::strlen(value),
                    static_cast<size_t>(var->maxLen)
                );
                std::memcpy(var->valueString, value, n);
                var->valueString[n] = 0;
            } break;
        }
    }

    void setVariable(const char* name, const char* value)
    {
        setVariableValue(findCVar(name), value);
    }

    void getVariable(const char* name, char* value)
    {
        if (!value) return;

        value[0] = 0;
        CVar* var = findCVar(name);
        if (!var) return;

        switch (var->type)
        {
            case CVAR_INT:
                std::sprintf(
                    value,
                    "%ld",
                    static_cast<long>(
                        var->valueInt
                            ? *var->valueInt
                            : var->serializedInt
                    )
                );
                break;

            case CVAR_FLOAT:
                std::sprintf(
                    value,
                    "%f",
                    static_cast<double>(
                        var->valueFloat
                            ? *var->valueFloat
                            : var->serializedFlt
                    )
                );
                break;

            case CVAR_BOOL:
                std::strcpy(
                    value,
                    (
                        var->valueBool
                            ? *var->valueBool
                            : var->serializedBool
                    )
                    ? "true"
                    : "false"
                );
                break;

            case CVAR_STRING:
                std::strcpy(
                    value,
                    var->valueString
                        ? var->valueString
                        : var->serializedString.c_str()
                );
                break;
        }
    }

    void registerDefaultCommands() {}
}
