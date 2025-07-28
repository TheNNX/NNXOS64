#include <ps2.h>
#include <SimpleTextIo.h>
#include <HALX64/include/APIC.h>
#include <interrupt.h>
#include <cpu.h>

KEY_STATE state;
UINT8 ScancodeSet = 0;

static
BOOLEAN
SetScancodeSet(UINT8 setNumber);

static
UINT8
GetScancodeSet(VOID);

static
UINT8 
GetKeyOnInterrupt(VOID);

KEY(UNDEF, 0, 0, 0, 0, 0);
KEY(ESC, K_ESCAPE, 0, 0, 0, 0);
KEY(F1, K_F1, 0, 0, 0, 0);
KEY(F2, K_F2, 0, 0, 0, 0);
KEY(F3, K_F3, 0, 0, 0, 0);
KEY(F4, K_F4, 0, 0, 0, 0);
KEY(F5, K_F5, 0, 0, 0, 0);
KEY(F6, K_F6, 0, 0, 0, 0);
KEY(F7, K_F7, 0, 0, 0, 0);
KEY(F8, K_F8, 0, 0, 0, 0);
KEY(F9, K_F9, 0, 0, 0, 0);
KEY(F10, K_F10, 0, 0, 0, 0);
KEY(F11, K_F11, 0, 0, 0, 0);
KEY(F12, K_F12, 0, 0, 0, 0);
KEY(F13, K_F13, 0, 0, 0, 0);
KEY(F14, K_F14, 0, 0, 0, 0);
KEY(F15, K_F15, 0, 0, 0, 0);
KEY(F16, K_F16, 0, 0, 0, 0);
KEY(F17, K_F17, 0, 0, 0, 0);
KEY(F18, K_F18, 0, 0, 0, 0);
KEY(F19, K_F19, 0, 0, 0, 0);
KEY(F20, K_F20, 0, 0, 0, 0);
KEY(F21, K_F21, 0, 0, 0, 0);
KEY(F22, K_F22, 0, 0, 0, 0);
KEY(F23, K_F23, 0, 0, 0, 0);
KEY(F24, K_F24, 0, 0, 0, 0);
KEY(Backtick, K_BACKTICK, '`', '~', '`', '~');
KEY(1, K_1, '1', '!', '1', '!');
KEY(2, K_2, '2', '@', '2', '@');
KEY(3, K_3, '3', '#', '3', '#');
KEY(4, K_4, '4', '$', '4', '$');
KEY(5, K_5, '5', '%', '5', '%');
KEY(6, K_6, '6', '^', '6', '^');
KEY(7, K_7, '7', '&', '7', '&');
KEY(8, K_8, '8', '*', '8', '*');
KEY(9, K_9, '9', '(', '9', '(');
KEY(0, K_0, '0', ')', '0', ')');

KEY(Minus, K_MINUS, '-', '_', '-', '_');
KEY(Equals, K_PLUS, '=', '+', '=', '+');
KEY(Backspace, K_BACK, 8, 8, 8, 8);
KEY(TAB, K_TAB, '\t', '\t', '\t', '\t');
KEY(Q, K_Q, 'q', 'Q', 'Q', 'q');
KEY(W, K_W, 'w', 'W', 'W', 'w');
KEY(E, K_E, 'e', 'E', 'E', 'e');
KEY(R, K_R, 'r', 'R', 'R', 'r');
KEY(T, K_T, 't', 'T', 'T', 't');
KEY(Y, K_Y, 'y', 'Y', 'Y', 'y');
KEY(U, K_U, 'u', 'U', 'U', 'u');
KEY(I, K_I, 'i', 'I', 'I', 'i');
KEY(O, K_O, 'o', 'O', 'O', 'o');
KEY(P, K_P, 'p', 'P', 'P', 'p');

KEY(SQB_Open, K_SQ_BRACKET_OPEN, '[', '{', '[', '{');
KEY(SQB_Close, K_SQ_BRACKET_CLOSE, ']', '}', ']', '}');
KEY(Backslash, K_BACKSLASH, '\\', '|', '\\', '|');

