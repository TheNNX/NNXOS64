/*
			WORK IN PROGRESS
		  IT IS NOT FUNCTIONAL

AND THE NAMES OF STRUCTURES AND FUNCTIONS
ARE VERY BAD, LIKE REALLY BAD. DID YOU
EXPECT ME TO ACTUALLY READ THE ACPI/AML
SPECIFIACTION? I'M NOT GOING TO READ THE
1000 PAGES JUST SO SOME NAMES ARE CORRECT.
AS LONG AS IT WORKS (WHICH AT THE MOMENT
IT DOESN'T) IT'S GOOD ENOUGH TO KEEP.
*/

#define PRINT_IN_DEBUG_ONLY

#include "AML.h"
#include "memory/nnxalloc.h"
#include "memory/MemoryOperations.h"

#ifndef  NULL
#define NULL 0
#endif // ! NULL

AML_Name g_HID;

extern "C" {
	extern UINT8 localLastError;
	UINT8 ACPI_ParseDSDT(ACPI_DSDT* table) {
		if (!verifyACPI_DSDT(table))
		{
			localLastError = ACPI_ERROR_INVALID_DSDT;
			return localLastError;
		}
		ACPI_AML_CODE acpi = ACPI_AML_CODE(table);
		PrintT("ACPI_AML_CODE class initialized\n");
		acpi.Parse(acpi.GetRootScope(), acpi.GetSize());
	}
}

ACPI_AML_CODE::ACPI_AML_CODE(ACPI_DSDT* table) {
	this->table = (UINT8*)table;
	this->index = 0;
	GetString(this->name, 4);
	this->size = GetDword();
	this->revision = GetByte();
	this->checksum = GetByte();
	GetString(this->oemid, 6, 0);
	GetString(this->tableid, 8, 0);
	this->oemRevision = GetDword();
	GetString(compilerName, 4);
	this->compilerRevision = GetDword();
	this->root = AMLScope();
	this->size -= index;
	g_HID = CreateName("_HID");
	InitializeNamespace(&(this->root));
}

UINT8 ACPI_AML_CODE::GetByte() {
	index++;
	return table[index-1];
}

UINT16 ACPI_AML_CODE::GetWord() {
	index += 2;
	return *((UINT16*)(table+index-2));
}

UINT32 ACPI_AML_CODE::GetDword() {
	index += 4;
	return *((UINT32*)(table+index-4));
}

UINT64 ACPI_AML_CODE::GetQword() {
	index += 8;
	return *((UINT64*)(table + index - 8));
}

void ACPI_AML_CODE::GetString(UINT8* output, UINT32 lenght) {
	for (UINT32 a = 0; a < lenght; a++) {
		output[a] = table[index];
		index++;
	}
}

void ACPI_AML_CODE::GetString(UINT8* output, UINT32 lenght, UINT8 terminator) {
	for (UINT32 a = 0; a < lenght; a++) {
		if (terminator == table[index])
			break;
		output[a] = table[index];
		index++;
	}
}

AML_Name ACPI_AML_CODE::GetName() {
	UINT32 temp = GetDword();
	return *((AML_Name*)(&temp));
}

