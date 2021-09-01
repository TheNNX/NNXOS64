/*
	I'LL FINISH THE PARSER AFTER
	DOING SOME MORE INTERESTING 
	STUFF. IT WORKS IN THE QEMU 
	VERSION I'M USING SO IT'S 
	   GOOD ENOUGH FOR NOW.
								  */
/*
	THIS IS VERY BROKEN
*/

#define PRINT_IN_DEBUG_ONLY

#include "AML.h"
#include "memory/nnxalloc.h"
#include "memory/MemoryOperations.h"

#ifndef  NULL
#define NULL 0
#endif // ! NULL

AML_NAME gHID;

extern "C" {
	extern UINT8 localLastError;
	UINT8 AcpiParseDSDT(ACPI_DSDT* table) {
		if (!AcpiVerifySdt((ACPI_SDT*)table))
		{
			localLastError = ACPI_ERROR_INVALID_DSDT;
			return localLastError;
		}

		AMLParser acpi = AMLParser(table);
		acpi.Parse(acpi.GetRootScope(), acpi.GetSize());
		return 0;
	}
}

AMLParser::AMLParser(ACPI_DSDT* table) {
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
	gHID = AcpiCreateName("_HID");
	InitializeNamespace(&(this->root));
}

UINT8 AMLParser::GetByte() {
	index++;
	return table[index-1];
}

UINT16 AMLParser::GetWord() {
	index += 2;
	return *((UINT16*)(table+index-2));
}

UINT32 AMLParser::GetDword() {
	index += 4;
	return *((UINT32*)(table+index-4));
}

UINT64 AMLParser::GetQword() {
	index += 8;
	return *((UINT64*)(table + index - 8));
}

void AMLParser::GetString(UINT8* output, UINT32 lenght) {
	for (UINT32 a = 0; a < lenght; a++) {
		output[a] = table[index];
		index++;
	}
}

void AMLParser::GetString(UINT8* output, UINT32 lenght, UINT8 terminator) {
	for (UINT32 a = 0; a < lenght; a++) {
		if (terminator == table[index])
			break;
		output[a] = table[index];
		index++;
	}
}

AML_NAME AMLParser::GetName() {
	UINT32 temp = GetDword();
	return *((AML_NAME*)(&temp));
}

