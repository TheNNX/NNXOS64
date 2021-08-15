#include "nnxlog.h"
#include "text.h"
#include "memory/MemoryOperations.h"

NNXLogger* gLogger = 0;

void NNXLogger::Log(const char* text, va_list l) {
	UINT64 index, textLength = FindCharacterFirst((char*)text, -1, 0);
	char numberBuffer[65];
	void* args = (void*)l;

	if (textLength == 0)
		return;

	for (index = 0; index < textLength;) {
		if (text[index] == '%') {
			index++;
			char nextCharacter = text[index++];
			switch (nextCharacter)
			{
			case '%':
			{
				this->AppendText("%", 1);
				break;
			}
			case 'i':
			case 'd':
			case 'x':
			case 'X':
			case 'u':
			case 'b':
			{
				INT64 i = *((INT64*)args);
				args = ((INT64*)args) + 1;
				if (nextCharacter == 'X')
					IntegerToASCIICapital(i, 16, numberBuffer);
				else if (nextCharacter == 'x')
					IntegerToASCII(i, 16, numberBuffer);
				else if (nextCharacter == 'b') {
					IntegerToASCII(i, 2, numberBuffer);
				}
				else if (nextCharacter == 'u')
					IntegerToASCII(i, -8, numberBuffer);
				else
					IntegerToASCII(i, -10, numberBuffer);

				this->AppendText(numberBuffer, FindCharacterFirst(numberBuffer, -1, 0));
				break;
			}
			case 'c': 
			{
				char i[2];
				i[0] = (*((INT64*)args)) & 0xFF;
				i[1] = 0;
				args = ((INT64*)args) + 1;
				this->AppendText(i, 1);
				break;
			}
			case 's':
			{
				char* str = *((char**)args);
				args = ((INT64*)args) + 1;
				this->AppendText(str, FindCharacterFirst(str, -1, 0));
				break;
			}
			case 'S':
			{
				UINT64 len;
				char* str = *((char**)args);
				args = ((INT64*)args) + 1;
				len = *((UINT64*)args);
				args = ((INT64*)args) + 1;
				this->AppendText(str, len);
				break;
			}
			default:
				break;
			}
		}
		else {
			UINT64 nextPercent = FindCharacterFirst((char*)text + index, textLength - index, '%');
			if (nextPercent == -1) {
				nextPercent = textLength - index;
			}

			this->AppendText(text + index, nextPercent);
			index += nextPercent;
		}
	}
}

void NNXLogger::AppendText(const char* text, UINT64 textLength) {
	for (int i = 0; i < textLength; i++) {
		PrintT("%c", text[i]);
	}
	if (position + textLength >= 512) {
		this->Flush();
	}
	MemCopy(buffer + position, (void*)text, textLength);
	position += textLength; 
	buffer[position] = 0;
}

void NNXLogger::Flush() {
	this->filesystem->functions.AppendFile(this->loggerFile, this->position, this->buffer);
	this->position = 0;
}

NNXLogger::NNXLogger(VFSFile* file) {
	this->loggerFile = file;
	this->filesystem = file->filesystem;
	buffer = new unsigned char[512];
	position = 0;
}

void NNXLogger::Log(const char* str, ...) {
	va_list args;
	va_start(args, str);
	this->Log(str, args);
	va_end(args);
}

void NNXLogger::Clear() {
	this->filesystem->functions.DeleteFile(this->loggerFile);
	this->filesystem->functions.RecreateDeletedFile(this->loggerFile);
	this->position = 0;
}

NNXLogger::~NNXLogger() {
	this->Flush();
	this->filesystem->functions.CloseFile(this->loggerFile);
}

extern "C" void NNXLogG(const char* str, ...) {
	va_list args;
	va_start(args, str);
	gLogger->Log(str, args);
	va_end(args);
}

extern "C" void NNXClearG() {
	gLogger->Clear();
}

extern "C" void NNXSetLoggerG(void* input) {
	if (gLogger)
		delete gLogger;
	gLogger = (NNXLogger*)input;
}

extern "C" void* NNXGetLoggerG() {
	return gLogger;
}

extern "C" void* NNXNewLoggerG(VFSFile* file) {
	NNXSetLoggerG(new NNXLogger(file));
	return (void*)NNXGetLoggerG();
}

extern "C" void NNXLoggerTest(VFS* filesystem) {
	VFSFile* loggerFile;
	if (filesystem->functions.CheckIfFileExists(filesystem, (char*)"LOG.TXT")) {
		if (filesystem->functions.DeleteAndCloseFile(filesystem->functions.OpenFile(filesystem, (char*)"LOG.TXT"))) {
			PrintT("Cannot delete old log\n");
			return;
		}
	}

	if (filesystem->functions.CreateFile(filesystem, (char*)"LOG.TXT")) {
		PrintT("Cannot create file\n");
		return;
	}

	loggerFile = filesystem->functions.OpenFile(filesystem, (char*)"LOG.TXT");
	if (loggerFile) {
		if (gLogger)
			delete gLogger;
		gLogger = new NNXLogger(loggerFile);
	}

	gLogger->Log("the system is alive for sure...");
	gLogger->Log("%s started!\n\n", __FUNCDNAME__);
	gLogger->Log("Numbers:\n Hex: 0x%x 0x%X \n Decimal: %i %d \n Octal: %u\n Binary: %b\n\n", 0xabcd1234LL, 0xabcd1234LL, 0xabcd1234LL, 0xabcd1234LL, 0xabcd1234LL, 0xabcd1234LL);
	gLogger->Log("Strings:\n normal %%s: %s \n fixed size %%S: %S\n\n", "hello i am a string and i am very happy to be able to be shown in all of my length", "hello i am a fixed size string and i'll be cut probably", 26LL);
	gLogger->Log("Characters: \n %c %c %c %c\n\n", 'a', 'b', 'c', 'd');
	gLogger->Log("This will be flushed now...?\n\n");
	gLogger->Flush();
	PrintT("DONE\n");
}