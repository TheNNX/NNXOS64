#pragma once

#include "AML.h"
#include "nnxllist.h"

AMLObjRef CreateAMLObjRef(VOID* pointer, AMLObjectType type);

class AMLNamedObject;
class AMLScope;
class AMLBuffer;
class AMLPackage;
class AMLDevice;

class AMLNamedObject {
public:
	AML_Name name;
	AMLObjRef object;
	AMLNamedObject(AML_Name name, AMLObjRef object);
	static AMLNamedObject* newObject(AML_Name name, AMLObjRef object);
};

class AMLBuffer {
public:
	AMLBuffer(UINT32 size);
	~AMLBuffer();
	UINT8 *data;
private:
	UINT32 size;
};

class AMLPackage {
public:
	AMLPackage(UINT8 numberOfElements);
	UINT8 numElements;					// According to ACPI 6.2 Specification 19.3.5 "ASL Data Types", the size of a package cannot exceed 255
	AMLObjRef* elements;
};

class AMLScope{
public:
	AML_Name name;
	AMLScope* parent;
	NNXLinkedList<AMLNamedObject*> namedObjects;
	NNXLinkedList<AMLScope*> children;
	NNXLinkedList<AMLDevice*> devices;
	AMLScope();
	static AMLScope* newScope(const char* name, AMLScope* parent);
};

typedef struct {
	AML_Name name;
	AMLScope* scope;
}AMLNameWithinAScope;

class ACPI_AML_CODE
{
public:
	ACPI_AML_CODE(ACPI_DSDT *table);
	void Parse(AMLScope* scope, UINT32 size);
	AMLScope* GetRootScope();
	UINT32 GetSize();
private:
	UINT64 index;
	UINT8* table;
	UINT8 name[4];
	UINT32 size;
	UINT8 revision;
	UINT8 checksum;
	UINT8 oemid[6];
	UINT8 tableid[8];
	UINT32 oemRevision;
	UINT8 compilerName[4];
	UINT32 compilerRevision;

	AMLBuffer* CreateBuffer();
	AMLPackage* CreatePackage();
	AMLScope* FindScope(AML_Name name, AMLScope* current);

	AMLNameWithinAScope DecodeNameWithinAScope(AMLScope* current);

	UINT32 DecodePkgLenght();
	UINT8 DecodePackageNumElements();
	VOID GetString(UINT8* output, UINT32 lenght);
	VOID GetString(UINT8* output, UINT32 lenght, UINT8 terminator);
	UINT8 GetByte();
	UINT16 GetWord();
	UINT32 GetDword();
	UINT64 GetQword();
	UINT32 DecodeBufferSize();
	AMLObjRef GetAMLObject(UINT8 opcode);

	AML_Name GetName();
	AMLScope root;
};

void PrintName(AML_Name name);
inline bool operator==(const AML_Name& name1, const AML_Name& name2) {
	return (*((DWORD*)name1.name) == *((DWORD*)name2.name));
}

void InitializeNamespace(AMLScope* root);

class AMLDevice : public AMLScope
{
private:
	AMLNamedObject* _HID;
public:
	void Init_HID();
	AMLNamedObject* Get_HID();
	static AMLDevice* newScope(const char* name, AMLScope* parent);
};

UINT64 GetIntegerFromAMLObjRef(AMLObjectReference objRef);