void ACPI_AML_CODE::Parse(AMLScope* scope, UINT32 size) {
	PrintT("Parsing for scope: %x ", scope); PrintName(scope->name); PrintT("\n");
	UINT64 indexMax = index + size;
	PrintT("Size: %i (max index: %x)\n\n", size, indexMax);
	while (index < indexMax) {
		UINT8 opcode = GetByte();
		switch (opcode)
		{
		case AML_OPCODE_ZEROOPCODE:
		case AML_OPCODE_ONESOPCODE:
			break;
		case AML_OPCODE_NAMEOPCODE: 
		{
			AMLNameWithinAScope nws = this->DecodeNameWithinAScope(scope);
			AMLNamedObject *namedObject = AMLNamedObject::newObject(nws.name, CreateAMLObjRef(NULL, tAMLInvalid));
			AMLObjRef amlObject = this->GetAMLObject(GetByte());
			namedObject->object = amlObject;
			nws.scope->namedObjects.add(namedObject);
			break;
		}
		case AML_OPCDOE_SCOPEOPCODE:
		{
			UINT64 index1 = index;
			UINT32 PkgLenght = this->DecodePkgLenght();
			
			AMLNameWithinAScope nws = this->DecodeNameWithinAScope(scope);
			AMLScope* _scope = FindScope(nws.name, nws.scope);
			if (_scope == 0) {
				PrintT("Couldn't find object: ");
				PrintName(nws.name);
				while (1);
			}
			UINT64 index2 = index;
			UINT32 size = PkgLenght - (index2 - index1);
			this->Parse(_scope, size);
			break;
		}
		case AML_OPCODE_EXTOPPREFIX:
		{
			UINT8 extOpcode = GetByte();
			switch (extOpcode)
			{
			case AML_OPCODE_EXTOP_DEVICEOPCODE:
			{
				UINT64 index1 = this->index;
				UINT32 PkgLenght = DecodePkgLenght();
				AMLNameWithinAScope nws = DecodeNameWithinAScope(scope);
				UINT64 index2 = this->index;
				UINT32 size = PkgLenght - (index2 - index1);
				PrintT("Device: ");
				PrintName(nws.name);
				PrintT("\n");

				AMLDevice *device = AMLDevice::newScope((const char*)nws.name.name, scope);
				this->Parse(device, size);
				device->Init_HID();
				PrintT("_HID: %x", GetIntegerFromAMLObjRef(->Get_HID()->object));
				break;
			}
			default:
				PrintT("Error: unimplemented ACPI Machine Language opcode 0x5B 0x%X", GetByte());
				while (1);
			}
			
		}
		default:
			PrintT("Error: unimplmeneted ACPI Machine Language opcode: 0x%X\n",opcode);
			while (1);
		}
	}
}

AMLNameWithinAScope ACPI_AML_CODE::DecodeNameWithinAScope(AMLScope* current) {
	AMLScope* dstScope = current;
	if (this->table[index] == AML_OPCODE_DUALNAMEPREFIX || this->table[index] == AML_OPCODE_MULTINAMEPREFIX) {
		UINT8 SegNum = this->table[index] == AML_OPCODE_DUALNAMEPREFIX ? 1 : GetByte() - 1; //substract one, since the last name is going to be read
																							//by the code outside the else-if
		for (UINT8 a = 0; a < SegNum; a++) {

			AML_Name name = GetName();
			dstScope = this->FindScope(name, dstScope);
			if (dstScope == 0) {
				localLastError == ACPI_ERROR_AML_OBJECT_NOT_FOUND;
				AMLNameWithinAScope null = { 0 };
				*((DWORD*)(&null.name)) = 0;
				null.scope = 0;
				return null;
			}
		}
	}
	if (this->table[index] == AML_OPCODE_PARENTPREFIXCHAR) {
		dstScope = dstScope->parent;
	}
	else if (this->table[index] == AML_OPCODE_ROOTCHAROPCODE) {
		dstScope = &(this->root);
	}
	AMLNameWithinAScope name;
	name.name = GetName();
	name.scope = dstScope;
	return name;
}

AMLScope* ACPI_AML_CODE::FindScope(AML_Name name, AMLScope* current) {
	AMLScope* temp = current;

	while (temp) {
		if (temp->name == name) {
			return temp;
		}

		NNXLinkedListEntry<AMLScope*>* scope = current->children.first;

		while (scope) {
			if (scope->value->name == name) {
				return scope->value;
			}
			scope = scope->next;
		}

		temp = temp->parent;
	}

	return 0;
}

AMLNamedObject::AMLNamedObject(AML_Name name, AMLObjRef object) {
	this->name = name;
	this->object = object;
}

