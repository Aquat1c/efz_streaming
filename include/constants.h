#pragma once

// Memory offsets for EFZ Revival
#define EFZ_BASE_OFFSET_P1 0x390104
#define EFZ_BASE_OFFSET_P2 0x390108
#define EFZ_BASE_OFFSET_GAME_STATE 0x39010C

// Character name offset
#define CHARACTER_NAME_OFFSET 0x94

// Win count and nickname offsets from EfzRevival.dll+A02CC
#define WIN_COUNT_BASE_OFFSET 0xA02CC
#define P1_WIN_COUNT_OFFSET 0x4C8
#define P2_WIN_COUNT_OFFSET 0x4CC
#define P1_NICKNAME_OFFSET 0x3BE
#define P2_NICKNAME_OFFSET 0x43E

// Character IDs - corrected to match internal game IDs
#define CHAR_ID_AKANE     0
#define CHAR_ID_AKIKO     1
#define CHAR_ID_IKUMI     2
#define CHAR_ID_MISAKI    3
#define CHAR_ID_SAYURI    4
#define CHAR_ID_KANNA     5
#define CHAR_ID_KAORI     6
#define CHAR_ID_MAKOTO    7
#define CHAR_ID_MINAGI    8
#define CHAR_ID_MIO       9
#define CHAR_ID_MISHIO    10
#define CHAR_ID_MISUZU    11
#define CHAR_ID_MIZUKA    12
#define CHAR_ID_NAGAMORI  13
#define CHAR_ID_NANASE    14
#define CHAR_ID_EXNANASE  15
#define CHAR_ID_NAYUKI    16
#define CHAR_ID_NAYUKIB   17
#define CHAR_ID_SHIORI    18
#define CHAR_ID_AYU       19
#define CHAR_ID_MAI       20
#define CHAR_ID_MAYU      21
#define CHAR_ID_MIZUKAB   22
#define CHAR_ID_KANO      23

// Character names array
static const char* CHARACTER_NAMES[] = {
    "Akane", "Akiko", "Ikumi", "Misaki", "Sayuri", "Kanna",
    "Kaori", "Makoto", "Minagi", "Mio", "Mishio", "Misuzu",
    "Mizuka", "Nagamori", "Nanase", "ExNanase", "Nayuki", "NayukiB",
    "Shiori", "Ayu", "Mai", "Mayu", "MizukaB", "Kano"
};

#define MAX_CHARACTER_ID 23
#define MAX_NICKNAME_LENGTH 20

// HTTP server settings
#define HTTP_SERVER_PORT 8080
#define HTTP_RESPONSE_BUFFER_SIZE 2048
