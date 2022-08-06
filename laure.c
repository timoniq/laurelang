#include "laurelang.h"
#include "laureimage.h"
#include "predpub.h"
#include <readline/readline.h>
#include <errno.h>
#include <signal.h>
#include <signal.h>
#include "compiler/compiler.h"

#include <unistd.h>

#define string char*
#define up printf("\033[A")
#define down printf("\n")
#define erase printf("\33[2K\r")
#define PATH_MAX 1024

#define str_starts(s, start) (strncmp(start, s, strlen(start)) == 0)

// Laurelang interpreter along with abstract machine
// uses semantic versioning.
#define VERSION        "0.1.0"
#define BUGTRACKER_URL "https://github.com/timoniq/laurelang"

#ifndef LL_RELEASE_BUILD
#define LL_PRE_RELEASE "-dev"
#else
#define LL_PRE_RELEASE ""
#endif

struct laure_flag {
    int    id;
    char  *name;
    char  *doc;
    bool   readword;
};

const struct laure_flag flags[] = {
    // 0
    {1, "-f", "Set main filename", true},
    {2, "-q", "Startup query", true},
    {3, "-norepl", "Don't run REPL after startup", false},
    {4, "--version", "Show version and quit", false},
    {5, "-clean", "Do not load std", false},
    {6, "-signal", "Usage with -q <..>, returns error code if predicate fails", false},
    {7, "--library", "Manually set lib_path", true},
    {8, "-D", "Add dyn flag. Format var=value", true},
    {9, "-nomain", "Do not run main predicate", false},
    {10, "--ignore", "Ignore consultation failures", false},
    {11, "-ws", "Enable weighted search", false},
    {12, "-b", "Consult bytecode", true},
    {12, "-bytecode", "Consult bytecode", true}
};

struct cmd_info {
    int id;
    string name;
    int argc; // -1 is any
    string help;
};

const struct cmd_info commands[] = {
    {0, ".consult", -1, "Consults files. Each arg is a filename."},
    {1, ".quit", 0, "Quits laurelang REPL."},
    {2, ".help", 0, "Shows this message."},
    {3, ".flags", 0, "Shows all available flags for laure interpreter."},
    {4, ".scope", 0, "Shows global scope values; needed for debug."},
    // {5, ...},
    {6, ".doc", 1, "Shows documentation for object."},
    {7, ".getinfo", 0, "Shows information about reasoning system."},
    {8, ".ast", -1, "Shows AST of query passed."},
    {9, ".lock", -1, "Locks instances."},
    {10, ".unlock", -1, "Unlocks instances."},
    {11, ".timeout", 1, "Sets timeout (in seconds)"},
    {12, ".ws", 0, "Toggle weighted search mode"},
    {13, ".backtrace", 0, "Show recent backtrace"}
};

struct filename_linked {
    string filename;
    struct filename_linked *next;
};

bool   FLAG_NOREPL = false;
bool   FLAG_CLEAN = false;
bool   FLAG_SIGNAL = false;
bool   FLAG_NOMAIN = false;
string FLAG_QUERY = NULL;
string FLAG_LIBRARY = NULL;

string                  INTERPRETER_PATH = NULL;
struct filename_linked *FILENAMES = NULL;
char                   _PATH[PATH_MAX] = {0};
char                    DPROMPT[32] = PROMPT;
FILE                   *BYTECODE[32] = {0};
uint                    BYTECODE_N   = 0;

typedef struct {
    int argc;
    string* argv;
} args_parsed;

