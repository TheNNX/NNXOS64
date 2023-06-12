#ifndef NNX_KEYBOARD_HEADER
#define NNX_KEYBOARD_HEADER
#include <nnxtype.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KEYBOARD_PORT 0x60
#define KEYBOARD_COMMAND_PORT 0x64
#define KB_ACK 0xFA

#define KB_SCANCODESET1 1
#define KB_SCANCODESET2 2
#define KB_SCANCODESET3 3
#define KB_SCANCODESET_UNKNOWN 0xff

#define K_LBUTTON 1
#define K_RBUTTON 2
#define K_CANCEL 3
#define K_MBUTTON 4
#define K_BACK 8
#define K_TAB 9
#define K_CLEAR 0XC
#define K_RETURN 0X0D
#define K_ENTER 0X0D
#define K_SHIFT 0X10
#define K_CONTROL 0X11
#define K_MENU 0X12
#define K_PAUSE 0x13
#define K_CAPSLOCK 0X14
#define K_ESCAPE 0X1B
#define K_SPACE 0X20
#define K_PROR 0X21
#define K_NEXT 0X22
#define K_END 0X23
#define K_HOME 0X24
#define K_LEFT 0X25
#define K_UP 0X26
#define K_RIGHT 0X27
#define K_DOWN 0X28
#define K_SELECT 0X29
#define K_PRINT 0X2A
#define K_EXECUTE 0X2B
#define K_SNAPSHOT 0X2C
#define K_INSERT 0X2D
#define K_DELETE 0X2E
#define K_HELP 0X2F
#define K_0 0X30
#define K_1 0X31
#define K_2 0X32
#define K_3 0X33
#define K_4 0X34
#define K_5 0X35
#define K_6 0X36
#define K_7 0X37
#define K_8 0X38
#define K_9 0X39
#define K_A 0X41
#define K_B 0X42
#define K_C 0X43
#define K_D 0X44
#define K_E 0X45
#define K_F 0X46
#define K_G 0X47
#define K_H 0X48
#define K_I 0X49
#define K_J 0X4A
#define K_K 0X4B
#define K_L 0X4C
#define K_M 0X4D
#define K_N 0X4E
#define K_O 0X4F
#define K_P 0X50
#define K_Q 0X51
#define K_R 0X52
#define K_S 0X53
#define K_T 0X54
#define K_U 0X55
#define K_V 0X56
#define K_W 0X57
#define K_X 0X58
#define K_Y 0X59
#define K_Z 0X5A
#define K_LWIN 0x5b
#define K_RWIN 0X5C
#define K_APPS 0X5D
#define K_NUMPAD0 0x60
#define K_NUMPAD1 0x61
#define K_NUMPAD2 0x62
#define K_NUMPAD3 0x63
#define K_NUMPAD4 0x64
#define K_NUMPAD5 0x65
#define K_NUMPAD6 0x66
#define K_NUMPAD7 0x67
#define K_NUMPAD8 0x68
#define K_NUMPAD9 0x69
#define K_MULTIPLY 0x6A
#define K_ADD 0x6B
#define K_SEPARATOR 0x6C
#define K_SUBTRACT 0x6D
#define K_DECIMAL 0x6E
#define K_DIVIDE 0x6F
#define K_F1 0x70
#define K_F2 0x71
#define K_F3 0x72
#define K_F4 0x73
#define K_F5 0x74
#define K_F6 0x75
#define K_F7 0x76
#define K_F8 0x77
#define K_F9 0x78
#define K_F10 0x79
#define K_F11 0x7A
#define K_F12 0x7B
#define K_F13 0x7C
#define K_F14 0x7D
#define K_F15 0x7E
#define K_F16 0x7F
#define K_F17 0x80
#define K_F18 0x81
#define K_F19 0x82
#define K_F20 0x83
#define K_F21 0x84
#define K_F22 0x85
#define K_F23 0x86
#define K_F24 0x87
#define K_NUMLOCK 0x90
#define K_SCROLL 0x91
#define K_NUMPAD_EQUALS 0x92
#define K_LSHIFT 0xa0
#define K_RSHIFT 0xa1
#define K_LCONTROL 0xa2
#define K_RCONTROL 0xa3
#define K_LMENU 0xa4
#define K_RMENU 0xa5

