#pragma once


struct SkContext;
struct CBackend;
struct AstFile;


CBackend* CreateCBackend(SkContext* context);
void DestroyCBackend(CBackend* cb);

bool CBackendCompile(CBackend* cb, AstFile** asts, int numModules, const char* filename, const char* buildFolder);
