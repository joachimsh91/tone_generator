// unicode.c
#include "ff.h"

// Converts small characters to big characters
WCHAR ff_wtoupper(WCHAR chr) {
    if (chr >= 'a' && chr <= 'z') {
        chr -= 0x20;
    }
    return chr;
}

// Dummy-converting between Unicode and OEM
WCHAR ff_convert(WCHAR chr, UINT dir) {
    return chr;
}