AMLNamedObject* AMLNamedObject::newObject(AML_Name name, AMLObjRef object) {
	AMLNamedObject* output = (AMLNamedObject*)nnxmalloc(sizeof(AMLNamedObject));
	*output = AMLNamedObject(name, object);
	PrintT("Allocated new named object for name ");PrintName(output->name);PrintT(" at location %x\n", output);
	return output;
}

void PrintName(AML_Name t) {
	char buffer[5] = { 0 };
	for (int a = 0; a < 4; a++) {
		buffer[a] = t.name[a];
	}
	PrintT("%s", buffer);
}

AMLScope::AMLScope() {
	this->namedObjects = NNXLinkedList<AMLNamedObject*>();
	this->children = NNXLinkedList<AMLScope*>();
	this->parent = 0;
}

UINT32 ACPI_AML_CODE::DecodePkgLenght() {
	UINT8 Byte0 = GetByte();
	UINT8 PkgLengthType = (Byte0 & 0xc0) >> 6;
	if (PkgLengthType) {
		UINT32 result = Byte0 & 0xf;
		for (int a = 0; a < PkgLengthType; a++) {
			result |= (((UINT32)GetByte()) << (4 + 8 * a));
		}
		PrintT("PKGLENGHT: %x\n",result);
		return result;
	}
	else {
		PrintT("PKGLENGHT: %x\n", Byte0 & 0x3f);
		return Byte0 & 0x3f;
	}
}

UINT8 ACPI_AML_CODE::DecodePackageNumElements() {
	UINT8 Byte0 = this->GetByte();
	
	if (Byte0 == AML_OPCODE_BYTEPREFIX) {
		Byte0 = this->GetByte();
	}
	else {
		return Byte0;
	}
	
}

UINT64 GetIntegerFromAMLObjRef(AMLObjectReference objRef) {
	switch (objRef.type)
	{
	case tAMLByte: 
		return *((UINT8*)objRef.pointer);
	case tAMLWord:
		return *((UINT16*)objRef.pointer);
	case tAMLDword:
		return *((UINT32*)objRef.pointer);
	case tAMLQword:
		return *((UINT64*)objRef.pointer);
	case tAMLString:
		return *((UINT64*)objRef.pointer);
	default:
		return 0;
	} 
}


UINT32 ACPI_AML_CODE::DecodeBufferSize() {
	AMLObjRef objRef = GetAMLObject(GetByte());
	return GetIntegerFromAMLObjRef(objRef);
}

AMLBuffer* ACPI_AML_CODE::CreateBuffer() {
	UINT32 pkgLength = DecodePkgLenght();
	UINT32 bufferSize = DecodeBufferSize();
	PrintT("Allocating buffer");
	AMLBuffer* amlBuffer = (AMLBuffer*)nnxmalloc(sizeof(AMLBuffer));
	PrintT(" (%x)", amlBuffer);
	*amlBuffer = AMLBuffer(bufferSize);
	PrintT(" - Done\n");
	for (int a = 0; a < bufferSize; a++) {
		amlBuffer->data[a] = GetByte();
	}
	return amlBuffer;
}

AMLPackage::AMLPackage(UINT8 numElements) {
	this->numElements = numElements;
	this->elements = (AMLObjRef*)nnxcalloc(numElements, sizeof(AMLObjRef));
}

AMLPackage* ACPI_AML_CODE::CreatePackage() {
	UINT32 pkgLenght = DecodePkgLenght();
	UINT8 numElements = DecodePackageNumElements();
	AMLPackage* amlPackage = (AMLPackage*)nnxmalloc(sizeof(AMLPackage));
	*amlPackage = AMLPackage(numElements);

	amlPackage->elements = (AMLObjRef*)(nnxcalloc(numElements, sizeof(AMLObjRef)));
	PrintT("Package elements:\n");
	for (int a = 0; a < numElements; a++) {
		AMLObjRef element = GetAMLObject(GetByte());
		amlPackage->elements[a] = element;
		PrintT("   Type: 0x%x, \n", amlPackage->elements[a].type);
	}

	return amlPackage;
}

