#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "laurelang.h"

size_t laure_string_strlen(const char *s) {
	if (!s) return 0;
	
    size_t len = 0;

    while (*s) {
        unsigned char c = *(const unsigned char *)s++;
        if ((c < 0x80) || c > 0xBF)
            len++;
    }

    return len;
}

size_t laure_string_substrlen(const char *s, size_t n) {
    const char *end = s + n;
    size_t len = 0;

    while (s < end) {
        unsigned char c = *(const unsigned char *)s++;
        if ((c < 0x80) || (c > 0xBF))
            len++;
    }

    return len;
}

bool laure_string_is_char(const string s) {
    unsigned char c = *(const unsigned char *)s;
    return (c >= 0x80) && (c <= 0xBF);
}


// utf-8 processing taken from trealla
// source: https://github.com/infradig/trealla/blob/main/src/utf8.c

int laure_string_put_len(int c_) {
    uint c = (uint)c_;

    int len = 0;
    if (c <= 0x7F)
        len = 1;
    else if (c <= 0x07FF)
        len = 2;
    else if (c <= 0xFFFF)
        len = 3;
    else if (c <= 0x010FFFF)
        len = 4;
    else
        len = 0;
    
    return len;
}

int laure_string_put_char_bare(string _dst, int _ch)
{
	uint ch = (uint)_ch;
	unsigned char *dst = (unsigned char *)_dst;
	int len = 0;

	if (ch <= 0x7F) {
		*dst++ = ch;
		len = 1;
	} else if (ch <= 0x07FF) {
		*dst = 0b11000000;
		*dst++ |= (ch >> 6) & 0b00011111;
		*dst = 0b10000000;
		*dst++ |= (ch & 0b00111111);
		len = 2;
	} else if (ch <= 0xFFFF) {
		*dst = 0b11100000;
		*dst++ |= (ch >> 12) & 0b00001111;
		*dst = 0b10000000;
		*dst++ |= (ch >> 6) & 0b00111111;
		*dst = 0b10000000;
		*dst++ |= ch & 0b00111111;
		len = 3;
	} else if (ch <= 0x010FFFF) {
		*dst = 0b11110000;
		*dst++ |= (ch >> 18) & 0b00000111;
		*dst = 0b10000000;
		*dst++ |= (ch >> 12) & 0b00111111;
		*dst = 0b10000000;
		*dst++ |= (ch >> 6) & 0b00111111;
		*dst = 0b10000000;
		*dst++ |= ch & 0b00111111;
		len = 4;
	} else
		len = 0;

	return len;
}

int laure_string_get_char(string* _s) {
    const unsigned char *src = (const unsigned char *)*_s;
	int expect = 1;
	unsigned int n = 0;

	while (expect--) {
		unsigned char ch = *src++;

		if ((ch & 0b11111100) == 0b11111100) {
			n = ch & 0b00000001;
			expect = 5;
		} else if ((ch & 0b11111000) == 0b11111000) {
			n = ch & 0b00000011;
			expect = 4;
		} else if ((ch & 0b11110000) == 0b11110000) {
			n = ch & 0b00000111;
			expect = 3;
		} else if ((ch & 0b11100000) == 0b11100000) {
			n = ch & 0b00001111;
			expect = 2;
		} else if ((ch & 0b11000000) == 0b11000000) {
			n = ch & 0b00011111;
			expect = 1;
		} else if ((ch & 0b10000000) == 0b10000000) {
			n <<= 6;
			n |= ch & 0b00111111;
		} else {
			n = ch;
		}
	}

	*_s = (const char *)src;
	return (int)n;
}

int laure_string_put_char(string dst, int c) {
    int len = laure_string_put_char_bare(dst, c);
    dst[len] = '\0';
    return len;
}

int laure_string_peek_char(const string s) {
    return laure_string_get_char(&s);
}

size_t laure_string_len_char(const string _s) {
    const string s = _s;
    laure_string_get_char(&s);
    return s - _s;
}

int laure_string_getc(int (*fn)(), void *p) {
    unsigned int n = 0;
	int expect = 1;

	while (expect--) {
		int _ch = fn(p);

		if (_ch == EOF)
			return EOF;

		unsigned char ch = (unsigned char)_ch;

		if ((ch & 0b11111100) == 0b11111100) {
			n = ch & 0b00000001;
			expect = 5;
		} else if ((ch & 0b11111000) == 0b11111000) {
			n = ch & 0b00000011;
			expect = 4;
		} else if ((ch & 0b11110000) == 0b11110000) {
			n = ch & 0b00000111;
			expect = 3;
		} else if ((ch & 0b11100000) == 0b11100000) {
			n = ch & 0b00001111;
			expect = 2;
		} else if ((ch & 0b11000000) == 0b11000000) {
			n = ch & 0b00011111;
			expect = 1;
		} else if ((ch & 0b10000000) == 0b10000000) {
			n <<= 6;
			n |= ch & 0b00111111;
		} else {
			n = ch;
		}
	}

	return (int)n;
}

int laure_string_char_at_pos(const string buff, size_t buff_len, size_t i) {
    const string s = buff;
    size_t idx = 0;
    while (s < (buff + buff_len)) {
        int c = laure_string_get_char(&s);
        if (idx++ == i)
            return c;
    }
    return 0;
}

size_t laure_string_offset_at_pos(const string buff, size_t buff_len, size_t i) {
    const string s = buff;
    size_t idx = 0;
    
    while (s < (buff + buff_len)) {
        if (idx++ == i)
            break;
        laure_string_get_char(&s);
    }
    return s - buff;
}