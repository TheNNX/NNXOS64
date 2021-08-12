/*
	I'LL FINISH THE PARSER AFTER
	DOING SOME MORE INTERESTING 
	STUFF. IT WORKS IN THE QEMU 
	VERSION I'M USING SO IT'S 
	   GOOD ENOUGH FOR NOW.
								  */

#define PRINT_IN_DEBUG_ONLY

#include "AML.h"
#include "memory/nnxalloc.h"
#include "memory/MemoryOperations.h"

#ifndef  NULL
#define NULL 0
#endif // ! NULL

AMLName gHID;

extern "C" {
	extern UINT8 localLastError;
	UINT8 ACPIParseDSDT(ACPI_DSDT* table) {
		if (!ACPIVerifySDT((ACPI_SDT*)table))
		{
			localLastError = ACPI_ERROR_INVALID_DSDT;
			return localLastError;
		}
		AMLParser acpi = AMLParser(table);
		acpi.Parse(acpi.GetRootScope(), acpi.GetSize());
	}
}

AMLParser::AMLParser(ACPI_DSDT* table) {
	this->table = (UINT8*)table;
	this->index = 0;
	GetString(this->name, 4);
	this->size = GetDWORD();
	this->revision = GetBYTE();
	this->checksum = GetBYTE();
	GetString(this->oemid, 6, 0);
	GetString(this->tableid, 8, 0);
	this->oemRevision = GetDWORD();
	GetString(compilerName, 4);
	this->compilerRevision = GetDWORD();
	this->root = AMLScope();
	this->size -= index;
	gHID = CreateName("_HID");
	InitializeNamespace(&(this->root));
}

UINT8 AMLParser::GetBYTE() {
	index++;
	return table[index-1];
}

UINT16 AMLParser::GetWORD() {
	index += 2;
	return *((UINT16*)(table+index-2));
}

UINT32 AMLParser::GetDWORD() {
	index += 4;
	return *((UINT32*)(table+index-4));
}

