#pragma once

#define DECLARE_DLL_JMP(name, type, offset)                                    \
    type __declspec(naked) __stdcall name##()                                  \
    {                                                                          \
        __asm { jmp dword ptr [ offset ] }                                     \
    }