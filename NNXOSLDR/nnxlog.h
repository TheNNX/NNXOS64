#ifndef NNX_LOG_HEADER
#define NNX_LOG_HEADER

#include "device/fs/vfs.h"
#include "nnxarg.h"

#ifdef __cplusplus
extern "C" {
#endif

void NNXLogG(const char* str, ...);
void NNXClearG();
void NNXSetLoggerG(void*);
void* NNXGetLoggerG(void);
void* NNXNewLoggerG(VFSFile*);
void NNXLoggerTest(VFS* filesystem);

#ifdef __cplusplus
}

class NNXLogger {
public:
	NNXLogger(VFSFile* loggerFile);
	~NNXLogger();
	void Log(const char* str, ...);
	void Clear();
	void Log(const char* str, va_list l);
	void Flush();
private:
	VFSFile* loggerFile;
	VFS* filesystem;
	UINT64 position;
	unsigned char* buffer;
	void AppendText(const char* text, UINT64 textLength);
};

extern NNXLogger* gLogger;

#endif

#endif