AMLObjRef CreateAMLObjRef(VOID* pointer, AMLObjectType type) {
	AMLObjRef result = { 0 };
	result.pointer = pointer;
	result.type = type;
	return result;
}

AMLObjRef ACPI_AML_CODE::GetAMLObject(UINT8 opcode) {

	switch (opcode)
	{
	case AML_OPCODE_ZEROOPCODE:
	case AML_OPCODE_ONEOPCODE:
	case AML_OPCODE_ONESOPCODE:
		return CreateAMLObjRef(&(*((UINT8*)nnxmalloc(sizeof(UINT8))) = opcode), tAMLByte);
	case AML_OPCODE_BYTEPREFIX:
		return CreateAMLObjRef(&(*((UINT8*)nnxmalloc(sizeof(UINT8))) = GetByte()), tAMLByte);
	case AML_OPCODE_WORDPREFIX:
		return CreateAMLObjRef(&(*((UINT16*)nnxmalloc(sizeof(UINT16))) = GetWord()), tAMLWord);
	case AML_OPCODE_DWORDPREFIX:
		return CreateAMLObjRef(&(*((UINT32*)nnxmalloc(sizeof(UINT32))) = GetDword()), tAMLDword);
	case AML_OPCODE_STRINGPREFIX: {
		UINT8* str = (UINT8*)nnxcalloc(256, 1);
		GetString(str, 255, 0);
		return CreateAMLObjRef(str, tAMLString); 
	}
	case AML_OPCODE_QWORDPREFIX:
		return CreateAMLObjRef(&(*((UINT64*)nnxmalloc(sizeof(UINT64))) = GetQword()), tAMLQword);
	case AML_OPCODE_BUFFEROPCODE:
		return CreateAMLObjRef(CreateBuffer(), tAMLBuffer);
	case AML_OPCODE_PACKAGEOPCODE:
		return CreateAMLObjRef(CreatePackage(), tAMLPackage);
	default:
		return CreateAMLObjRef(NULL, tAMLInvalid);
	}
}

AMLBuffer::AMLBuffer(UINT32 size) {
	this->size = size;
	this->data = (UINT8*)nnxcalloc(size, 1);
}

AMLBuffer::~AMLBuffer() {
	nnxfree(this->data);
}

AMLScope* ACPI_AML_CODE::GetRootScope() {
	return &(this->root);
}

UINT32 ACPI_AML_CODE::GetSize() {
	return this->size;
}

void InitializeNamespace(AMLScope* root) {
	PrintT("Adding predefined scopes.\n");
	root->children.add(AMLScope::newScope("_SB_", root));
	root->children.add(AMLScope::newScope("_TZ_", root));
	root->children.add(AMLScope::newScope("_SI_", root));
	root->children.add(AMLScope::newScope("_PR_", root));
	root->children.add(AMLScope::newScope("_GPE", root));
	PrintT("Adding predefined scopes ended.\n");
}

extern "C" AML_Name CreateName(const char* name) {
	AML_Name n;
	memcpy(n.name, (void*)name, 4);
	return n;
}

AMLScope* AMLScope::newScope(const char* name, AMLScope* parent) {
	AMLScope* result = (AMLScope*)nnxmalloc(sizeof(AMLScope));
	*result = AMLScope();
	result->parent = parent;
	result->name = CreateName(name);
	return result;
}


AMLNamedObject* AMLDevice::Get_HID() {
	return this->_HID;
}

void AMLDevice::Init_HID() {
	NNXLinkedListEntry<AMLNamedObject*>* current = this->namedObjects.first;
	while (current) {
		if (current->value->name == g_HID) {
			this->_HID = current->value;
		}
		current = current->next;
	}
}

AMLDevice* AMLDevice::newScope(const char* name, AMLScope* parent) {
	AMLDevice* result = (AMLDevice*)nnxmalloc(sizeof(AMLDevice));
	*result = AMLDevice();
	result->parent = parent;
	result->name = CreateName(name);
	return result;
}