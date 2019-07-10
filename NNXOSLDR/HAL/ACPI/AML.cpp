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
		acpi.Parse();
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
	this->namedObjects = NNXLinkedList<AMLNamedObject*>();
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

void ACPI_AML_CODE::Parse() {
	while (1) {
		UINT8 opcode = GetByte();
		switch (opcode)
		{
		case AML_OPCODE_ZEROOPCODE:
		case AML_OPCODE_ONESOPCODE:
			break;
		case AML_OPCODE_NAMEOPCODE: {
			AMLNamedObject *namedObject = AMLNamedObject::newObject(GetName(), NULL);
			void* amlObject = this->GetAMLObject(GetByte()).pointer;
			namedObject->object = amlObject;
			namedObjects.add(namedObject);
			break;
		}
		default:
			PrintT("Error: unimplmeneted ACPI Machine Language opcode: 0x%x\n",opcode);
			while (1);
		}
	}
}

AMLNamedObject::AMLNamedObject(AML_Name name, void* object) {
	this->name = name;
	this->object = object;
}

AMLNamedObject* AMLNamedObject::newObject(AML_Name name, void* object) {
	AMLNamedObject* output = (AMLNamedObject*)nnxmalloc(sizeof(AMLNamedObject));
	AMLNamedObject toCopy = AMLNamedObject(name, object);
	*output = toCopy;
	PrintT("Allocated new named object for name ");
	output->PrintName();
	PrintT("\n");
	return output;
}

void AMLNamedObject::PrintName() {
	char buffer[5] = { 0 };
	for (int a = 0; a < 4; a++) {
		buffer[a] = this->name.name[a];
	}
	PrintT("%s", buffer);
}

AMLNamespace::AMLNamespace() {
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

UINT32 ACPI_AML_CODE::DecodePackageNumElements() {
	UINT8 Byte0 = this->GetByte();
	
	if (Byte0 == AML_OPCODE_WORDPREFIX &&
		this->table[this->index + 2] != AML_OPCODE_ZEROOPCODE &&
		this->table[this->index + 2] != AML_OPCODE_ONEOPCODE &&
		this->table[this->index + 2] != AML_OPCODE_ONESOPCODE &&
		!(this->table[this->index + 2] > AML_OPCODE_BYTEPREFIX && this->table[this->index + 2] < AML_OPCODE_QWORDPREFIX)) 
	{
		return GetWord();
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

AMLPackage::AMLPackage(UINT16 numElements) {
	this->numElements = numElements;
}

AMLPackage* ACPI_AML_CODE::CreatePackage() {
	UINT32 pkgLenght = DecodePkgLenght();
	UINT64 numElements = DecodePackageNumElements();
	AMLPackage* amlPackage = (AMLPackage*)nnxmalloc(sizeof(AMLPackage));
	*amlPackage = AMLPackage(numElements);



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

AMLBuffer::AMLBuffer(UINT8 size) {
	this->size = size;
	this->data = (UINT8*)nnxcalloc(size, 1);
}

AMLBuffer::~AMLBuffer() {
	nnxfree(this->data);
}