#define K_SEMICOLON 0xba
#define K_PLUS 0xbb
#define K_COMMA 0xbc
#define K_MINUS 0xbd
#define K_PERIOD 0xbe
#define K_SLASH 0xbf
#define K_BACKTICK 0XC0
#define K_SQ_BRACKET_OPEN 0xdb
#define K_SQ_BRACKET_CLOSE 0xdd
#define K_BACKSLASH 0xdc
#define K_QUOTE 0xde

    struct KEY_STATE;
    extern struct KEY_STATE state;

#define keyname(x) Key ## x
#define KEY(name,numval,baseChar,shiftChar,capsChar,shiftCapsChar) UINT8 keyname(name)(UINT8 released){\
    state.KeyState[numval] = !released; \
    if(released)\
        return 0;\
    if(state.KeyState[K_SHIFT] && !state.KeyState[K_CAPSLOCK]){\
        return shiftChar;\
    }else if(!state.KeyState[K_SHIFT] && state.KeyState[K_CAPSLOCK]){\
        return capsChar;\
    }else if(state.KeyState[K_SHIFT] && state.KeyState[K_CAPSLOCK]){\
        return shiftCapsChar;\
    }\
    return baseChar;    \
}

#define KEYSide(name,numval,numvalMain) UINT8 keyname(name)(UINT8 released){\
    if(released){\
        state.KeyState[numvalMain] -= state.KeyState[numval];\
        state.KeyState[numval] = 0;\
    }else{\
        if(!state.KeyState[numval]){\
            state.KeyState[numval]++; \
            state.KeyState[numvalMain]++; \
        }\
    }\
    return 0;\
}

#define KEYOnOff(name,numval) UINT8 keyname(name)(UINT8 released){\
    if(!released)\
    {\
        state.KeyState[numval] = !state.KeyState[numval];\
    }\
    return 0;\
}


    extern UINT8(*ScancodeSet2Keys[])();

    extern UINT8 ScancodeSet;

    VOID KeyboardInitialize();
    UINT8 KeyboardInterrupt();
    UINT8 GetScancodeSet();
    UINT8 SetScancodeSet(UINT8);

