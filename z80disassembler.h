#if !defined(_DISASM_)
#define _DISASM_
#ifdef _WIN32
#include <windows.h>
#define UWORD unsigned int
#else
#include <string.h>
#define UWORD unsigned int
#define ULONG unsigned long
#define CHAR char
#define WORD unsigned int
#define BYTE unsigned char
#define VOID void
#endif
#define Boolean bool
#define STR char*
#define UBYTE unsigned char

void        Disassemble(UWORD adr,STR s);
UBYTE       OpcodeLen(ULONG p);

#endif
