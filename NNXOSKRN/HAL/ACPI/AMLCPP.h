#ifndef NNX_AMLCPP_HEADER
#define NNX_AMLCPP_HEADER

#include "AML.h"
#include "nnxllist.hpp"

AMLObjRef CreateAMLObjRef(VOID* pointer, AMLObjectType type);

class AMLNamedObject;
class AMLScope;
class AMLBuffer;
class AMLPackage;
class AMLMethodDef;
class AMLDevice;
class AMLField;
class AMLOpetationRegion;
class AMLFieldUnit;

class AMLNamedObject
{
public:
	AML_NAME name;
	AMLObjRef object;
	AMLNamedObject(AML_NAME name, AMLObjRef object);
	static AMLNamedObject* newObject(AML_NAME name, AMLObjRef object);
};

class AMLBuffer
{
public:
	AMLBuffer(UINT32 size);
	UINT8 *data;
	UINT32 size;
};

class AMLPackage
{
public:
	AMLPackage(UINT8 numberOfElements);
	UINT8 numElements;					// According to ACPI 6.2 Specification 19.3.5 "ASL Data Types", the size of a package cannot exceed 255
	AMLObjRef* elements;
};

class AMLScope
{
public:
	AML_NAME name;
	AMLScope* parent;
	NNXLinkedList<AMLNamedObject*> namedObjects;
	NNXLinkedList<AMLScope*> children;
	NNXLinkedList<AMLDevice*> devices;
	NNXLinkedList<AMLMethodDef*> methods;
	AMLScope();
	static AMLScope* newScope(const char* name, AMLScope* parent);
	AMLOpetationRegion *opRegion;
};

typedef struct
{
	AML_NAME name;
	AMLScope* scope;
}AML_NAME_WITHIN_SCOPE;

class AMLParser
{
public:
	AMLParser(ACPI_DSDT *table);
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
	AMLMethodDef* CreateMethod(AML_NAME methodName, AMLScope* scope);
	AMLScope* FindScope(AML_NAME name, AMLScope* current);

	AML_NAME_WITHIN_SCOPE GetNameWithinAScope(AMLScope* current);

	UINT32 DecodePkgLenght();
	UINT8 DecodePackageNumElements();
	VOID GetString(UINT8* output, UINT32 lenght);
	VOID GetString(UINT8* output, UINT32 lenght, UINT8 terminator);
	UINT8 GetByte();
	UINT16 GetWord();
	UINT32 GetDword();
	UINT64 GetQword();
	UINT64 GetInteger();
	UINT32 DecodeBufferSize();
	AMLBuffer * ReadBufferData();
	AMLBuffer * AmlToBuffer(AMLObjRef data);
	AMLBuffer * CreateBufferFromBufferData();
	AMLBuffer * CreateBufferFromType5Opcode(UINT8 opcode);
	AMLObjRef GetAmlObject(UINT8 opcode);
	AMLOpetationRegion* CreateOperationRegion(AML_NAME_WITHIN_SCOPE opRegionName);
	AMLField* CreateField(AMLOpetationRegion* parent, UINT8 access, UINT8 lock, UINT8 updateRule);
	AMLFieldUnit* CreateFieldUnit(AML_NAME fieldUnitName, UINT8 width);
	AML_NAME GetName();
	AMLScope root;
};

void PrintName(AML_NAME name);
inline bool operator==(const AML_NAME& name1, const AML_NAME& name2)
{
	return (*((DWORD*) name1.name) == *((DWORD*) name2.name));
}

void InitializeNamespace(AMLScope* root);

class AMLDevice : public AMLScope
{
private:
	AMLNamedObject* _HID;
public:
	void InitHid();
	AMLNamedObject* GetHid();
	static AMLDevice* newScope(const char* name, AMLScope* parent);
};

UINT64 GetIntegerFromAmlObjRef(AMLObjectReference objRef);

class AMLMethodDef
{
public:
	AML_NAME_WITHIN_SCOPE name;
	UINT8 parameterNumber;
	AMLParser* parser;
	UINT64 codeIndex;
	UINT64 maxCodeIndex;

	AMLMethodDef(AML_NAME, AMLParser*, AMLScope*);
};

class AMLFieldUnit
{
public:
	AML_NAME name;
	UINT8 width;
};

class AMLField
{
public:
	AMLOpetationRegion* parent;
	UINT8 accessWidth;
	UINT8 lock;
	UINT8 updateRule;
	NNXLinkedList<AMLFieldUnit*> fieldUnits;
};

class AMLOpetationRegion
{
public:
	UINT8 type;
	UINT64 offset;
	UINT64 lenght;
	NNXLinkedList<AMLField*> fields;
	AMLOpetationRegion* previous;
	AML_NAME_WITHIN_SCOPE name;
};

#endif