KEYOnOff(Capslock, K_CAPSLOCK);
KEY(A, K_A, 'a', 'A', 'A', 'a');
KEY(S, K_S, 's', 'S', 'S', 's');
KEY(D, K_D, 'd', 'D', 'D', 'd');
KEY(F, K_F, 'f', 'F', 'F', 'f');
KEY(G, K_G, 'g', 'G', 'G', 'g');
KEY(H, K_H, 'h', 'H', 'H', 'h');
KEY(J, K_J, 'j', 'J', 'J', 'j');
KEY(K, K_K, 'k', 'K', 'K', 'k');
KEY(L, K_L, 'l', 'L', 'L', 'l');
KEY(Semicolon, K_SEMICOLON, ';', ':', ';', ':');
KEY(Quote, K_QUOTE, '\'', '"', '\'', '"');
KEY(Enter, K_ENTER, '\n', '\n', '\n', '\n');
KEYSide(LShift, K_LSHIFT, K_SHIFT);
KEY(Z, K_Z, 'z', 'Z', 'Z', 'z');
KEY(X, K_X, 'x', 'X', 'X', 'x');
KEY(C, K_C, 'c', 'C', 'C', 'c');
KEY(V, K_V, 'v', 'V', 'V', 'v');
KEY(B, K_B, 'b', 'B', 'B', 'b');
KEY(N, K_N, 'n', 'N', 'N', 'n');
KEY(M, K_M, 'm', 'M', 'M', 'm');
KEY(Comma, K_COMMA, ',', '<', ',', '<');
KEY(Period, K_PERIOD, '.', '>', '.', '>');
KEY(Slash, K_SLASH, '/', '?', '/', '?');
KEYSide(RShift, K_RSHIFT, K_SHIFT);
KEYSide(LCTRL, K_LCONTROL, K_CONTROL);
KEY(LWIN, K_LWIN, 0, 0, 0, 0);
KEY(RWIN, K_RWIN, 0, 0, 0, 0);
KEYSide(LALT, K_RMENU, K_MENU);
KEY(Space, K_SPACE, ' ', ' ', ' ', ' ');
KEYSide(RALT, K_RMENU, K_MENU);
KEYSide(RCTRL, K_RCONTROL, K_CONTROL);
KEY(PrintScreen, K_SNAPSHOT, 0, 0, 0, 0);

UINT8(*ScancodeSet2Keys[])() = { KeyUNDEF, KeyF9, KeyUNDEF, KeyF5, KeyF3, KeyF1, KeyF2, KeyF12, KeyUNDEF, KeyF10, KeyF8,
                                KeyF6, KeyF4, KeyTAB, KeyBacktick, KeyUNDEF, KeyUNDEF, KeyLALT, KeyLShift, KeyUNDEF, KeyLCTRL, KeyQ,
                                Key1, KeyUNDEF, KeyUNDEF, KeyUNDEF, KeyZ, KeyS, KeyA, KeyW, Key2, KeyUNDEF, KeyUNDEF, KeyC, KeyX, KeyD,
                                KeyE, Key4, Key3, KeyUNDEF, KeyUNDEF, KeySpace, KeyV, KeyF, KeyT, KeyR, Key5, KeyUNDEF, KeyUNDEF,
                                KeyN, KeyB, KeyH, KeyG, KeyY, Key6, KeyUNDEF, KeyUNDEF, KeyUNDEF, KeyM, KeyJ, KeyU, Key7, Key8, KeyUNDEF,
                                KeyUNDEF, KeyComma, KeyK, KeyI, KeyO, Key0, Key9, KeyUNDEF, KeyUNDEF, KeyPeriod, KeySlash, KeyL, KeySemicolon,
                                KeyP, KeyMinus, KeyUNDEF, KeyUNDEF, KeyUNDEF, KeyQuote, KeyUNDEF, KeySQB_Open, KeyEquals, KeyUNDEF, KeyUNDEF,
                                KeyCapslock, KeyRShift, KeyEnter, KeySQB_Close, KeyUNDEF, KeyBackslash, KeyUNDEF, KeyUNDEF, KeyUNDEF,
                                KeyUNDEF, KeyUNDEF, KeyUNDEF, KeyUNDEF, KeyUNDEF, KeyBackspace };

