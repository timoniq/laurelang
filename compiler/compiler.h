// laurelang compiler
// (c) Arseny Kriuchkov, 2022

#include "../src/laurelang.h"
#include <limits.h>
#pragma once 

/* COMPILATION FLAGS
*/
extern void CF_NOPE;


struct PredicateCompileData {
    string name;
    bool has_resp;
    uint argument_number;
};

// HEADER
#define HEADER_BITS 5
#define ID_MAX 128

/* NAME DECLARATION
   1 byte - N length of name
   N bytes - name
*/

/* Case Declaration
   1 byte - Predicate ID
   X exprs before endblock: Braced args
   1 expr of response if R
   Y exprs before endblock: Body
*/
#define CH_preddecl     0x00001

/* Header Declaration
   {NAME DECLARATION}
   1 byte - given ID
   1 bit - is primitive
   1 bit - is abstract template
   1 bit - R has resp
   X+R exprs args before endblock
*/
#define CH_preddeclHead 0x00010
#define CH_cnstrHead    0x00100

/* Call
   1 bit - known K
   if !K
       {NAME DECLARATION}
    X exprs of arguments before endblock
*/
#define CH_call         0x01000


/* Assert to value
   1 byte - variable ID
   1 byte - value length L
   L bytes - value
*/
#define CH_assertV2VAL  0x10000

/* Assert to variable
   1 byte - variable1 ID
   1 byte - variable2 ID
*/
#define CH_assertV2V    0x00011

/* Image variable
   1 byte - variable1 ID
   1 byte - variable2 ID
*/
#define CH_img          0x00101

/* Image + Assert to value
   1 byte - variable ID
   1 byte - variable to im ID
   1 byte value length L
   L bytes - value
*/
#define CH_img_asrt2VAL 0x00110

/* Image + Assert to variable
   1 byte variable ID
   1 byte variable to image ID
   1 byte variable to assert ID
*/
#define CH_img_asrt2V   0x01001

/* Use/useso command
   {NAME DECLARATION}
*/
#define CH_cmd_use      0x01010
#define CH_cmd_useso    0x01100

/* Error
   {NAME DECLARATION}
*/
#define CH_cmd_error    0x10001
/* Command set
   {NAME DECLARATION} - name
   {NAME DECLARATION} - value
*/
#define CH_cmd_set      0x10010

/* Nothing */
#define CH_cut          0x10100
#define CH_endblock     0x11000
#define CH_endblockWcut 0x00111

/* Variable / unify / nested var
   1 byte - variable ID
   if nestedvar:
      3 bits nesting
*/
#define COUNT_BITS_NESTING 3
#define CH_var          0x01011
#define CH_unify        0x01101
#define CH_nestedvar    0x01110


#define CH_startblock   0x10011

/* Give ID
   {NAME DECLARATION}
*/
#define CH_give_id      0x10101

/*
10101 10110 11001 11010 11100 
01111 10111 11011 11101 11110 
11111
*/

typedef struct Name {
    string s;
    bool flag;
} Name;

extern Name VARIABLE_IDS[ID_MAX];
extern uint VARIABLES_SIGNED;

typedef struct Bitstream {
    FILE *stream;
    unsigned char buf;
    unsigned int idx;
} Bitstream;

Bitstream *bitstream_new(FILE *stream);
bool bitstream_flush(Bitstream *bs);
bool bitstream_write_bit(Bitstream *bs, bool bit);

bool laure_compile_expression(
    laure_expression_t *expr,
    FILE *writable_stream
);