#pragma pack(push, 1)
    //designed to be compatible with MS virtual key mappings
    typedef struct KEY_STATE
    {
        union
        {
            UINT8 KeyState[256];
            struct
            {
                UINT8 KEY00;
                UINT8 LBUTTON;
                UINT8 RBUTTON;
                UINT8 CANCEL;
                UINT8 MBUTTON;
                UINT8 KEY05;
                UINT8 KEY06;
                UINT8 KEY07;
                UINT8 BACK;
                UINT8 TAB;
                UINT8 KEY0A;
                UINT8 KEY0B;
                UINT8 CLEAR;
                union
                {
                    UINT8 RETURN;
                    UINT8 ENTER;
                };
                UINT8 KEY0E;
                UINT8 KEY0F;
                UINT8 SHIFT;
                UINT8 CONTROL;
                union
                {
                    UINT8 ALT;
                    UINT8 MENU;
                };
                UINT8 PAUSE;
                union
                {
                    UINT8 CAPSLOCK;
                    UINT8 CAPITAL;
                };
                UINT8 KEY15;
                UINT8 KEY16;
                UINT8 KEY17;
                UINT8 KEY18;
                UINT8 KEY19;
                UINT8 KEY1A;
                UINT8 ESCAPE;
                UINT8 CONVERT;
                UINT8 NONCONVERT;
                UINT8 ACCEPT;
                UINT8 MODECHANGE;
                UINT8 SPACE;
                UINT8 PRIOR;
                UINT8 NEXT;
                UINT8 END;
                UINT8 HOME;
                UINT8 LEFT;
                UINT8 UP;
                UINT8 RIGHT;
                UINT8 DOWN;
                UINT8 SELECT;
                UINT8 PRINT;
                UINT8 EXECUTE;
                UINT8 SNAPSHOT;
                UINT8 INSERT;
                UINT8 DELETEKEY;
                UINT8 HELP;
                union
                {
                    UINT8 NUMBERS[10];
                    struct
                    {
                        UINT8 NUMBER0;
                        UINT8 NUMBER1;
                        UINT8 NUMBER2;
                        UINT8 NUMBER3;
                        UINT8 NUMBER4;
                        UINT8 NUMBER5;
                        UINT8 NUMBER6;
                        UINT8 NUMBER7;
                        UINT8 NUMBER8;
                        UINT8 NUMBER9;

                    };
                };
                UINT8 KEY3A;
                UINT8 KEY3B;
                UINT8 KEY3C;
                UINT8 KEY3D;
                UINT8 KEY3E;
                UINT8 KEY3F;
                UINT8 KEY40;
                union
                {
                    UINT8 LETTERS[26];
                    struct
                    {
                        UINT8 LETTER_A;
                        UINT8 LETTER_B;
                        UINT8 LETTER_C;
                        UINT8 LETTER_D;
                        UINT8 LETTER_E;
                        UINT8 LETTER_F;
                        UINT8 LETTER_G;
                        UINT8 LETTER_H;
                        UINT8 LETTER_I;
                        UINT8 LETTER_J;
                        UINT8 LETTER_K;
                        UINT8 LETTER_L;
                        UINT8 LETTER_M;
                        UINT8 LETTER_N;
                        UINT8 LETTER_O;
                        UINT8 LETTER_P;
                        UINT8 LETTER_Q;
                        UINT8 LETTER_R;
                        UINT8 LETTER_S;
                        UINT8 LETTER_T;
                        UINT8 LETTER_U;
                        UINT8 LETTER_V;
                        UINT8 LETTER_W;
                        UINT8 LETTER_X;
                        UINT8 LETTER_Y;
                        UINT8 LETTER_Z;
                    };
                };
                UINT8 LWIN;
                UINT8 RWIN;
                UINT8 APPS;
                UINT8 KEY5E;
                UINT8 SLEEP;
                union
                {
                    UINT8 NUMPAD[10];
                    struct
                    {
                        UINT8 NUMPAD0;
                        UINT8 NUMPAD1;
                        UINT8 NUMPAD2;
                        UINT8 NUMPAD3;
                        UINT8 NUMPAD4;
                        UINT8 NUMPAD5;
                        UINT8 NUMPAD6;
                        UINT8 NUMPAD7;
                        UINT8 NUMPAD8;
                        UINT8 NUMPAD9;
                    };
                };
                UINT8 MULTIPLY;
                UINT8 ADD;
                UINT8 SEPARATOR;
                UINT8 SUBTRACT;
                UINT8 DECIMAL;
                UINT8 DIVIDE;
                union
                {
                    UINT8 FUNCTION[24];
                    struct
                    {
                        UINT8 F1;
                        UINT8 F2;
                        UINT8 F3;
                        UINT8 F4;
                        UINT8 F5;
                        UINT8 F6;
                        UINT8 F7;
                        UINT8 F8;
                        UINT8 F9;
                        UINT8 F10;
                        UINT8 F11;
                        UINT8 F12;
                        UINT8 F13;
                        UINT8 F14;
                        UINT8 F15;
                        UINT8 F16;
                        UINT8 F17;
                        UINT8 F18;
                        UINT8 F19;
                        UINT8 F20;
                        UINT8 F21;
                        UINT8 F22;
                        UINT8 F23;
                        UINT8 F24;
                    };
                };
                UINT8 KEY88;
                UINT8 KEY89;
                UINT8 KEY8A;
                UINT8 KEY8B;
                UINT8 KEY8C;
                UINT8 KEY8D;
                UINT8 KEY8E;
                UINT8 KEY8F;
                UINT8 NUMLOCK;
                UINT8 SCROLL;
                UINT8 NEC_EQUALS;
                UINT8 KEY93;
                UINT8 KEY94;
                UINT8 KEY95;
                UINT8 KEY96;
                UINT8 KEY97;
                UINT8 KEY98;
                UINT8 KEY99;
                UINT8 KEY9A;
                UINT8 KEY9B;
                UINT8 KEY9C;
                UINT8 KEY9D;
                UINT8 KEY9E;
                UINT8 KEY9F;
                UINT8 LSHIFT;
                UINT8 RSHIFT;
                UINT8 LCONTROL;
                UINT8 RCONTROL;
                UINT8 LMENU;
                UINT8 RMENU;
                UINT8 KEYA6;
                UINT8 KEYA7;
                UINT8 KEYA8;
                UINT8 KEYA9;
                UINT8 KEYAA;
                UINT8 KEYAB;
                UINT8 KEYAC;
                UINT8 KEYAD;
                UINT8 KEYAE;
                UINT8 KEYAF;
                UINT8 KEYB0;
                UINT8 KEYB1;
                UINT8 KEYB2;
                UINT8 KEYB3;
                UINT8 KEYB4;
                UINT8 KEYB5;
                UINT8 KEYB6;
                UINT8 KEYB7;
                UINT8 KEYB8;
                UINT8 KEYB9;
                UINT8 SEMICOLON;
                UINT8 PLUS;
                UINT8 COMMA;
                UINT8 MINUS;
                UINT8 PERIOD;
                UINT8 SLASH;
                UINT8 BACKTICK;
                UINT8 KEYC1;
                UINT8 KEYC2;
                UINT8 KEYC3;
                UINT8 KEYC4;
                UINT8 KEYC5;
                UINT8 KEYC6;
                UINT8 KEYC7;
                UINT8 KEYC8;
                UINT8 KEYC9;
                UINT8 KEYCA;
                UINT8 KEYCB;
                UINT8 KEYCC;
                UINT8 KEYCD;
                UINT8 KEYCE;
                UINT8 KEYCF;
                UINT8 KEYD0;
                UINT8 KEYD1;
                UINT8 KEYD2;
                UINT8 KEYD3;
                UINT8 KEYD4;
                UINT8 KEYD5;
                UINT8 KEYD6;
                UINT8 KEYD7;
                UINT8 KEYD8;
                UINT8 KEYD9;
                UINT8 KEYDA;
                UINT8 SQBRACKETOPEN;
                UINT8 BACKSLASH;
                UINT8 SQBRACKERCLOSE;
                union
                {
                    UINT8 QUOTE;
                    UINT8 SINGLEQUOTE;
                    UINT8 DOUBLEQUOTE;
                    UINT8 APOSTROPHE;
                };
                UINT8 KEYDF;
                UINT8 KEYE0;
                UINT8 KEYE1;
                UINT8 KEYE2;
                UINT8 KEYE3;
                UINT8 KEYE4;
                UINT8 KEYE5;
                UINT8 KEYE6;
                UINT8 KEYE7;
                UINT8 KEYE8;
                UINT8 KEYE9;
                UINT8 KEYEA;
                UINT8 KEYEB;
                UINT8 KEYEC;
                UINT8 KEYED;
                UINT8 KEYEE;
                UINT8 KEYEF;
                UINT8 KEYF0;
                UINT8 KEYF1;
                UINT8 KEYF2;
                UINT8 KEYF3;
                UINT8 KEYF4;
                UINT8 KEYF5;
                UINT8 KEYF6;
                UINT8 KEYF7;
                UINT8 KEYF8;
                UINT8 KEYF9;
                UINT8 KEYFA;
                UINT8 KEYFB;
                UINT8 KEYFC;
                UINT8 KEYFD;
                UINT8 KEYFE;
                UINT8 KEYFF;
            };
        };
    }KEY_STATE;


#ifdef __cplusplus
}
#endif

#pragma pack(pop)
#endif