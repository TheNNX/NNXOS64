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

#include "AML.h"
#include "memory/nnxalloc.h"
#include "memory/MemoryOperations.h"

#ifndef  NULL
#define NULL 0
#endif // ! NULL


extern "C" { 

	UINT8 localLastError = 0;

	UINT8 ACPI_LastError() {
		return localLastError;
	}

	UINT8 ACPI_ParseDSDT(ACPI_DSDT* table) {
		PrintT("ACPI_AML_CODE class initialization\n");
		if (!verifyACPI_DSDT(table))
		{
			localLastError = ACPI_ERROR_INVALID_DSDT;
			return localLastError;
		}
		ACPI_AML_CODE acpi = ACPI_AML_CODE(table);
		PrintT("ACPI_AML_CODE class initialized\n");
		acpi.Parse(acpi.GetRootScope(), acpi.GetSize());
	}

	ACPI_RSDT* GetRSDT(ACPI_RDSP* rdsp) {
		if (!verifyACPI_RDSP(rdsp)) {
			localLastError = ACPI_ERROR_INVALID_RDSP;
			return 0;
		}

		UINT64 result = 0;
		result = rdsp->RSDTAddress;
		return (ACPI_RSDT*)result;
	}

	ACPI_XSDT* GetXSDT(ACPI_RDSP* rdsp) {
		if (rdsp->Revision == 0)
		{
			localLastError = ACPI_ERROR_NOT_SUPPORTED_BY_ACPI_10;
			return 0;
		}
		if (!verifyACPI_RDSP(rdsp)) {
			localLastError = ACPI_ERROR_INVALID_RDSP;
			return 0;
		}

		return rdsp->v20.XSDTAddress;
	}



	ACPI_FADT* GetFADT(ACPI_RDSP* rdsp) {
		if (rdsp->Revision == 0) {
			ACPI_RSDT* rsdt = GetRSDT(rdsp);
			if (!rsdt)
			{
				localLastError = ACPI_ERROR_INVALID_RSDT;
				return 0;
			}
			if (!verifyACPI_RSDT(rsdt)) {
				localLastError = ACPI_ERROR_INVALID_RSDT;
				return 0;
			}
			UINT32 numberOfEntries = (rsdt->Header.Lenght - sizeof(rsdt->Header)) / 4;
			for (UINT32 a = 0; a < numberOfEntries; a++) {
				UINT32 tableAddress = rsdt->OtherSDTs[a];
				ACPI_SDTHeader* sdt = (ACPI_SDTHeader*)tableAddress;
				const UINT8 dsdt[] = {'F','A','C','P'};
				if (*((UINT32*)dsdt) == *((UINT32*)sdt->Signature))
				{
					PrintT("FACP: %i\n",a);
					return (ACPI_FADT*)tableAddress;
				}
				else {
					PrintT("%i: %s\n",a,sdt->Signature);
				}
			}
		}
		else {
			ACPI_XSDT* xsdt = GetXSDT(rdsp);
			if (!xsdt)
			{
				localLastError = ACPI_ERROR_INVALID_XSDT;
				return 0;
			}
			if(!verifyACPI_XSDT(xsdt)) {
				localLastError = ACPI_ERROR_INVALID_XSDT;
				return 0;
			}
			UINT32 numberOfEntries = (xsdt->Header.Lenght - sizeof(xsdt->Header)) / 8;
			for (UINT32 a = 0; a < numberOfEntries; a++) {
				ACPI_SDTHeader* tableAddress = xsdt->OtherSDTs[a];
				const UINT8 dsdt[] = { 'F','A','C','P' };
				if (*((UINT32*)dsdt) == *((UINT32*)tableAddress->Signature))
				{
					PrintT("FACP: %i\n", a);
					return (ACPI_FADT*)tableAddress;
				}
				else {
					PrintT("%i: %s\n", a, tableAddress->Signature);
				}
			}
		}
		localLastError = ACPI_ERROR_INVALID_FADT;
		return 0;
	}

	BOOL verifyACPI_FADT(ACPI_FADT* fadt) {
		UINT32 sum = 0;
		for (UINT32 index = 0; index < fadt->Header.Lenght; index++) {
			sum += ((char*)fadt)[index];
		}
		return (sum & 0xFF) == 0;
	}

	BOOL verifyACPI_DSDT(ACPI_DSDT* dsdt) {
		UINT32 sum = 0;
		for (UINT32 index = 0; index < dsdt->Header.Lenght; index++) {
			sum += ((char*)dsdt)[index];
		}
		return (sum & 0xFF) == 0;
	}

	BOOL verifyACPI_XSDT(ACPI_XSDT* xsdt) {
		UINT32 sum = 0;
		for (UINT32 index = 0; index < xsdt->Header.Lenght; index++) {
			sum += ((char*)xsdt)[index];
		}
		return (sum & 0xFF) == 0;
	}

	BOOL verifyACPI_RSDT(ACPI_RSDT* rsdt) {
		UINT32 Lenght = rsdt->Header.Lenght;
		UINT64 sum = 0;
		for (UINT32 a = 0; a < Lenght; a++) {
			sum += ((UINT8*)rsdt)[a];
		}
		return (sum & 0xFF) == 0;
	}

	BOOL verifyACPI_RDSP(ACPI_RDSP* rdsp) {
		UINT32 Lenght = sizeof(ACPI_RDSP) - sizeof(ACPI_RDSPExtension);
		if (rdsp->Revision > 0) {
			Lenght = rdsp->v20.Lenght;
		}
		PrintT("%x %i %x\n", rdsp, rdsp->Revision, Lenght);
		UINT32 sum = 0;
		for (UINT32 index = 0; index < Lenght; index++) {
			sum += ((char*)rdsp)[index];
		}
		return (sum & 0xFF) == 0;
	}
}
ACPI_AML_CODE::ACPI_AML_CODE(ACPI_DSDT* table) {
	this->table = (UINT8*)table;
	PrintT("So far so good\n");
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
	PrintT("name: %x\n",temp);
	return *((AML_Name*)(&temp));
}