static BOOLEAN KeyboardInitialized = FALSE;

static
BOOLEAN
KeyboardIsr(
    PKINTERRUPT InterruptObj,
    PVOID Ctx)
{
    char k = GetKeyOnInterrupt();
    if (k)
    {
        PrintT("%c\n", k);
    }

    Ps2FlushBuffer();
    return TRUE;
}

NTSTATUS 
Ps2KeyboardInitialize(VOID)
{
    PKINTERRUPT interrupt;
    UINT8 keyboardDeviceId;

    keyboardDeviceId = Ps2ResetPort1();
    if (keyboardDeviceId == -1)
    {
        return STATUS_INTERNAL_ERROR;
    }

    PrintT("Keyboard device id %i\n", keyboardDeviceId);
   
    ScancodeSet = GetScancodeSet();

    if (ScancodeSet != 2)
    {
        SetScancodeSet(2);
    }

    ScancodeSet = 2;

    IoCreateInterrupt(
        &interrupt, 
        KEYBOARD_VECTOR, 
        IrqHandler, 
        KeGetCurrentProcessorId(), 
        3, 
        FALSE,
        KeyboardIsr);

    /* FIXME */
    interrupt->IoApicVector = 1;
    interrupt->pfnSetMask = ApicSetInterruptMask;

    KeConnectInterrupt(interrupt);

    Ps2EnableInterruptPort1();
    KeyboardInitialized = TRUE;

    return STATUS_SUCCESS;
}

static
UINT8 
GetKeyOnInterrupt(VOID)
{
    UINT8(*key)();
    UINT8 scancode;

    if (!(Ps2ReadStatusPort() & PS2_STATUS_OUTPUT_READY))
    {
        return 0;
    }

    if (!KeyboardInitialized)
    {
        return 0;
    }
    
    scancode = Ps2ReadNoPoll();

    if (scancode == 0xf0)
    {
        scancode = Ps2ReadNoPoll();
        
        if (scancode > (sizeof(ScancodeSet2Keys) / sizeof(*ScancodeSet2Keys)))
        {
            return 0;
        }
        
        key = ScancodeSet2Keys[scancode];
        return key(1);
    }
    else if (scancode == 0xe0)
    {
        Ps2FlushBuffer();
        return 0;
    }

    if (scancode > (sizeof(ScancodeSet2Keys) / sizeof(*ScancodeSet2Keys)))
    {
        return 0;
    }
    
    key = ScancodeSet2Keys[scancode];
    return key(0);
}

static
UINT8 
GetScancodeSet(VOID)
{
    UINT8 byte;

    Ps2SendToPort1(0xF0);
    Ps2SendToPort1(0x0);
    
    byte = Ps2Read();

    if (byte == PS2_ACK)
    {
        while (byte = Ps2ReadNoPoll() == PS2_ACK);
        
        Ps2FlushBuffer();
        
        PrintT("BYTE: %X\n", byte);
        switch (byte)
        {
            case 0x43:    // TRANSLATED
            case 1:       // NOT TRANSLATED
                return KB_SCANCODESET1;
            case 0x41:    // TRANSLATED
            case 2:       // NOT TRANSLATED
                return KB_SCANCODESET2;
            case 0x3f:    // TRANSLATED
            case 3:       // NOT TRANSLATED
                return KB_SCANCODESET3;
            default:
                return KB_SCANCODESET_UNKNOWN;
        }
    }
    else
    {
        return GetScancodeSet();
    }

    return byte;
}

static
BOOLEAN 
SetScancodeSet(UINT8 setNumber)
{
    if (setNumber < KB_SCANCODESET1 || setNumber > KB_SCANCODESET3)
    {
        return FALSE;
    }

    Ps2SendToPort1(0xF0);
    Ps2SendToPort1(setNumber);
    
    return (Ps2Read()  == PS2_ACK);
}