#include <ps2.h>
#include <Port.h>
#include <SimpleTextIO.h>

UINT8
Ps2ReadStatusPort(VOID)
{
    return inb(PS2_STATUS_PORT);
}

VOID
Ps2PollWaitForOutputBuffer(VOID)
{
    while (!(inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_READY));
}

VOID
Ps2PollWaitForInputBuffer(VOID)
{
    while (inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT_FULL);
}

VOID
Ps2FlushBuffer(VOID)
{
    while (inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_READY)
    {
        inb(PS2_DATA_PORT);
    }
}

VOID
Ps2SendToPort1(UINT8 byte)
{
    Ps2PollWaitForInputBuffer();
    outb(PS2_DATA_PORT, byte);
}

VOID
Ps2SendToPort2(UINT8 byte)
{
    outb(PS2_COMMAND_PORT, PS2_COMMAND_SEND_BYTE_PORT2);

    Ps2PollWaitForInputBuffer();
    outb(PS2_DATA_PORT, byte);
}


UINT8
Ps2Read(VOID)
{
    Ps2PollWaitForOutputBuffer();
    return inb(PS2_DATA_PORT);
}

VOID 
Ps2SendToPort1NoPoll(UINT8 byte)
{
    outb(PS2_DATA_PORT, byte);
}

VOID 
Ps2SendToPort2NoPoll(UINT8 byte)
{
    outb(PS2_COMMAND_PORT, PS2_COMMAND_SEND_BYTE_PORT2);

    outb(PS2_DATA_PORT, byte);
}

UINT8 
Ps2ReadNoPoll(VOID)
{
    return inb(PS2_DATA_PORT);
}

static
UINT8
Ps2GetConfigByte(VOID)
{
    outb(PS2_COMMAND_PORT, PS2_COMMAND_READ_CCB);
    Ps2PollWaitForOutputBuffer();
    return inb(PS2_DATA_PORT);
}

static
VOID
Ps2SetConfigByte(UINT8 configByte)
{
    outb(PS2_COMMAND_PORT, PS2_COMMAND_WRITE_CCB);
    Ps2PollWaitForInputBuffer();
    outb(PS2_DATA_PORT, configByte);
}

VOID 
Ps2EnableInterruptPort1(VOID)
{
    Ps2SetConfigByte(Ps2GetConfigByte() | PS2_CCB_PS2_PORT1_INTERRUPT);
}

VOID 
Ps2EnableInterruptPort2(VOID)
{
    Ps2SetConfigByte(Ps2GetConfigByte() | PS2_CCB_PS2_PORT2_INTERRUPT);
}

UINT8
Ps2ResetPort1(VOID)
{
    UINT8 response;

    outb(PS2_COMMAND_PORT, PS2_COMMAND_ENABLE_PORT1);

    Ps2SendToPort1(0xFF);

    response = Ps2Read();
    if (response != 0xFA && response != 0xAA)
    {
        return -1;
    }

    response = Ps2Read();
    if (response != 0xFA && response != 0xAA)
    {
        return -1;
    }

    return 0;
}

UINT8
Ps2ResetPort2(VOID)
{
    UINT8 response;

    outb(PS2_COMMAND_PORT, PS2_COMMAND_ENABLE_PORT2);

    Ps2SendToPort2(0xFF);

    response = Ps2Read();
    if (response != 0xFA && response != 0xAA)
    {
        return -1;
    }

    response = Ps2Read();
    if (response != 0xFA && response != 0xAA)
    {
        return -1;
    }

    return Ps2Read();
}

NTSTATUS
Ps2Initialize(VOID)
{
    UINT8 configByte;

    PrintT("PS2 Init started\n");

    outb(PS2_COMMAND_PORT, PS2_COMMAND_TEST_CONTROLLER);
    Ps2PollWaitForOutputBuffer();
    if (inb(PS2_DATA_PORT) != 0x55)
    {
        return STATUS_INTERNAL_ERROR;
    }

    outb(PS2_COMMAND_PORT, 0xFF);

    configByte = Ps2GetConfigByte();

    configByte &= (~PS2_CCB_PS2_PORT1_TRANSLATION);
    configByte &= (~PS2_CCB_PS2_PORT1_INTERRUPT);
    configByte &= (~PS2_CCB_PS2_PORT2_INTERRUPT);

    Ps2SetConfigByte(configByte);

    return STATUS_SUCCESS;
}