// laurelang compiler
// (c) Arseny Kriuchkov, 2022

#include "../src/laurelang.h"
#include "../src/predpub.h"
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

typedef unsigned char ID;
typedef unsigned char H;

// HEADER
#define HEADER_BITS 5

#ifndef ID_MAX
// not greater than 256
#define ID_MAX 256
#endif

/* NAME DECLARATION
   1 byte - N length of name
   N bytes - name
*/

/* Case Declaration
   1 byte - Predicate ID
   Y exprs before endblock
*/
#define CH_preddecl     0

/* Header Declaration
   {NAME DECLARATION}
   1 byte - given ID
   1 bit - is abstract template
   1 bit - R has resp
   X+R exprs args before endblock
*/
#define CH_preddeclHead 1
#define CH_cnstrHead    2

/* Call
   1 byte - ID
   X exprs of arguments before endblock
*/
#define CH_call         3


/* Assert to value
   1 byte - variable ID
   {SIMILAR TO NAME DECLARATION}
*/
#define CH_assertV2VAL  4

/* Assert to variable
   1 byte - variable1 ID
   1 byte - variable2 ID
*/
#define CH_assertV2V    5

/* Image variable (standart variable ~ variable)
   1 byte - variable1 ID
   1 byte - variable2 ID
*/
#define CH_img          6

/* Image variable non standart
   2 expressions (1 ~ 2)
*/
#define CH_imgNS        7

/* Use/useso command
   {NAME DECLARATION}
*/
#define CH_cmd_use      8
#define CH_cmd_useso    9

/* Error
   {NAME DECLARATION}
*/
#define CH_cmd_error    10
/* Command set
   {NAME DECLARATION} - name
   {NAME DECLARATION} - value
*/
#define CH_cmd_set      11

/* Nothing */
#define CH_cut          12
#define CH_endblock     13
#define CH_endblockWcut 14

/* Variable / unify / nested var
   1 byte - variable ID
   if nestedvar:
      COUNT_BITS_NESTING bits nesting
*/
#define COUNT_BITS_NESTING 3
#define CH_var          15
#define CH_unify        16
#define CH_nestedvar    17


#define CH_startblock   18

/* Give ID
   1 byte - given ID
   {NAME DECLARATION}
*/
#define CH_give_id      19

/* Expression set isolated
   N expressions until ENDBLOCK
*/
#define CH_isol_start   20

/* DATA (expression let_custom)
   {SIMILAR TO NAME DECLARATION}
*/
#define CH_data         21

/* Implication
   X expressions until endblock : fact set
   Y expressions until endblock : implies for
*/
#define CH_implication  22

/*
11001 11010 11100 
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
int  bitstream_read_bit(Bitstream *bs);
bool bitstream_load_byte(Bitstream *bs);

bool laure_compiler_compile_expression(
    laure_expression_t *expr,
    FILE *writable_stream
);

void laure_compiler_consult_bytecode(laure_session_t *session, Bitstream *bits);
void laure_consult_bytecode(laure_session_t *session, FILE *file);
int laure_compiler_cli(laure_session_t *comp_session, int argc, char *argv[]);

// SIGNATURE
extern unsigned char SIGNATURE[];

#define SIGNATURE_CHARBITS 4
#define SIGNATURE_LENGTH   9

void write_signature(FILE *file);