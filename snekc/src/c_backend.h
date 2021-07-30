#pragma once


struct SkContext;
struct CBackend;
struct AstModule;


CBackend* CreateCBackend(SkContext* context);
void DestroyCBackend(CBackend* cb);

bool CBackendCompile(CBackend* cb, AstModule** asts, int numModules, const char* filename, const char* buildFolder);
