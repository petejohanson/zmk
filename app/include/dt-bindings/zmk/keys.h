
#pragma once

#define KC_A 0x04
#define KC_B 0x05
#define KC_C 0x06
#define KC_D 0x07
#define KC_E 0x08
#define KC_F 0x09
#define KC_G 0x0A
#define KC_H 0x0B
#define KC_I 0x0C
#define KC_J 0x0D
#define KC_K 0x0E
#define KC_L 0x0F
#define KC_M 0x10
#define KC_N 0x11
#define KC_O 0x12
#define KC_P 0x13
#define KC_Q 0x14
#define KC_R 0x15
#define KC_S 0x16
#define KC_T 0x17
#define KC_U 0x18
#define KC_V 0x19
#define KC_W 0x1A
#define KC_X 0x1B
#define KC_Y 0x1C
#define KC_Z 0x1D
#define KC_1 0x1E
#define KC_2 0x1F
#define KC_3 0x20
#define KC_4 0x21
#define KC_5 0x22
#define KC_6 0x23
#define KC_7 0x24
#define KC_8 0x25
#define KC_9 0x26
#define KC_0 0x27
#define KC_RET 0x28
#define KC_ESC 0x29
#define KC_DEL 0x2A
#define KC_BKSP KC_DEL
#define KC_TAB 0x2B
#define KC_SPC 0x2C
#define KC_MIN 0x2D
#define KC_EQL 0x2E
#define KC_LBKT 0x2F
#define KC_RBKT 0x30
#define KC_FSLH 0x31

#define KC_SCLN 0x33
#define KC_QUOT 0x34
#define KC_GRAV 0x35
#define KC_CMMA 0x36
#define KC_DOT 0x37
#define KC_BSLH 0x38
#define KC_CLCK 0x39
#define KC_F1 0x3A
#define KC_F2 0x3B

#define KC_RARW 0x4F
#define KC_LARW 0x50
#define KC_DARW 0x51
#define KC_UARW 0x52

#define KC_KDIV 0x54
#define KC_KMLT 0x55
#define KC_KMIN 0x56
#define KC_KPLS 0x57

#define KC_APP 0x65

#define KC_CURU 0xB4

#define KC_LPRN 0xB6
#define KC_RPRN 0xB7
#define KC_LCUR 0xB8
#define KC_RCUR 0xB9

#define KC_CRRT 0xC3
#define KC_PRCT 0xC4
#define KC_LABT 0xC5
#define KC_RABT 0xC6
#define KC_AMPS 0xC7
#define KC_PIPE 0xC9
#define KC_COLN 0xCB
#define KC_HASH 0xCC
#define KC_KSPC 0xCD
#define KC_ATSN 0xCE
#define KC_BANG 0xCF

#define KC_LCTL 0xE0
#define KC_LSFT 0xE1
#define KC_LALT 0xE2
#define KC_LGUI 0xE3
#define KC_RCTL 0xE4
#define KC_RSFT 0xE5
#define KC_RALT 0xE6
#define KC_RGUI 0xE7

#define KC_VOLU 0x80
#define KC_VOLD 0x81

/* The following are select consumer page usages */

#define KC_MNXT 0x100
#define KC_MPRV 0x101
#define KC_MSTP 0x102
#define KC_MJCT 0x103
#define KC_MPLY 0x104
#define KC_MMUT 0x105
#define KC_MVLU 0x106
#define KC_MVLD 0x107

#define ZC_TRNS (0xFFFF)
#define ZC_NO (0xFFFF - 1)

#define ZC_CSTM(n) (0xFFF + n)

#define MOD_LCTL (1 << 0x00)
#define MOD_LSFT (1 << 0x01)
#define MOD_LALT (1 << 0x02)
#define MOD_LGUI (1 << 0x03)
#define MOD_RCTL (1 << 0x04)
#define MOD_RSFT (1 << 0x05)
#define MOD_RALT (1 << 0x06)
#define MOD_RGUI (1 << 0x07)

#define ZK_ACTION(k) (k >> 24)
#define _ACTION(a) (a << 24)
#define _ACTION_MODS(m) (m << 16)
#define ZK_KEY(a) (a & 0xFFFF)
#define ZK_MODS(a) ((a >> 16) & 0xFF)

#define ZK_IS_CONSUMER(k) (ZK_KEY(k) >= 0x100)

#define ZMK_ACTION_KEY 0x01
#define ZMK_ACTION_MOD_TAP 0x01
#define ZMK_ACTION_ONE_SHOT 0x02

#define MT(mods, kc) (_ACTION(ZMK_ACTION_MOD_TAP) + _ACTION_MODS(mods) + kc)
