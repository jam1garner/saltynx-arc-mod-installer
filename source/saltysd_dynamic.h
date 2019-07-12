#ifndef SALTYSD_DYNAMIC_H
#define SALTYSD_DYNAMIC_H

#include <stdint.h>

#include "useful.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint64_t SaltySDCore_GetSymbolAddr(void* base, char* name) LINKABLE;
extern uint64_t SaltySDCore_FindSymbol(char* name) LINKABLE;
extern uint64_t SaltySDCore_FindSymbolBuiltin(char* name) LINKABLE;
extern void SaltySDCore_RegisterModule(void* base) LINKABLE;
extern void SaltySDCore_RegisterBuiltinModule(void* base) LINKABLE;
extern void SaltySDCore_DynamicLinkModule(void* base) LINKABLE;
extern void SaltySDCore_ReplaceModuleImport(void* base, char* name, void* newfunc) LINKABLE;
extern void SaltySDCore_ReplaceImport(char* name, void* newfunc) LINKABLE;

#ifdef __cplusplus
}
#endif

#endif // SALTYSD_DYNAMIC_H