args_parsed args_parse(string str) {
    string* result = 0;
    size_t count = 0;
    string tmp = str;
    string last_comma = 0;
    char delim[2];
    delim[0] = ' ';
    delim[1] = 0;

    while (*tmp) {
        if (' ' == *tmp) {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    count += last_comma < (str + strlen(str) - 1);
    count++;
    
    result = laure_alloc(sizeof(string) * count);

    if (result) {
        size_t idx  = 0;
        string token = strtok(str, delim);

        while (token) {
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        *(result + idx) = 0;
    }

    args_parsed ap;
    ap.argc = (int)count-1;
    ap.argv = result;
    return ap;
}

void print_header(string header, uint sz);
string convert_filepath(string filepath) {
    string new;
    if (str_starts(filepath, "@/")) {
        filepath++;
        new = laure_alloc(strlen(lib_path) + strlen(filepath));
        strcpy(new, lib_path);
        strcat(new, filepath);
    } else {
        new = strdup(filepath);
    }
    return new;
}

bool endswith(string s, string end);

string search_path(string original_path);

void sigint_handler(int _) {
    printf("\n%sCtrl-C%s: Goodbye\n", LAURUS_NOBILIS, NO_COLOR);
    exit(0);
}

void init_backtrace();

int laure_process_query(laure_session_t *session, string line) {

    LAURE_CLOCK = clock();

    while(strlen(line) > 0 && line[0] == ' ')
        line++;

    while(lastc(line) == ';') lastc(line) = 0;
    
    string startline = strdup(line);

    if (line[0] == '.') {
        if (str_eq(line, ".")) {up; erase; return 1;}
        args_parsed args = args_parse(line);
        bool found = false;
        for (int i = 0; i < sizeof(commands) / sizeof(struct cmd_info); i++) {
            struct cmd_info cmd = commands[i];
            if (str_eq(cmd.name, args.argv[0])) {
                found = true;
                if (cmd.argc != args.argc - 1 && cmd.argc != -1) {
                    printf("  requires %d args (got %d)\n", cmd.argc, args.argc - 1);
                    break;
                }
    // ----------
    switch (cmd.id) {
        case 0: {
            if (args.argc == 1) {
                printf("  %sUsage: .consult {files}%s\n", RED_COLOR, NO_COLOR);
                break;
            }
            for (int j = 1; j < args.argc; j++) {
                string path = convert_filepath(args.argv[j]);
                string spath = search_path(path);
                if (! spath) {
                    printf("  %s%s%s: unable to consult (path can't be found)\n", RED_COLOR, path, NO_COLOR);
                    laure_free(path);
                    laure_free(args.argv);
                    return 1;
                }

                string full_path = strdup( realpath(spath, _PATH) );
                bool failed[1];
                failed[0] = false;
                laure_consult_recursive(session, full_path, (int*)failed);
                if (! failed[0])
                    printf("  %s%s%s: consulted\n", GREEN_COLOR, args.argv[j], NO_COLOR);
                laure_free(path);
                laure_free(spath);
            }
            break;
        }
        case 1: {
            printf("Force quit\n");
            return 0;
        }
        case 2: {
            printf("help:\n");
            for (int i = 0; i < sizeof(commands) / sizeof(struct cmd_info); i++) {
                struct cmd_info cmd = commands[i];
                printf("  %s%s%s - %s\n", BOLD_WHITE, cmd.name, NO_COLOR, cmd.help);
            }
            break;
        }
        case 3: {
            printf("flags:\n");
            for (int i = 0; i < sizeof(flags) / sizeof(struct laure_flag); i++) {
                struct laure_flag flag = flags[i];
                printf("  %s%s%s - %s %s\n", BOLD_WHITE, flag.name, NO_COLOR, flag.doc, flag.readword ? "{arguments}" : "");
            }
            break;
        }
        case 4: {
            laure_scope_show(session->scope);
            break;
        }
        case 6: {
            Instance *ins = laure_scope_find_by_key(session->scope, args.argv[1], true);

            if (!ins) {
                printf("  %s%s is undefined%s\n", args.argv[1], RED_COLOR, NO_COLOR);
                laure_free(args.argv);
                return 1;
            }

            string doc = instance_get_doc(ins);

            printf("   ");

            if (!doc) {
                printf("%sno documentation for this instance%s", RED_COLOR, NO_COLOR);
            } else {
                bool color_set = false;
                for (int i = 0; i < strlen(doc); i++) {
                    char c = doc[i];
                    if (c == '\n') {
                        printf("\n   ");
                    } else if (c == '`') {
                        printf("%s", color_set ? NO_COLOR : LAURUS_NOBILIS);
                        color_set = color_set ? false : true;
                    } else {
                        printf("%c", c);
                    }
                }
            }

            printf("\n");
            break;
        }
        case 7: {
            printf("  %s%sλaurelang %s%s", BOLD_DEC, LAURUS_NOBILIS, VERSION, NO_COLOR);
            #ifdef GIT_VER
            printf(" (%s)\n", GIT_VER);
            #else
            printf("\n");
            #endif
            printf("  running `%s`\n", LAURE_INTERPRETER_PATH);
            printf("  bugtracker\n    %s%s%s\n", LAURUS_NOBILIS, BUGTRACKER_URL, NO_COLOR);
            printf("  %sbuild info%s:\n", BOLD_WHITE, NO_COLOR);
            printf("    %sscope build%s: %s\n", 
            BOLD_WHITE, NO_COLOR,
            #ifdef FEATURE_LINKED_SCOPE
            "linked (production)"
            #else
            "stodgy (debug)"
            #endif
            );
            printf("    %slib_path%s: `%s`\n", BOLD_WHITE, NO_COLOR, lib_path);
            printf("    %scompiled at%s: %s %s\n", BOLD_WHITE, NO_COLOR, __DATE__, __TIME__);
            if (LAURE_TIMEOUT != 0)
                printf("  %stimeout%s: %us\n", BOLD_WHITE, NO_COLOR, LAURE_TIMEOUT);
            else printf("  %stimeout%s: no\n", BOLD_WHITE, NO_COLOR);
            printf("  %sflags%s: [ ", BOLD_WHITE, NO_COLOR);
            if (FLAG_CLEAN) printf("CLEAN ");
            if (FLAG_NOREPL) printf("NOREPL ");
            if (FLAG_QUERY) printf("QUERY ");
            if (FLAG_SIGNAL) printf("SIGNAL ");
            printf("]\n");
            printf("  %sdflags%s: [ ", BOLD_WHITE, NO_COLOR);
            for (uint idx = 0; idx < DFLAG_N; idx++) {
                printf("%s=%s ", DFLAGS[idx][0], DFLAGS[idx][1]);
            }
            printf("]\n");
            break;
        }
        case 8: {
            string q = startline + 5;
            laure_parse_result result = laure_parse(q);
            if (!result.is_ok) {
                printf("  query is invalid: %s\n", result.err);
                break;
            }
            printf("\n");
            laure_expression_show(result.exp, 0);
            printf("\n");
            break;
        }
        case 9: {
            if (args.argc == 1) {
                printf("  %sUsage: .lock {names}%s\n", RED_COLOR, NO_COLOR);
            } else {
                for (int j = 1; j < args.argc; j++) {
                    string name = args.argv[j];
                    Instance *to_lock = laure_scope_find_by_key(session->scope, name, true);
                    if (! to_lock) {
                        printf("  %s %sis undefined%s\n", name, RED_COLOR, NO_COLOR);
                    } else {
                        instance_lock(to_lock);
                        printf("  %s%s %slocked%s\n", BOLD_WHITE, name, GREEN_COLOR, NO_COLOR);
                    }
                }
            }
            break;
        }
        case 10: {
            if (args.argc == 1) {
                printf("  %sUsage: .unlock {names}%s\n", RED_COLOR, NO_COLOR);
            } else {
                for (int j = 1; j < args.argc; j++) {
                    string name = args.argv[j];
                    Instance *to_unlock = laure_scope_find_by_key(session->scope, name, true);
                    if (! to_unlock) {
                        printf("  %s %sis undefined%s\n", name, RED_COLOR, NO_COLOR);
                    } else {
                        instance_unlock(to_unlock);
                        printf("  %s%s %sunlocked%s\n", BOLD_WHITE, name, GREEN_COLOR, NO_COLOR);
                    }
                }
            }
            break;
        }
        case 11: {
            string value_s = args.argv[1];
            int value;
            if (strcmp(value_s, "no") == 0) value = 0;
            else value = atoi(value_s);
            if (value < 0) break;
            LAURE_TIMEOUT = (uint)value;
            printf("  timeout was set to %us\n", LAURE_TIMEOUT);
            break;
        }
        case 12: {
            #ifdef DISABLE_WS
            printf("WS can't be enabled because of laurelang compilation settings\n");
            #else
            if (LAURE_WS) {
                LAURE_WS = false;
                printf("  WS disabled.\n");
            } else {
                LAURE_WS = true;
                printf("  WS enabled.\n");
            }
            #endif
            break;
        }
        case 13: {
            laure_backtrace_print(LAURE_BACKTRACE);
            break;
        }
        default: break;
    }
    // ----------
            }
        }
        if (! found) {
            printf("  unknown command `%s%s%s`, use %s%s%s\n", colored(args.argv[0] + 1), colored(".help"));
        }
        laure_free(args.argv);
        return 1;
    }

    int code = 1;
    if (str_starts(line, "\\")) line++;
    
    laure_parse_many_result res = laure_parse_many(line, ',', NULL);

    if (!res.is_ok) {
        printf("  %serror%s: parser( %s )\n", RED_COLOR, NO_COLOR, res.err);
        code = 2;
    } else {
        #ifdef DEBUG
        printf("AST: %s%s%s\n", colored("["));
        laure_expression_t *ptr;
        EXPSET_ITER(res.exps, ptr, {
            laure_expression_show(ptr, 2);
        });
        printf("%s%s%s\n", colored("]"));
        #endif

        laure_expression_set *expset = laure_expression_compose(res.exps);

        qcontext *qctx = qcontext_new(laure_expression_compose(expset));
        var_process_kit *vpk = laure_vpk_create(expset);
        control_ctx *cctx = control_new(session, session->scope, qctx, vpk, NULL, false);

        laure_backtrace_nullify(LAURE_BACKTRACE);
        qresp response = laure_start(cctx, expset);

        if (!laure_is_silent(cctx)) {
            if (response.state == q_error) {
                code = 2;
                if (LAURE_BACKTRACE && LAURE_BACKTRACE->cursor > 1) {
                    laure_backtrace_print(LAURE_BACKTRACE);
                }
                if (! LAURE_ACTIVE_ERROR) {
                    printf("  %serror%s: %s\n", RED_COLOR, NO_COLOR, (string)response.payload);
                } else {
                    char buff[512];
                    laure_error_write(LAURE_ACTIVE_ERROR, buff, 512);
                    printf("%s\n", buff);
                    laure_error_free(LAURE_ACTIVE_ERROR);
                    LAURE_ACTIVE_ERROR = NULL;
                }
            } else if (response.state == q_yield) {
                if (response.payload == (char*)0x1) {
                    printf("  true\n");
                } else if (response.payload == 0x0) {
                    code = 2;
                    printf("  false\n");
                }
            }
        }
        laure_scope_free(cctx->tmp_answer_scope);
        laure_free(cctx);
    }
    return code;
}

void add_filename(string str) {
    struct filename_linked filename = {str, NULL};
    struct filename_linked *ptr = laure_alloc(sizeof(struct filename_linked));
    *ptr = filename;
    if (! FILENAMES)
        FILENAMES = ptr;
    else {
        struct filename_linked *l = FILENAMES;
        while (l->next) {l = l->next;};
        l->next = ptr;
    }
}

string readline_wrapper() {
    #ifndef DISABLE_WS
    if (LAURE_WS) 
        printf("%sW%s ", BLUE_COLOR, NO_COLOR);
    #endif
    return readline(DPROMPT);
}

int laure_compiler_cli(laure_session_t *comp_session, int argc, char *argv[]);

int main(int argc, char *argv[]) {
    signal(SIGINT, sigint_handler);

    if (argc >= 2 && str_eq(argv[1], "help")) {
        if (argc == 2) {
            help1: {};
            printf("This laurelang interpreter version %s%s%s\n", LAURUS_NOBILIS, VERSION, NO_COLOR);
            printf("Pass knownledge base filenames to consult.\n");
            printf("Read documentation at %shttps://docs.laurelang.org%s\n", LAURUS_NOBILIS, NO_COLOR);
            printf("* %s%s help build%s to see build information\n", YELLOW_COLOR, argv[0], NO_COLOR);
        } else if (argc == 3) {
            if (str_eq(argv[2], "build")) {
                printf("Build information:\n");
                printf("%sVERSION%s: %s%s", BOLD_WHITE, NO_COLOR, VERSION, LL_PRE_RELEASE);
                #ifdef GIT_VER
                printf(" %s\n", GIT_VER);
                #else
                printf("\n");
                #endif
                #ifdef __VERSION__
                printf("%sCOMPILER%s: %s\n", BOLD_WHITE, NO_COLOR, __VERSION__);
                #endif
                printf("%sSCOPE BUILD%s: %s\n", 
                    BOLD_WHITE, NO_COLOR,
                    #ifdef FEATURE_LINKED_SCOPE
                    "linked"
                    #else
                    "stodgy"
                    #endif
                );
                printf("%sLIBRARY%s: \"%s\"\n", BOLD_WHITE, NO_COLOR, lib_path);
                printf("%sCOMPILED AT%s: %s %s\n", BOLD_WHITE, NO_COLOR, __DATE__, __TIME__);
            } else {
                printf("Unknown help option `%s`\n", argv[2]);
                goto help1;
            }
        } else {
            printf("Too much args passed\n");
            goto help1;
        }
        return 1;
    }

    if (argc >= 2 && str_eq(argv[1], "compile")) {
        laure_session_t comp_session[1];
        comp_session->scope = NULL;
        memset(comp_session->_included_filepaths, 0, included_fp_max * sizeof(void*));
        return laure_compiler_cli(comp_session, argc - 2, argv + 2);
    }

    for (int idx = 0; idx < argc; idx++) {
        string str = argv[idx];
        if (! idx) INTERPRETER_PATH = str;
        else {
            if (str[0] != '-') {
                add_filename(str);
                continue;
            } else {
                bool found = false;

                for (int i = 0; i < sizeof(flags) / sizeof(struct laure_flag); i++) {
                    struct laure_flag flag = flags[i];
                    if (strcmp(flag.name, str) == 0) {
                        found = true;
                        string word = NULL;
                        if (flag.readword) {
                            if (idx == argc - 1) {
                                printf("%sFlag %s requires an argument%s\n", RED_COLOR, flag.name, NO_COLOR);
                                continue;
                            }
                            idx++;
                            word = argv[idx];
                        }
            // ---------- 
            switch (flag.id) {
                case 1: {
                    add_filename(word);
                    break;
                }
                case 2: {
                    FLAG_QUERY = word;
                    break;
                }
                case 3: {
                    FLAG_NOREPL = true;
                    break;
                }
                case 4: {
                    printf("%s\n", VERSION);
                    return 0;
                }
                case 5: {
                    FLAG_CLEAN = true;
                    break;
                }
                case 6: {
                    FLAG_SIGNAL = true;
                    break;
                }
                case 7: {
                    FLAG_LIBRARY = word;
                    break;
                }
                case 8: {
                    string flagname = word;
                    string value = word;
                    while (value[0] != '=') value++;
                    value++;
                    if (value[0] == '\"' && lastc(value) == '\"') {
                        value++; lastc(value) = 0;
                    }
                    flagname[strlen(word) - strlen(value) - 1] = 0;
                    add_dflag(flagname, value);
                    break;
                }
                case 9: {
                    FLAG_NOMAIN = true;
                    break;
                }
                case 10: {
                    LAURE_ASK_IGNORE = true;
                    break;
                }
                case 11: {
                    #ifdef DISABLE_WS
                    printf("laurelang was compiled with DISABLE_WS flag set. so weighted search cannot be enabled\n");
                    #else
                    LAURE_WS = true;
                    #endif
                    break;
                }
                case 12: {
                    FILE *file = fopen(word, "rb");
                    if (! file) {
                        printf("%sFile (bytecode) %s is undefined%s\n", RED_COLOR, word, NO_COLOR);
                        break;
                    }
                    BYTECODE[BYTECODE_N] = file;
                    BYTECODE_N++;
                    break;
                }
            }}}

            if (!found)
                printf("  %sFlag %s is undefined%s\n", RED_COLOR, str, NO_COLOR);
            }
            // ----------
        }
    }

    printf("laurelang v%s%s\n", VERSION, LL_PRE_RELEASE);
    LAURE_INTERPRETER_PATH = INTERPRETER_PATH;

    string prompt = getenv("LLPROMPT");
    if (prompt) strcpy(DPROMPT, prompt);

    string timeout_s = getenv("LLTIMEOUT");
    if (timeout_s) LAURE_TIMEOUT = atoi(timeout_s);

    laure_session_t *session = laure_session_new();
    laure_set_translators();
    laure_init_name_buffs();
    laure_register_builtins(session);

    if (BYTECODE_N) {
        debug("%d bytecode source(s) should be loaded\n", BYTECODE_N);
        for (uint i = 0; i < BYTECODE_N; i++) {
            FILE *file = BYTECODE[i];
            debug("loading source n %d\n", i);
            if (file) {
                laure_consult_bytecode(session, file);
                fclose(file);
            }
        }
        printf("  %sBytecode Sources%s: consulted\n", GREEN_COLOR, NO_COLOR);
    }

    if (! FLAG_CLEAN) {
        string lp = FLAG_LIBRARY ? FLAG_LIBRARY : lib_path;
        string real_path = realpath(lp, _PATH);
        if (! real_path) {
            printf("Can't load standard lib from `%s`\n", lp);
        } else {
            string path = strdup( real_path );
            bool failed[1];
            failed[0] = false;
            laure_consult_recursive(session, path, (int*)failed);
        }
    }

    struct filename_linked *filenames = FILENAMES;
    while (filenames) {
        string path = convert_filepath(filenames->filename);
        string spath = search_path(path);
        if (! spath) {
            printf("  %s%s%s: unable to consult (path can't be found)\n", RED_COLOR, path, NO_COLOR);
            laure_free(path);
            if (FLAG_SIGNAL) return SIGABRT;
            filenames = filenames->next;
            continue;
        }

        string full_path = strdup( realpath(spath, _PATH) );
        bool failed[1];
        failed[0] = false;
        laure_consult_recursive(session, full_path, (int*)failed);
        if (! failed[0])
            printf("  %s%s%s: consulted\n", GREEN_COLOR, filenames->filename, NO_COLOR);
        else
            printf("  %s%s%s: not consulted\n", RED_COLOR, filenames->filename, NO_COLOR);
        laure_free(path);
        laure_free(spath);
        filenames = filenames->next;
    }

    if (FLAG_QUERY) {
        printf("%s%s\n", DPROMPT, FLAG_QUERY);
        int result = laure_process_query(session, FLAG_QUERY);
        if (FLAG_SIGNAL && result != 1) {
            return SIGABRT;
        }
    }

    if (FLAG_NOREPL) return 0;
    init_backtrace();

    if (setjmp(JUMPBUF)) {
        printf("  ↳ %sjumped off %s\n", YELLOW_COLOR, NO_COLOR);
    }

    string line;
    while ((line = readline_wrapper()) != NULL) {
        if (!strlen(line)) {
            erase;
            up;
            continue;
        }
        bool lp = false;
        while (laure_parser_needs_continuation(line)) {
            if (lastc(line) == '\\') lastc(line) = 0;
            
            if (lp) up;
            up; erase;
            printf("(%s%s%s)\n", GREEN_COLOR, line, NO_COLOR); erase;

            string continuation = readline("> ");
            if (! continuation) break;
            
            char nline[512];
            snprintf(nline, 512, "%s%s", line, continuation);
            laure_free(line);
            
            line = strdup( nline );
            lp = true;
        }
        if (lastc(line) == '.') lastc(line) = 0;
        if (lastc(line) == '%') lastc(line) = '\r';
        
        if (lp) {up; up; erase; printf("%s%s\n", DPROMPT, line); erase;}
        int res = laure_process_query(session, line);
        if (!res) break;
        laure_free(line);
    }
    
    return 0;
}
