#ifndef NNX_AML_HEADER
#define NNX_AML_HEADER

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

#include <SimpleTextIo.h>
#include <nnxtype.h>

#pragma pack(push, 1)

typedef UINT64 AMLObjectType, AMLObjType;

#ifdef __cplusplus 
extern "C" {
#endif

    typedef struct AMLObjectReference
    {
        void* pointer;
        AMLObjType type;
    }AMLObjRef, AMLObjectReference;

    typedef struct SDT_HEADER
    {
        UINT8 Signature[4];
        UINT32 Lenght;
        UINT8 Revision;
        UINT8 Checksum;
        UINT8 OEMID[6];
        UINT8 OEMTableID[8];
        UINT32 OEMRevision;
        UINT32 CreatorID;
        UINT32 CreatorRevision;
    }ACPI_SDT_HEADER, ACPI_SDT;

    typedef struct GAS
    {
        UINT8 AddressSpace;
        UINT8 BitWidth;
        UINT8 BitOffset;
        UINT8 AccessSize;
        UINT64 Address;
    }ACPI_GAS;

    typedef struct XSDT
    {
        ACPI_SDT_HEADER Header;
        ACPI_SDT_HEADER* OtherSDTs[0];
    }ACPI_XSDT;

    typedef struct RSDT
    {
        ACPI_SDT_HEADER Header;
        UINT32 OtherSDTs[0];
    }ACPI_RSDT;

    typedef struct RDSPExtension
    {
        UINT32 Lenght;
        ACPI_XSDT* XSDTAddress;
        UINT8 ExtendedChecksum;
        UINT8 Reserved[3];
    }ACPI_RDSPExtension;

    typedef struct RDSP
    {
        UINT8 Singature[8];
        UINT8 Checksum;
        UINT8 OEMID[6];
        UINT8 Revision;
        UINT32 RSDTAddress;
        ACPI_RDSPExtension v20;

    }ACPI_RDSP;

    typedef struct DSDT
    {
        ACPI_SDT_HEADER Header;
        UINT8 amlCode[0];
    }ACPI_DSDT;

    typedef struct FADT
    {
        ACPI_SDT_HEADER Header;
        UINT32 FirmwareCtrl;
        UINT32 DSDT;
        UINT8 Reserved;
        UINT8 PreferredPowerManagmnetProfile;
        UINT16 SCIInterrupt;
        UINT32 SMICommandPort;
        UINT8 ACPIEnable;
        UINT8 ACPIDisable;
        UINT8 S4BIOSREQ;
        UINT8 PSTATECTRL;
        UINT32 PM1aEventBlock;
        UINT32 PM1bEventBlock;
        UINT32 PM1aControlBlock;
        UINT32 PM1bControlBlock;
        UINT32 PM2ControlBlock;
        UINT32 PMTimerBlock;
        UINT32 GPE0Block;
        UINT32 GPE1Block;
        UINT8 PM1EventLenght;
        UINT8 PM1ControlLenght;
        UINT8 PM2ControlLenght;
        UINT8 PMTimerLenght;
        UINT8 GPE0Lenght;
        UINT8 GPE1Lenght;
        UINT8 GPE1Base;
        UINT8 CSTATECTRL;
        UINT16 WorstC2Latency;
        UINT16 WorstC3Latency;
        UINT16 FlushSize;
        UINT16 FlushStride;
        UINT8 DutyOffset;
        UINT8 DutyWidth;
        UINT8 Day;
        UINT8 Month;
        UINT8 CenturyRegister;

        UINT16 BootArchitectureFlags;
        UINT8 Reserved2;
        UINT32 Flags;

        //ACPI 2.0+
        ACPI_GAS ResetReg;
        UINT8 ResetValue;
        UINT8 Reserved3[3];

        UINT64 X_FirmwareCtrl;
        UINT64 X_DSDT;

        ACPI_GAS X_PM1aEventBlock;
        ACPI_GAS X_PM1bEventBlock;
        ACPI_GAS X_PM1aControlBlock;
        ACPI_GAS X_PM1bControlBlock;
        ACPI_GAS X_PM2ControlBlock;
        ACPI_GAS X_PMTimerBlock;
        ACPI_GAS X_GPE0Block;
        ACPI_GAS X_GPE1Block;
    }ACPI_FADT, ACPI_FACP;

    typedef struct ACPI_MADT_ENTRY
    {
        UINT8 Type;
        UINT8 Length;
    }ACPI_MADT_ENTRY;

    typedef struct MADT
    {
        ACPI_SDT_HEADER Header;
        UINT32 LapicBase32;
        UINT32 Flags;
        ACPI_MADT_ENTRY InteruptControlerStruct;
    }ACPI_MADT, ACPI_APIC;

#define ACPI_MADT_TYPE_LAPIC                0x00
#define ACPI_MADT_TYPE_IOAPIC                0x01
#define ACPI_MADT_TYPE_SOURCE_OVERRIDE        0x02
#define ACPI_MADT_TYPE_IOAPIC_NMI_SOURCE    0x03
#define ACPI_MADT_TYPE_LAPIC_NMI_SOURCE        0x04
#define ACPI_MADT_TYPE_LAPIC_ADDRESS_64        0x05
#define ACPI_MADT_TYPE_X2LAPIC                0X09

    typedef struct ACPI_MADT_LAPIC
    {
        ACPI_MADT_ENTRY Header;
        UINT8 ProcessorUid;
        UINT8 LapicID;
        UINT32 Flags;
    }ACPI_MADT_LAPIC;

    typedef struct ACPI_MADT_IOAPIC
    {
        ACPI_MADT_ENTRY Header;
        UINT8 IoApicId;
        UINT8 Reserved;
        UINT32 IoApicAddress;
        UINT32 IoApicInterruptBase;
    }ACPI_MADT_IOAPIC;

    typedef struct ACPI_MADT_SOURCE_OVERRIDE
    {
        ACPI_MADT_ENTRY Header;
        UINT8 Bus;
        UINT8 Source;
        UINT32 GlobalSystemInterrupt;
        UINT16 Flags;
    }ACPI_MADT_SOURCE_OVERRIDE;

    typedef struct ACPI_MADT_IOAPIC_NMI_SOURCE
    {
        ACPI_MADT_ENTRY Header;
        UINT16 Flags;
        UINT32 GlobalSystemInterrupt;
    }ACPI_MADT_IOAPIC_NMI_SOURCE;

    typedef struct ACPI_MADT_LAPIC_NMI_SOURCE
    {
        ACPI_MADT_ENTRY Header;
        UINT8 ProcessorUid;
        UINT16 Flags;
        UINT8 LapicLintN;
    }ACPI_MADT_LAPIC_NMI_SOURCE;

    typedef struct ACPI_MADT_LAPIC_ADDRESS_64
    {
        ACPI_MADT_ENTRY Header;
        UINT16 Reserved;
        UINT64 AddressOverride;
    }ACPI_MADT_LAPIC_ADDRESS_64;

    typedef struct ACPI_MADT_X2LAPIC
    {
        ACPI_MADT_ENTRY Header;
        UINT16 Reserved;
        UINT32 X2LapicId;
        UINT32 Flags;
        UINT32 ProcessorUid;
    }ACPI_MADT_X2LAPIC;

    typedef struct
    {
        UINT8 name[4];
    } AML_NAME;

#pragma pack(pop)

#define AML_OPCODE_ZEROOPCODE 0
#define AML_OPCODE_ONEOPCODE 1
#define AML_OPCODE_ALIASOPCODE 0X6
#define AML_OPCODE_NAMEOPCODE 0X8
#define AML_OPCODE_BYTEPREFIX 0XA
#define AML_OPCODE_WORDPREFIX 0XB
#define AML_OPCODE_DWORDPREFIX 0XC
#define AML_OPCODE_STRINGPREFIX 0XD
#define AML_OPCODE_QWORDPREFIX 0XE
#define AML_OPCDOE_SCOPEOPCODE 0X10
#define AML_OPCODE_BUFFEROPCODE 0X11
#define AML_OPCODE_PACKAGEOPCODE 0X12
#define AML_OPCODE_VARIABLEPACKAGEOPCODE 0X13
#define AML_OPCODE_METHODOPCODE 0x14
#define AML_OPCODE_EXTERNAL 0X15
#define AML_OPCODE_DUALNAMEPREFIX 0x2e
#define AML_OPCODE_MULTINAMEPREFIX 0X2F
#define AML_OPCODE_EXTOPPREFIX 0X5B
#define AML_OPCODE_ROOTCHAROPCODE 0X5C
#define AML_OPCODE_PARENTPREFIXCHAR 0X5E
#define AML_OPCODE_NAMECHAROPCODE 0X5F
#define AML_OPCODE_LOCAL0OPCODE 0X60
#define AML_OPCODE_LOCAL1OPCODE 0X61
#define AML_OPCODE_LOCAL2OPCODE 0X62
#define AML_OPCODE_LOCAL3OPCODE 0X63
#define AML_OPCODE_LOCAL4OPCODE 0X64
#define AML_OPCODE_LOCAL5OPCODE 0X65
#define AML_OPCODE_LOCAL6OPCODE 0X66
#define AML_OPCODE_LOCAL7OPCODE 0X67
#define AML_OPCODE_ARG0OPCODE 0X68
#define AML_OPCODE_ARG1OPCODE 0X69
#define AML_OPCODE_ARG2OPCODE 0X6A
#define AML_OPCODE_ARG3OPCODE 0X6B
#define AML_OPCODE_ARG4OPCODE 0X6C
#define AML_OPCODE_ARG5OPCODE 0X6D
#define AML_OPCODE_ARG6OPCODE 0X6E
#define AML_OPCODE_STOREOPCODE 0X70
#define AML_OPCODE_REFOFOPCODE 0X71
#define AML_OPCODE_ADDOPCODE 0X72
#define AML_OPCODE_CONCATOPCODE 0X73
#define AML_OPCODE_SUBTRACTOPCODE 0X74
#define AML_OPCODE_INCREMENTOPCODE 0X75
#define AML_OPCODE_DECREMENTOPCODE 0X76
#define AML_OPCODE_MULTIPLYOPCODE 0X77
#define AML_OPCODE_DIVIDEOPCODE 0X78
#define AML_OPCODE_SHIFTLEFTOPCODE 0X79
#define AML_OPCODE_SHIFTRIGHTOPCODE 0X7A
#define AML_OPCODE_ANDOPCODE 0X7B
#define AML_OPCODE_NANDOPCODE 0X7C
#define AML_OPCODE_OROPCODE 0X7D
#define AML_OPCODE_NOROPCODE 0X7E
#define AML_OPCODE_XOROPCODE 0X7F
#define AML_OPCODE_NOTOPCODE 0X80
#define AML_OPCODE_FINDSETLEFTBITOPCODE 0X81
#define AML_OPCODE_FINDSETRIGHTBITOPCODE 0X82
#define AML_OPCODE_DEREFOFOPCODE 0X83
#define AML_OPCODE_CONCATRESOPCODE 0X84
#define AML_OPCODE_MODOPCODE 0X85
#define AML_OPCODE_NOTIFYOPCODE 0X86
#define AML_OPCODE_SIZEOFOPCODE 0X87
#define AML_OPCODE_INDEXOPCODE 0X88
#define AML_OPCODE_MATCHOPCODE 0X89
#define AML_OPCODE_CREATEDWORDFIELDOPCODE 0X8A
#define AML_OPCODE_CREATEWORDFIELDOPCODE 0X8B
#define AML_OPCODE_CREATEBYTEFIELDOPCODE 0X8C
#define AML_OPCODE_CREATEBITFIELDOPCODE 0X8D
#define AML_OPCODE_TYPEOPCODE 0X8E
#define AML_OPCODE_CREATEQWORDFIELDOPCODE 0X8F
#define AML_OPCODE_LANDOPCODE 0X90
#define AML_OPCODE_LOROPCODE 0X91
#define AML_OPCODE_LNOTOPCODE 0X92
#define AML_OPCODE_CMP_LNOTEQUALOPCODE 0X93      
#define AML_OPCODE_CMP_LLESSEQUALOPCODE  0X94 
#define AML_OPCODE_CMP_LGREATEREQUALOPCODE 0X95
#define AML_OPCODE_LEQUALOPCODE 0X93
#define AML_OPCODE_LGREATEROPCODE 0X94
#define AML_OPCODE_LLESSOPCODE 0X95
#define AML_OPCODE_TOBUFFEROPCODE 0X96
#define AML_OPCODE_TODECIMALSTRINGOPCODE 0X97
#define AML_OPCODE_TOHEXSTRINGOPCODE 0X98
#define AML_OPCODE_TOINTEGEROPCODE 0X99
#define AML_OPCODE_TOSTRINGOPCODE 0X9C
#define AML_OPCODE_COPYOBJECTOPCODE 0X9D
#define AML_OPCODE_MIDOPCODE 0X9E
#define AML_OPCODE_CONTINUEOPCODE 0X9F
#define AML_OPCODE_IFOPCODE 0XA0
#define AML_OPCODE_ELSEOPCODE 0XA1
#define AML_OPCODE_WHILEOPCODE 0XA2
#define AML_OPCODE_NOOPOPCODE 0XA3
#define AML_OPCODE_RETURNOPCODE 0XA4
#define AML_OPCODE_BREAKOPCODE 0XA5
#define AML_OPCODE_BREAKPOINTOPCODE 0XCC
#define AML_OPCODE_ONESOPCODE 0XFF

#define AML_OPCODE_EXTOP_MUTEXOPCODE 0X1
#define AML_OPCODE_EXTOP_EVENTOPCODE 0X02
#define AML_OPCODE_EXTOP_CONDREFOFOPCODE 0X12
#define AML_OPCODE_EXTOP_CREATEFIELDOPCODE 0X13
#define AML_OPCODE_EXTOP_LOADTABLEOPCODE 0X1F
#define AML_OPCODE_EXTOP_LOADOPCODE 0X20
#define AML_OPCODE_EXTOP_STALLOPCODE 0X21
#define AML_OPCODE_EXTOP_SLEEPOPCODE 0X22
#define AML_OPCODE_EXTOP_ACQUIREOPCODE 0X23
#define AML_OPCODE_EXTOP_SIGNALOPCODE 0X24
#define AML_OPCODE_EXTOP_WAITOPCODE 0X25
#define AML_OPCODE_EXTOP_RESETOPCODE 0X26
#define AML_OPCODE_EXTOP_RELEASEOPCODE 0X27
#define AML_OPCODE_EXTOP_FROMBCDOPCODE 0X28
#define AML_OPCODE_EXTOP_TOBCDOPCODE 0X29
#define AML_OPCODE_EXTOP_UNLOADOPCODE 0X2A
#define AML_OPCODE_EXTOP_REVISIONOPCODE 0X30
#define AML_OPCODE_EXTOP_DEBUGOPCODE 0X31
#define AML_OPCODE_EXTOP_FATALOPCODE 0X32
#define AML_OPCODE_EXTOP_TIMEROPCODE 0X33
#define AML_OPCODE_EXTOP_OPREGIONOPCODE 0X80
#define AML_OPCODE_EXTOP_FIELDOPCODE 0X81
#define AML_OPCODE_EXTOP_DEVICEOPCODE 0X82
#define AML_OPCODE_EXTOP_PROCESSOROPCODE 0X83
#define AML_OPCODE_EXTOP_POWERRESOPCODE 0X84
#define AML_OPCODE_EXTOP_THERMALZONEOPCODE 0X85
#define AML_OPCODE_EXTOP_INDEXFIELDOPCODE 0X86
#define AML_OPCODE_EXTOP_BANKFIELDOPCODE 0X87
#define AML_OPCODE_EXTOP_DATAREGIONOPCODE 0X88

#define ACPI_SUCCESS 0
#define ACPI_ERROR_INVALID_RDSP 1
#define ACPI_ERROR_INVALID_XSDT 2
#define ACPI_ERROR_INVALID_DSDT 3
#define ACPI_ERROR_INVALID_RSDT 4
#define ACPI_ERROR_INVALID_FADT 4
#define ACPI_ERROR_INVALID_SDT 0x10
#define ACPI_ERROR_NOT_SUPPORTED_BY_ACPI_10 0x11
#define ACPI_ERROR_SDT_NOT_FOUND 0x12

#define ACPI_ERROR_AML_OBJECT_NOT_FOUND 0x89
#define ACPI_ERROR_AML_BUFFER_INVALID_SIZE 0x8a

#define ACPI_ERROR_MSG(a) PrintT("ACPI ERROR: %s\n",a)

    enum
    {
        tAMLByte = AML_OPCODE_BYTEPREFIX,
        tAMLWord = AML_OPCODE_WORDPREFIX,
        tAMLDword = AML_OPCODE_DWORDPREFIX,
        tAMLString = AML_OPCODE_STRINGPREFIX,
        tAMLQword = AML_OPCODE_QWORDPREFIX,
        tAMLBuffer = AML_OPCODE_BUFFEROPCODE,
        tAMLPackage = AML_OPCODE_PACKAGEOPCODE,
        tAMLInvalid = AML_OPCODE_ZEROOPCODE,
        tAMLFieldUnit = 256,
        tAMLName = AML_OPCODE_NAMEOPCODE
    };

    inline void AcpiError(UINT32 code)
    {
        switch (code)
        {
            case ACPI_SUCCESS:
                return;
            case ACPI_ERROR_INVALID_RDSP:
                ACPI_ERROR_MSG("INVALID RDSP");
                break;
            case ACPI_ERROR_INVALID_XSDT:
                ACPI_ERROR_MSG("INVALID XSDT");
                break;
            case ACPI_ERROR_INVALID_RSDT:
                ACPI_ERROR_MSG("INVALID RSDT");
                break;
            case ACPI_ERROR_INVALID_DSDT:
                ACPI_ERROR_MSG("INVALID DSDT");
                break;
            case ACPI_ERROR_INVALID_SDT:
                ACPI_ERROR_MSG("INVALID SDT");
                break;
            case ACPI_ERROR_NOT_SUPPORTED_BY_ACPI_10:
                ACPI_ERROR_MSG("The requested feature is not supported by ACPI version 1.0");
                break;
            case ACPI_ERROR_AML_BUFFER_INVALID_SIZE:
                ACPI_ERROR_MSG("The buffer exceeds the limits.");
                break;
            case ACPI_ERROR_SDT_NOT_FOUND:
                ACPI_ERROR_MSG("Requested ACPI SDT not found.");
            default:
                break;
        }
    }
    ACPI_XSDT* GetXsdt(ACPI_RDSP* rdsp);
    ACPI_RSDT* GetRsdt(ACPI_RDSP* rdsp);
    VOID* AcpiGetTable(ACPI_RDSP* rdsp, const char* name);
    UINT8 AcpiParseDSDT(ACPI_DSDT* table);
    UINT8 AcpiLastError();
    AML_NAME AcpiCreateName(const char* name);
    BOOL AcpiVerifyRdsp(ACPI_RDSP*);
    BOOL AcpiVerifySdt(ACPI_SDT_HEADER*);

#ifdef __cplusplus 
}
#endif

#endif