void AMLParser::Parse(AMLScope* scope, UINT32 size) {
	PrintT("Parsing for scope: %x ", scope); PrintName(scope->name); PrintT("\n");
	UINT64 indexMax = index + size;
	while (index < indexMax) {
		UINT8 opcode = GetByte();
		PrintT("[ACPI opcode]: %x\n", opcode);
		switch (opcode)
		{
		case AML_OPCODE_ZEROOPCODE:
		case AML_OPCODE_ONESOPCODE:
			break;
		case AML_OPCODE_NAMEOPCODE:
		{
			AML_NAME_WITHIN_SCOPE nws = this->GetNameWithinAScope(scope);
			AMLNamedObject *namedObject = AMLNamedObject::newObject(nws.name, CreateAMLObjRef(NULL, tAMLInvalid));
			AMLObjRef amlObject = this->GetAmlObject(GetByte());
			namedObject->object = amlObject;
			nws.scope->namedObjects.Add(namedObject);
			break;
		}
		case AML_OPCDOE_SCOPEOPCODE:
		{
			UINT64 index1 = index;
			UINT32 PkgLenght = this->DecodePkgLenght();

			AML_NAME_WITHIN_SCOPE nws = this->GetNameWithinAScope(scope);
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
		case AML_OPCODE_METHODOPCODE:
		{
			UINT64 index1 = this->index;
			UINT32 PkgLenght = DecodePkgLenght();
			AML_NAME_WITHIN_SCOPE name = GetNameWithinAScope(scope);
			AMLMethodDef* methodDef = CreateMethod(name.name, name.scope);
			methodDef->name.scope->methods.Add(methodDef);
			UINT8 flags = GetByte();
			UINT64 index2 = this->index;
			methodDef->codeIndex = index2;
			methodDef->maxCodeIndex = methodDef->codeIndex + PkgLenght - (index2 - index1);
			methodDef->parameterNumber = flags & 0x7;

			/* 
				We want to skip the code of the function since this class is only a parser.
				There will be a separate 'AMLExecutor' class that will deal with the code execution.
			*/
			this->index = methodDef->maxCodeIndex;

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
				AML_NAME_WITHIN_SCOPE nws = GetNameWithinAScope(scope);
				UINT64 index2 = this->index;
				UINT32 size = PkgLenght - (index2 - index1);
				PrintT("Device: ");
				PrintName(nws.name);
				PrintT("\n");

				AMLDevice *device = AMLDevice::newScope((const char*)nws.name.name, scope);
				this->Parse(device, size);
				device->InitHid();
				PrintT("_HID: %x\n", GetIntegerFromAmlObjRef(device->GetHid()->object));
				break;
			}case AML_OPCODE_EXTOP_OPREGIONOPCODE: {

				AML_NAME_WITHIN_SCOPE name = GetNameWithinAScope(scope);
				AMLOpetationRegion* prevOpRegion = scope->opRegion;
				scope->opRegion = CreateOperationRegion(name);
				scope->opRegion->type = GetByte();
				scope->opRegion->offset = GetIntegerFromAmlObjRef(GetAmlObject(GetByte()));
				scope->opRegion->lenght = GetInteger();
				scope->opRegion->previous = prevOpRegion;
				break;
			}
			case AML_OPCODE_EXTOP_FIELDOPCODE: {
				PrintT("Declaring field ");

				UINT32 startIndex = this->index;
				UINT32 pkgLenght = DecodePkgLenght();

				AML_NAME_WITHIN_SCOPE fieldName = GetNameWithinAScope(scope);
				UINT8 fieldFlags = GetByte();

				AMLField* field = this->CreateField(scope->opRegion, fieldFlags & 0xf, (fieldFlags & 0x10) >> 4, (fieldFlags & 0xE0) >> 5);

				INT64 pkgLenghtCounter = pkgLenght - (this->index - startIndex);

				while (pkgLenghtCounter > 0) {
					startIndex = this->index;

					AML_NAME_WITHIN_SCOPE name = GetNameWithinAScope(scope);
					AMLFieldUnit* fieldUnit = CreateFieldUnit(name.name, GetByte());
					name.scope->namedObjects.Add(AMLNamedObject::newObject(name.name, CreateAMLObjRef(fieldUnit, tAMLFieldUnit)));
					field->fieldUnits.Add(fieldUnit);
					PrintName(fieldUnit->name);
					PrintT("\n");

					pkgLenghtCounter -= (this->index - startIndex);
				}

				scope->opRegion->fields.Add(field);
				break;
			}

			default:
				PrintT("Error: unimplemented ACPI Machine Language opcode 0x5B 0x%X", extOpcode);
				while (1);
			}
			break;
		}
		default:
			PrintT("Error: unimplmeneted ACPI Machine Language opcode: 0x%X\n", opcode);
			while (1);
		}
	}
	if (scope != &root) {
		PrintT("Parsing for scope "); PrintName(scope->name); PrintT(" ended.\n\n");
	}
	else {
		PrintT("Parsing for scope <root> ended.\n\n");
	}
}

UINT64 AMLParser::GetInteger() {
	AMLObjRef objRef = GetAmlObject(GetByte());
	UINT64 res = GetIntegerFromAmlObjRef(objRef);
	return res;
}