void ACPI_AML_CODE::Parse(AMLScope* scope, UINT32 size) {
	UINT64 indexMax = index + size;
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
			AMLScope* scope = FindScope(nws.name, nws.scope);
			if (scope == 0) {
				PrintT("Couldn't find object: ");
				PrintName(nws.name);
				while (1);
			}
			UINT64 index2 = index;
			UINT32 size = PkgLenght - (index2 - index1);
			this->Parse(scope, size);
		}
		default:
			PrintT("Error: unimplmeneted ACPI Machine Language opcode: 0x%x\n",opcode);
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
	if (this->table[index] == '^') {
		dstScope = dstScope->parent;
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
	AMLNamedObject toCopy = AMLNamedObject(name, object);
	*output = toCopy;
	PrintT("Allocated new named object for name ");
	PrintName(output->name);
	PrintT("\n");
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
			result |= (GetByte() << (4 + 8 * a));
		}
		return result;
	}
	else {
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

AMLBuffer* ACPI_AML_CODE::CreateBuffer() {
	UINT32 pkgLength = DecodePkgLenght();
	UINT32 bufferSize = *((UINT32*)(GetAMLObject(GetByte()).pointer));
	
	AMLBuffer* amlBuffer = (AMLBuffer*)nnxmalloc(sizeof(AMLBuffer));
	*amlBuffer = AMLBuffer(bufferSize);
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
	root->children.add(AMLScope::newScope("_SB_"));
	root->children.add(AMLScope::newScope("_TZ_"));
	root->children.add(AMLScope::newScope("_SI_"));
	root->children.add(AMLScope::newScope("_PR_"));
	root->children.add(AMLScope::newScope("_GPE"));
	PrintT("Adding predefined scopes ended.\n");
}

AMLScope* AMLScope::newScope(const char* name) {
	AMLScope* result = (AMLScope*)nnxmalloc(sizeof(AMLScope));
	*result = AMLScope();
	memcpy(result->name.name, (void*)name, 4);
	return result;
}