UINT64 AMLParser::GetQWORD() {
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

AMLName AMLParser::GetName() {
	UINT32 temp = GetDWORD();
	return *((AMLName*)(&temp));
}

void AMLParser::Parse(AMLScope* scope, UINT32 size) {
	PrintT("Parsing for scope: %x ", scope); PrintName(scope->name); PrintT("\n");
	UINT64 indexMax = index + size;
	while (index < indexMax) {
		UINT8 opcode = GetBYTE();
		switch (opcode)
		{
		case AML_OPCODE_ZEROOPCODE:
		case AML_OPCODE_ONESOPCODE:
			break;
		case AML_OPCODE_NAMEOPCODE:
		{
			AMLNameWithinAScope nws = this->GetNameWithinAScope(scope);
			AMLNamedObject *namedObject = AMLNamedObject::newObject(nws.name, CreateAMLObjRef(NULL, tAMLInvalid));
			AMLObjRef amlObject = this->GetAMLObject(GetBYTE());
			namedObject->object = amlObject;
			nws.scope->namedObjects.Add(namedObject);
			break;
		}
		case AML_OPCDOE_SCOPEOPCODE:
		{
			UINT64 index1 = index;
			UINT32 PkgLenght = this->DecodePkgLenght();

			AMLNameWithinAScope nws = this->GetNameWithinAScope(scope);
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
			AMLNameWithinAScope name = GetNameWithinAScope(scope);
			AMLMethodDef* methodDef = CreateMethod(name.name, name.scope);
			methodDef->name.scope->methods.Add(methodDef);
			UINT8 flags = GetBYTE();
			UINT64 index2 = this->index;
			methodDef->codeIndex = index2;
			methodDef->maxCodeIndex = methodDef->codeIndex + PkgLenght - (index2 - index1);
			methodDef->parameterNumber = flags & 0x7;

			//we want to skip the code of the function since this class is only a parser, there will be a separate 'AMLExecutor' class that will deal with the code execution
			this->index = methodDef->maxCodeIndex;

			break;
		}
		case AML_OPCODE_EXTOPPREFIX:
		{
			UINT8 extOpcode = GetBYTE();
			switch (extOpcode)
			{
			case AML_OPCODE_EXTOP_DEVICEOPCODE:
			{
				UINT64 index1 = this->index;
				UINT32 PkgLenght = DecodePkgLenght();
				AMLNameWithinAScope nws = GetNameWithinAScope(scope);
				UINT64 index2 = this->index;
				UINT32 size = PkgLenght - (index2 - index1);
				PrintT("Device: ");
				PrintName(nws.name);
				PrintT("\n");

				AMLDevice *device = AMLDevice::newScope((const char*)nws.name.name, scope);
				this->Parse(device, size);
				device->Init_HID();
				PrintT("_HID: %x\n", GetIntegerFromAMLObjRef(device->Get_HID()->object));
				break;
			}case AML_OPCODE_EXTOP_OPREGIONOPCODE: {
				AMLNameWithinAScope name = GetNameWithinAScope(scope);
				AMLOpetationRegion* prevOpRegion = scope->opRegion;
				scope->opRegion = CreateOperationRegion(name);
				scope->opRegion->type = GetBYTE();
				scope->opRegion->offset = GetIntegerFromAMLObjRef(GetAMLObject(GetBYTE()));
				scope->opRegion->lenght = GetInteger();
				scope->opRegion->previous = prevOpRegion;
				PrintT("Done\n");
				break;
			}
			case AML_OPCODE_EXTOP_FIELDOPCODE: {
				PrintT("Declaring field\n");

				UINT32 startIndex = this->index;
				UINT32 pkgLenght = DecodePkgLenght();

				AMLNameWithinAScope fieldName = GetNameWithinAScope(scope);
				UINT8 fieldFlags = GetBYTE();

				AMLField* field = this->CreateField(scope->opRegion, fieldFlags & 0xf, (fieldFlags & 0x10) >> 4, (fieldFlags & 0xE0) >> 5);

				INT64 pkgLenghtCounter = pkgLenght - (this->index - startIndex);

				while (pkgLenghtCounter > 0) {
					startIndex = this->index;

					AMLNameWithinAScope name = GetNameWithinAScope(scope);
					AMLFieldUnit* fieldUnit = CreateFieldUnit(name.name, GetBYTE());
					name.scope->namedObjects.Add(AMLNamedObject::newObject(name.name, CreateAMLObjRef(fieldUnit, tAMLFieldUnit)));
					field->fieldUnits.Add(fieldUnit);
					PrintName(fieldUnit->name);

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
	AMLObjRef objRef = GetAMLObject(GetBYTE());
	UINT64 res = GetIntegerFromAMLObjRef(objRef);
	return res;
}

AMLNameWithinAScope AMLParser::GetNameWithinAScope(AMLScope* current) {
	AMLScope* dstScope = current;
	if (this->table[index] == AML_OPCODE_DUALNAMEPREFIX || this->table[index] == AML_OPCODE_MULTINAMEPREFIX) {
		UINT8 SegNum = this->table[index] == AML_OPCODE_DUALNAMEPREFIX ? 1 : GetBYTE() - 1; //substract one, since the last name is going to be read
																							//by the code outside the else-if
		for (UINT8 a = 0; a < SegNum; a++) {

			AMLName name = GetName();
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

AMLScope* AMLParser::FindScope(AMLName name, AMLScope* current) {
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

AMLNamedObject::AMLNamedObject(AMLName name, AMLObjRef object) {
	this->name = name;
	this->object = object;
}

AMLNamedObject* AMLNamedObject::newObject(AMLName name, AMLObjRef object) {
	AMLNamedObject* output = (AMLNamedObject*)NNXAllocatorAlloc(sizeof(AMLNamedObject));
	*output = AMLNamedObject(name, object);
	//PrintT("Allocated new named object for name ");PrintName(output->name);PrintT(" at location %x\n", output);
	return output;
}

void PrintName(AMLName t) {
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
	UINT8 Byte0 = GetBYTE();
	UINT8 PkgLengthType = (Byte0 & 0xc0) >> 6;
	if (PkgLengthType) {
		UINT32 result = Byte0 & 0xf;
		for (int a = 0; a < PkgLengthType; a++) {
			result |= (((UINT32)GetBYTE()) << (4 + 8 * a));
		}
		return result;
	}
	else {
		return Byte0 & 0x3f;
	}
}

UINT8 AMLParser::DecodePackageNumElements() {
	UINT8 Byte0 = this->GetBYTE();
	
	if (Byte0 == AML_OPCODE_BYTEPREFIX) {
		Byte0 = this->GetBYTE();
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
		return (UINT64)objRef.pointer;
	default:
		return 0;
	} 
}


UINT32 AMLParser::DecodeBufferSize() {
	AMLObjRef objRef = GetAMLObject(GetBYTE());
	return GetIntegerFromAMLObjRef(objRef);
}

AMLBuffer* AMLParser::ReadBufferData() {
	UINT32 bufferSize = DecodeBufferSize();
	AMLBuffer* amlBuffer = (AMLBuffer*)NNXAllocatorAlloc(sizeof(AMLBuffer));
	*amlBuffer = AMLBuffer(bufferSize);
	for (int a = 0; a < bufferSize; a++) {
		amlBuffer->data[a] = GetBYTE();
	}
	return amlBuffer;
}

int __strlen(char* string) {
	int a = 0;
	while (string[a]) a++;
	return a;
}

AMLBuffer* AMLParser::AMLToBuffer(AMLObjRef data) {
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
	switch(UINT8 opcode = GetBYTE()) {
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
		return AMLToBuffer(GetAMLObject(GetBYTE()));
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
		AMLObjRef element = GetAMLObject(GetBYTE());
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

AMLObjRef AMLParser::GetAMLObject(UINT8 opcode) {
	switch (opcode)
	{
	case AML_OPCODE_ZEROOPCODE:
	case AML_OPCODE_ONEOPCODE:
		return CreateAMLObjRef(&(*((UINT8*)NNXAllocatorAlloc(sizeof(UINT8))) = opcode), this->revision < 2 ? tAMLDword : tAMLQword);
	case AML_OPCODE_BYTEPREFIX:
		return CreateAMLObjRef(&(*((UINT8*)NNXAllocatorAlloc(sizeof(UINT8))) = GetBYTE()), tAMLByte);
	case AML_OPCODE_WORDPREFIX:
		return CreateAMLObjRef(&(*((UINT16*)NNXAllocatorAlloc(sizeof(UINT16))) = GetWORD()), tAMLWord);
	case AML_OPCODE_DWORDPREFIX:
		return CreateAMLObjRef(&(*((UINT32*)NNXAllocatorAlloc(sizeof(UINT32))) = GetDWORD()), tAMLDword);
	case AML_OPCODE_STRINGPREFIX: {
		UINT8* str = (UINT8*)NNXAllocatorAllocArray(256, 1);
		GetString(str, 255, 0);
		return CreateAMLObjRef(str, tAMLString); 
	}
	case AML_OPCODE_QWORDPREFIX:
		return CreateAMLObjRef(&(*((UINT64*)NNXAllocatorAlloc(sizeof(UINT64))) = GetQWORD()), tAMLQword);
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

extern "C" AMLName CreateName(const char* name) {
	AMLName n;
	MemCopy(n.name, (void*)name, 4);
	return n;
}

AMLScope* AMLScope::newScope(const char* name, AMLScope* parent) {
	AMLScope* result = (AMLScope*)NNXAllocatorAlloc(sizeof(AMLScope));
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
	result->name = CreateName(name);
	result->opRegion = 0;
	return result;
}

AMLMethodDef::AMLMethodDef(AMLName name, AMLParser* parser, AMLScope* scope) {
	this->parser = parser;
	this->name.scope = scope;
	this->name.name = name;
	this->parameterNumber = 0;
}

AMLMethodDef* AMLParser::CreateMethod(AMLName methodName, AMLScope* scope) {
	AMLMethodDef* method = (AMLMethodDef*)NNXAllocatorAlloc(sizeof(AMLMethodDef));
	*method = AMLMethodDef(methodName, this, scope);
	method->codeIndex = 0;
	method->maxCodeIndex = 0;
	return method;
}

AMLOpetationRegion* AMLParser::CreateOperationRegion(AMLNameWithinAScope name) {
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

AMLFieldUnit* AMLParser::CreateFieldUnit(AMLName name, UINT8 width) {
	AMLFieldUnit* fieldUnit = (AMLFieldUnit*)NNXAllocatorAlloc(sizeof(AMLFieldUnit));
	*fieldUnit = AMLFieldUnit();
	fieldUnit->width = width;
	fieldUnit->name = name;
	return fieldUnit;
}