AML_NAME_WITHIN_SCOPE AMLParser::GetNameWithinAScope(AMLScope* current) {
	AMLScope* dstScope = current;
	if (this->table[index] == AML_OPCODE_DUALNAMEPREFIX || this->table[index] == AML_OPCODE_MULTINAMEPREFIX) {
		UINT8 SegNum = this->table[index] == AML_OPCODE_DUALNAMEPREFIX ? 1 : GetByte() - 1; //substract one, since the last Name is going to be read
																							//by the code outside the else-if
		for (UINT8 a = 0; a < SegNum; a++) {

			AML_NAME name = GetName();
			dstScope = this->FindScope(name, dstScope);
			if (dstScope == 0) {
				localLastError == ACPI_ERROR_AML_OBJECT_NOT_FOUND;
				AML_NAME_WITHIN_SCOPE null = { 0 };
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
	AML_NAME_WITHIN_SCOPE name;
	name.name = GetName();
	name.scope = dstScope;
	return name;
}

AMLScope* AMLParser::FindScope(AML_NAME name, AMLScope* current) {
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

AMLNamedObject::AMLNamedObject(AML_NAME name, AMLObjRef object) {
	this->name = name;
	this->object = object;
}

AMLNamedObject* AMLNamedObject::newObject(AML_NAME name, AMLObjRef object) {
	AMLNamedObject* output = (AMLNamedObject*)NNXAllocatorAlloc(sizeof(AMLNamedObject));
	*output = AMLNamedObject(name, object);
	//PrintT("Allocated new named object for Name ");PrintName(output->Name);PrintT(" at location %x\n", output);
	return output;
}

void PrintName(AML_NAME t) {
	char buffer[5] = { 0 };
	for (int a = 0; a < 4; a++) {
		buffer[a] = t.name[a];
	}
	PrintT("%s", buffer);
}

AMLScope::AMLScope() {
	this->namedObjects = NNXLinkedList<AMLNamedObject*>();
	this->children = NNXLinkedList<AMLScope*>();
	this->methods = NNXLinkedList<AMLMethodDef*>();
	this->parent = 0;
}

UINT32 AMLParser::DecodePkgLenght() {
	UINT8 Byte0 = GetByte();
	UINT8 PkgLengthType = (Byte0 & 0xc0) >> 6;
	if (PkgLengthType) {
		UINT32 result = Byte0 & 0xf;
		for (int a = 0; a < PkgLengthType; a++) {
			result |= (((UINT32)GetByte()) << (4 + 8 * a));
		}
		return result;
	}
	else {
		return Byte0 & 0x3f;
	}
}

UINT8 AMLParser::DecodePackageNumElements() {
	UINT8 Byte0 = this->GetByte();
	
	if (Byte0 == AML_OPCODE_BYTEPREFIX) {
		Byte0 = this->GetByte();
	}
	else {
		return Byte0;
	}
	
}

UINT64 GetIntegerFromAmlObjRef(AMLObjectReference objRef) {
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
		return (UINT64)objRef.pointer;
	default:
		return 0;
	} 
}


UINT32 AMLParser::DecodeBufferSize() {
	AMLObjRef objRef = GetAmlObject(GetByte());
	return GetIntegerFromAmlObjRef(objRef);
}

AMLBuffer* AMLParser::ReadBufferData() {
	UINT32 bufferSize = DecodeBufferSize();
	AMLBuffer* amlBuffer = (AMLBuffer*)NNXAllocatorAlloc(sizeof(AMLBuffer));
	*amlBuffer = AMLBuffer(bufferSize);
	for (int a = 0; a < bufferSize; a++) {
		amlBuffer->data[a] = GetByte();
	}
	return amlBuffer;
}

int __strlen(char* string) {
	int a = 0;
	while (string[a]) a++;
	return a;
}

AMLBuffer* AMLParser::AmlToBuffer(AMLObjRef data) {
	AMLBuffer* buffer = (AMLBuffer*)NNXAllocatorAlloc(sizeof(AMLBuffer));
	switch (data.type) {
	case tAMLString:
		*buffer = AMLBuffer((__strlen((char*)data.pointer))+1);
		for (int a = 0; a < buffer->size; a++)
			buffer->data[a] = ((char*)data.pointer)[a];
		break;
	case tAMLDword:
		*buffer = AMLBuffer(4);
		for (int a = 0; a < 4; a++) 
			buffer->data[a] = ((UINT8*)data.pointer)[a];
		break;
	case tAMLQword:
		*buffer = AMLBuffer(8);
		for (int a = 0; a < 8; a++)
			buffer->data[a] = ((UINT8*)data.pointer)[a];
		break;
	case tAMLBuffer:
		*buffer = AMLBuffer(((AMLBuffer*)data.pointer)->size);
		for (int a = 0; a < buffer->size; a++) {
			buffer->data[a] = ((AMLBuffer*)data.pointer)->data[a];
		}
		break;
	default:
		return 0;
	}
	return buffer;
}

AMLBuffer* AMLParser::CreateBufferFromBufferData() {
	switch(UINT8 opcode = GetByte()) {
	case AML_OPCODE_TOBUFFEROPCODE:	//TODO: rest of type 5 opcodes
		return this->CreateBufferFromType5Opcode(opcode);
	default:
		this->index--;
		return ReadBufferData();
	}
}

AMLBuffer* AMLParser::CreateBufferFromType5Opcode(UINT8 opcode) {
	switch (opcode) {
	case AML_OPCODE_TOBUFFEROPCODE:
		return AmlToBuffer(GetAmlObject(GetByte()));
	}
	return 0;
}


AMLBuffer* AMLParser::CreateBuffer() {
	UINT32 pkgLength = DecodePkgLenght();
	return ReadBufferData();
}

AMLPackage::AMLPackage(UINT8 numElements) {
	this->numElements = numElements;
	this->elements = (AMLObjRef*)NNXAllocatorAllocArray(numElements, sizeof(AMLObjRef));
}

AMLPackage* AMLParser::CreatePackage() {
	UINT32 pkgLenght = DecodePkgLenght();
	UINT8 numElements = DecodePackageNumElements();
	AMLPackage* amlPackage = (AMLPackage*)NNXAllocatorAlloc(sizeof(AMLPackage));
	*amlPackage = AMLPackage(numElements);

	amlPackage->elements = (AMLObjRef*)(NNXAllocatorAllocArray(numElements, sizeof(AMLObjRef)));
	for (int a = 0; a < numElements; a++) {
		AMLObjRef element = GetAmlObject(GetByte());
		amlPackage->elements[a] = element;
	}

	return amlPackage;
}

AMLObjRef CreateAMLObjRef(VOID* pointer, AMLObjectType type) {
	AMLObjRef result = { 0 };
	result.pointer = pointer;
	result.type = type;
	return result;
}

AMLObjRef AMLParser::GetAmlObject(UINT8 opcode) {
	switch (opcode)
	{
	case AML_OPCODE_ZEROOPCODE:
	case AML_OPCODE_ONEOPCODE:
		return CreateAMLObjRef(&(*((UINT8*)NNXAllocatorAlloc(sizeof(UINT8))) = opcode), this->revision < 2 ? tAMLDword : tAMLQword);
	case AML_OPCODE_BYTEPREFIX:
		return CreateAMLObjRef(&(*((UINT8*)NNXAllocatorAlloc(sizeof(UINT8))) = GetByte()), tAMLByte);
	case AML_OPCODE_WORDPREFIX:
		return CreateAMLObjRef(&(*((UINT16*)NNXAllocatorAlloc(sizeof(UINT16))) = GetWord()), tAMLWord);
	case AML_OPCODE_DWORDPREFIX:
		return CreateAMLObjRef(&(*((UINT32*)NNXAllocatorAlloc(sizeof(UINT32))) = GetDword()), tAMLDword);
	case AML_OPCODE_STRINGPREFIX: {
		UINT8* str = (UINT8*)NNXAllocatorAllocArray(256, 1);
		GetString(str, 255, 0);
		return CreateAMLObjRef(str, tAMLString); 
	}
	case AML_OPCODE_QWORDPREFIX:
		return CreateAMLObjRef(&(*((UINT64*)NNXAllocatorAlloc(sizeof(UINT64))) = GetQword()), tAMLQword);
	case AML_OPCODE_BUFFEROPCODE:
		return CreateAMLObjRef(CreateBuffer(), tAMLBuffer);
	case AML_OPCODE_PACKAGEOPCODE:
		return CreateAMLObjRef(CreatePackage(), tAMLPackage);
	case AML_OPCODE_ONESOPCODE:
		return CreateAMLObjRef(&(*((UINT8*)NNXAllocatorAlloc(sizeof(UINT8))) = this->revision < 2 ? 0xFFFFFFFF : 0xFFFFFFFFFFFFFFFF), this->revision < 2 ? tAMLDword : tAMLQword);
	default:
		return CreateAMLObjRef(NULL, tAMLInvalid);
	}
}

AMLBuffer::AMLBuffer(UINT32 size) {
	this->size = size;
	this->data = (UINT8*)NNXAllocatorAllocArray(size, 1);
}

AMLScope* AMLParser::GetRootScope() {
	return &(this->root);
}

UINT32 AMLParser::GetSize() {
	return this->size;
}

void InitializeNamespace(AMLScope* root) {
	PrintT("Adding predefined scopes.\n");
	root->children.Add(AMLScope::newScope("_SB_", root));
	root->children.Add(AMLScope::newScope("_TZ_", root));
	root->children.Add(AMLScope::newScope("_SI_", root));
	root->children.Add(AMLScope::newScope("_PR_", root));
	root->children.Add(AMLScope::newScope("_GPE", root));
}

extern "C" AML_NAME AcpiCreateName(const char* name) {
	AML_NAME n;
	MemCopy(n.name, (void*)name, 4);
	return n;
}

AMLScope* AMLScope::newScope(const char* name, AMLScope* parent) {
	AMLScope* result = (AMLScope*)NNXAllocatorAlloc(sizeof(AMLScope));
	*result = AMLScope();
	result->parent = parent;
	result->name = AcpiCreateName(name);
	return result;
}


AMLNamedObject* AMLDevice::GetHid() {
	return this->_HID;
}

void AMLDevice::InitHid() {
	NNXLinkedListEntry<AMLNamedObject*>* current = this->namedObjects.first;
	while (current) {
		if (current->value->name == gHID) {
			this->_HID = current->value;
		}
		current = current->next;
	}
}

AMLDevice* AMLDevice::newScope(const char* name, AMLScope* parent) {
	AMLDevice* result = (AMLDevice*)NNXAllocatorAlloc(sizeof(AMLDevice));
	*result = AMLDevice();
	result->parent = parent;
	result->name = AcpiCreateName(name);
	result->opRegion = 0;
	return result;
}

AMLMethodDef::AMLMethodDef(AML_NAME name, AMLParser* parser, AMLScope* scope) {
	this->parser = parser;
	this->name.scope = scope;
	this->name.name = name;
	this->parameterNumber = 0;
}

AMLMethodDef* AMLParser::CreateMethod(AML_NAME methodName, AMLScope* scope) {
	AMLMethodDef* method = (AMLMethodDef*)NNXAllocatorAlloc(sizeof(AMLMethodDef));
	*method = AMLMethodDef(methodName, this, scope);
	method->codeIndex = 0;
	method->maxCodeIndex = 0;
	return method;
}

AMLOpetationRegion* AMLParser::CreateOperationRegion(AML_NAME_WITHIN_SCOPE name) {
	AMLOpetationRegion* opregion = (AMLOpetationRegion*)NNXAllocatorAlloc(sizeof(AMLOpetationRegion));
	*opregion = AMLOpetationRegion();
	opregion->name = name;
	return opregion;
}

AMLField* AMLParser::CreateField(AMLOpetationRegion *parent, UINT8 access, UINT8 lock, UINT8 updateRule) {
	AMLField* field = (AMLField*)NNXAllocatorAlloc(sizeof(AMLField));
	*field = AMLField();
	field->lock = lock;
	field->accessWidth = access;
	field->parent = parent;
	field->updateRule = updateRule;
	return field;
}

AMLFieldUnit* AMLParser::CreateFieldUnit(AML_NAME name, UINT8 width) {
	AMLFieldUnit* fieldUnit = (AMLFieldUnit*)NNXAllocatorAlloc(sizeof(AMLFieldUnit));
	*fieldUnit = AMLFieldUnit();
	fieldUnit->width = width;
	fieldUnit->name = name;
	return fieldUnit;
}