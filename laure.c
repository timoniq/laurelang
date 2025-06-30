#include "laurelang.h"
#include "laureimage.h"
#include "predpub.h"
#include "repl.h"
#include <readline/readline.h>
#include <readline/history.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>

#include <unistd.h>

#define string char*
#define up printf("\033[A")
#define down printf("\n")
#define erase printf("\33[2K\r")
#define PATH_MAX 1024

#define CMD_CHAR '.'
#define CMD_EMPTY_STR "."

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
    {3, "--norepl", "Don't run REPL after startup", false},
    {4, "--version", "Show version and quit", false},
    {5, "--clean", "Do not load std", false},
    {6, "--signal", "Usage with -q <..>, returns error code if predicate fails", false},
    {7, "--library", "Manually set lib_path", true},
    {8, "-D", "Add dyn flag. Format var=value", true},
    {9, "--nomain", "Do not run main predicate", false},
    {10, "--ignore", "Ignore consultation failures", false},
    {11, "--ws", "Enable weighted search", false},
    {12, "--b", "Consult bytecode", true},
    {12, "--bytecode", "Consult bytecode", true}
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
    {4, ".scope", 0, "Opens interactive scope browser"},
    // {5, ...},
    {6, ".doc", 1, "Shows documentation for object."},
    {7, ".about", 0, "Shows information about reasoning system."},
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
    if (str_starts(filepath, "<") && lastc(filepath) == '>') {
        new = laure_alloc(strlen(lib_path) + strlen(filepath));
        strcpy(new, lib_path);
        strcat(new, "/");
        strncat(new, filepath + 1, strlen(filepath) - 2);
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
    // Check for enhanced REPL commands first
    if (laure_repl_is_special_command(line)) {
        if (strcmp(line, ".clear") == 0) {
            laure_repl_clear_screen();
            return 1;
        } else if (strcmp(line, ".history") == 0) {
            printf("\n%sCommand History:%s\n", REPL_INFO_COLOR, REPL_RESET_COLOR);
            int history_len = history_length;
            for (int i = 1; i <= history_len; i++) {
                HIST_ENTRY *entry = history_get(i);
                if (entry && entry->line) {
                    printf("%s%3d%s %s\n", REPL_HINT_COLOR, i, REPL_RESET_COLOR, entry->line);
                }
            }
            printf("\n");
            return 1;
        } else if (strcmp(line, ".timing") == 0) {
            laure_repl_toggle_feature("timing");
            return 1;
        } else if (strcmp(line, ".theme") == 0) {
            laure_repl_toggle_feature("colors");
            laure_repl_update_prompt(session);
            return 1;
        }
    }

    LAURE_CLOCK = clock();

    while(strlen(line) > 0 && line[0] == ' ')
        line++;

    while(lastc(line) == ';') lastc(line) = 0;
    
    string startline = strdup(line);

    if (line[0] == CMD_CHAR) {
        if (str_eq(line, CMD_EMPTY_STR)) {up; erase; return 1;}
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
                    printf("  %s%s%s: consulted\n", GREEN_COLOR, spath, NO_COLOR);
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
            laure_repl_show_help();
            break;
        }
        case 3: {
            printf("\n%sInterpreter flags:%s\n", REPL_INFO_COLOR, REPL_RESET_COLOR);
            for (int i = 0; i < sizeof(flags) / sizeof(struct laure_flag); i++) {
                struct laure_flag flag = flags[i];
                printf("  %s%s%s - %s%s\n", 
                       REPL_SUCCESS_COLOR, flag.name, REPL_RESET_COLOR, 
                       flag.doc, flag.readword ? " {arg}" : "");
            }
            printf("\n");
            break;
        }
        case 4: {
            laure_scope_interactive(session->scope);
            break;
        }
        case 6: {
            Instance *ins = laure_scope_find_by_key(session->scope, args.argv[1], true);

            if (!ins) {
                printf("\n  %s%s%s is undefined\n\n", REPL_ERROR_COLOR, args.argv[1], REPL_RESET_COLOR);
                laure_free(args.argv);
                return 1;
            }

            string doc = instance_get_doc(ins);

            if (!doc) {
                printf("\n  %sNo documentation available for %s%s%s\n\n", 
                       REPL_HINT_COLOR, REPL_WARNING_COLOR, args.argv[1], REPL_RESET_COLOR);
            } else {
                printf("\n%sDocumentation for %s%s%s:%s\n", 
                       REPL_INFO_COLOR, REPL_SUCCESS_COLOR, args.argv[1], REPL_INFO_COLOR, REPL_RESET_COLOR);
                laure_pprint_doc(doc, 2);
                printf("\n");
            }

            break;
        }
        case 7: {
            printf("\n%slaurelang %s%s", LAURUS_NOBILIS, VERSION, REPL_RESET_COLOR);
            #ifdef GIT_VER
            printf(" (%s%s%s)", REPL_HINT_COLOR, GIT_VER, REPL_RESET_COLOR);
            #endif
            printf("\n");
            
            printf("  %sPath:%s %s\n", REPL_INFO_COLOR, REPL_RESET_COLOR, LAURE_INTERPRETER_PATH);
            printf("  %sLibrary:%s %s\n", REPL_INFO_COLOR, REPL_RESET_COLOR, lib_path);
            printf("  %sBugtracker:%s %s%s%s\n", REPL_INFO_COLOR, REPL_RESET_COLOR, 
                   REPL_SUCCESS_COLOR, BUGTRACKER_URL, REPL_RESET_COLOR);
            
            printf("  %sBuild:%s %s", REPL_INFO_COLOR, REPL_RESET_COLOR,
            #ifdef FEATURE_LINKED_SCOPE
            "linked"
            #else
            "debug"
            #endif
            );
            printf(" (%s %s)\n", __DATE__, __TIME__);
            
            if (LAURE_TIMEOUT != 0)
                printf("  %sTimeout:%s %us\n", REPL_INFO_COLOR, REPL_RESET_COLOR, LAURE_TIMEOUT);
            else 
                printf("  %sTimeout:%s none\n", REPL_INFO_COLOR, REPL_RESET_COLOR);
                
            printf("  %sFlags:%s", REPL_INFO_COLOR, REPL_RESET_COLOR);
            if (FLAG_CLEAN || FLAG_NOREPL || FLAG_QUERY || FLAG_SIGNAL) {
                if (FLAG_CLEAN) printf(" clean");
                if (FLAG_NOREPL) printf(" norepl");
                if (FLAG_QUERY) printf(" query");
                if (FLAG_SIGNAL) printf(" signal");
            } else {
                printf(" none");
            }
            printf("\n\n");
            break;
        }
        case 8: {
            string q = startline + 5;
            laure_parse_result result = laure_parse(q);
            if (!result.is_ok) {
                printf("\n  %sInvalid query:%s %s\n\n", REPL_ERROR_COLOR, REPL_RESET_COLOR, result.err);
                break;
            }
            printf("\n%sAST for '%s%s%s':%s\n", REPL_INFO_COLOR, REPL_SUCCESS_COLOR, q, REPL_INFO_COLOR, REPL_RESET_COLOR);
            laure_expression_show(result.exp, 0);
            printf("\n");
            break;
        }
        case 9: {
            if (args.argc == 1) {
                printf("\n  %sUsage:%s .lock {names}\n\n", REPL_INFO_COLOR, REPL_RESET_COLOR);
            } else {
                printf("\n");
                for (int j = 1; j < args.argc; j++) {
                    string name = args.argv[j];
                    Instance *to_lock = laure_scope_find_by_key(session->scope, name, true);
                    if (! to_lock) {
                        printf("  %s%s%s is undefined\n", REPL_ERROR_COLOR, name, REPL_RESET_COLOR);
                    } else {
                        instance_lock(to_lock);
                        printf("  %s%s%s locked\n", REPL_SUCCESS_COLOR, name, REPL_RESET_COLOR);
                    }
                }
                printf("\n");
            }
            break;
        }
        case 10: {
            if (args.argc == 1) {
                printf("\n  %sUsage:%s .unlock {names}\n\n", REPL_INFO_COLOR, REPL_RESET_COLOR);
            } else {
                printf("\n");
                for (int j = 1; j < args.argc; j++) {
                    string name = args.argv[j];
                    Instance *to_unlock = laure_scope_find_by_key(session->scope, name, true);
                    if (! to_unlock) {
                        printf("  %s%s%s is undefined\n", REPL_ERROR_COLOR, name, REPL_RESET_COLOR);
                    } else {
                        instance_unlock(to_unlock);
                        printf("  %s%s%s unlocked\n", REPL_SUCCESS_COLOR, name, REPL_RESET_COLOR);
                    }
                }
                printf("\n");
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
            if (value == 0)
                printf("\n  %sTimeout disabled%s\n\n", REPL_SUCCESS_COLOR, REPL_RESET_COLOR);
            else
                printf("\n  %sTimeout set to %u seconds%s\n\n", REPL_SUCCESS_COLOR, LAURE_TIMEOUT, REPL_RESET_COLOR);
            break;
        }
        case 12: {
            #ifdef DISABLE_WS
            printf("\n  %sWeighted search unavailable%s (compilation setting)\n\n", REPL_ERROR_COLOR, REPL_RESET_COLOR);
            #else
            if (LAURE_WS) {
                LAURE_WS = false;
                printf("\n  %sWeighted search disabled%s\n\n", REPL_INFO_COLOR, REPL_RESET_COLOR);
            } else {
                LAURE_WS = true;
                printf("\n  %sWeighted search enabled%s\n\n", REPL_SUCCESS_COLOR, REPL_RESET_COLOR);
            }
            #endif
            break;
        }
        case 13: {
            if (LAURE_BACKTRACE && LAURE_BACKTRACE->cursor > 0) {
                printf("\n%sBacktrace:%s\n", REPL_INFO_COLOR, REPL_RESET_COLOR);
                laure_backtrace_print(LAURE_BACKTRACE);
                printf("\n");
            } else {
                printf("\n  %sNo backtrace available%s\n\n", REPL_HINT_COLOR, REPL_RESET_COLOR);
            }
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
        
        // Start timing measurement
        struct timeval start_time, end_time;
        gettimeofday(&start_time, NULL);
        
        qresp response = laure_start(cctx, expset);
        
        // Calculate elapsed time
        gettimeofday(&end_time, NULL);
        double elapsed_time = (end_time.tv_sec - start_time.tv_sec) + 
                             (end_time.tv_usec - start_time.tv_usec) / 1000000.0;

        if (!laure_is_silent(cctx)) {
            if (response.state == q_error) {
                code = 2;
                if (LAURE_BACKTRACE && LAURE_BACKTRACE->cursor > 1) {
                    laure_backtrace_print(LAURE_BACKTRACE);
                }
                
                // Enhanced error display
                if (! LAURE_ACTIVE_ERROR) {
                    printf("  %serror%s: %s\n", RED_COLOR, NO_COLOR, (string)response.payload);
                } else {
                    // Analyze error and create enhanced context
                    laure_repl_error_context_t *error_ctx = laure_repl_analyze_error(LAURE_ACTIVE_ERROR, startline);
                    
                    if (error_ctx) {
                        laure_repl_print_error(startline, error_ctx);
                        laure_repl_free_error_context(error_ctx);
                    }
                    
                    // Also show the original error message
                    char buff[512];
                    laure_error_write(LAURE_ACTIVE_ERROR, buff, 512);
                    printf("%s\n", buff);
                    laure_error_free(LAURE_ACTIVE_ERROR);
                    LAURE_ACTIVE_ERROR = NULL;
                }
            } else if (response.state == q_yield) {
                // Check if we just completed a validation and need to invert semantics
                bool invert_for_validation = cctx->validation_failed;
                
                if (invert_for_validation) {
                    // If validation_failed is true, it means user's completeness claim was wrong
                    // So the overall query statement is false, regardless of response.payload
                    code = 2;
                    printf("  %s%s%s false\n", REPL_WARNING_COLOR, REPL_CROSS_MARK, REPL_RESET_COLOR);
                } else {
                    // Normal mode
                    if (response.payload == (void*)1) {
                        printf("  %s%s%s true\n", REPL_SUCCESS_COLOR, REPL_CHECK_MARK, REPL_RESET_COLOR);
                    } else if (response.payload == (void*)0) {
                        code = 2;
                        printf("  %s%s%s false\n", REPL_WARNING_COLOR, REPL_CROSS_MARK, REPL_RESET_COLOR);
                    }
                }
                
                // Print timing info if enabled
                if (REPL_STATE && REPL_STATE->show_timing) {
                    laure_repl_print_timing_info(elapsed_time);
                    printf("\n");
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


int main(int argc, char *argv[]) {
    signal(SIGINT, sigint_handler);

    if (argc >= 2 && (str_eq(argv[1], "help") || str_eq(argv[1], "--help"))) {
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

    /*

    if (argc >= 2 && str_eq(argv[1], "compile")) {
        laure_session_t comp_session[1];
        comp_session->scope = NULL;
        memset(comp_session->_included_filepaths, 0, included_fp_max * sizeof(void*));
        return laure_compiler_cli(comp_session, argc - 2, argv + 2);
    }

    */

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

    laure_session_t *session = laure_session_new(parameter_repl_mode);
    laure_set_translators();
    laure_init_name_buffs();
    laure_register_builtins(session);

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
            printf("  %s%s%s: consulted\n", GREEN_COLOR, spath, NO_COLOR);
        else
            printf("  %s%s%s: not consulted\n", RED_COLOR, spath, NO_COLOR);
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

    // Initialize enhanced REPL
    repl_init();
    laure_repl_update_prompt(session);
    laure_repl_print_welcome();

    if (setjmp(JUMPBUF)) {
        printf(" %s↳%s true\n", YELLOW_COLOR, NO_COLOR);
    }

    string line;
    while ((line = laure_repl_readline()) != NULL) {
        if (!strlen(line)) {
            erase;
            up;
            continue;
        }
        // Enhanced multiline input handling
        if (laure_repl_should_continue_line(line)) {
            char *full_line = laure_repl_handle_multiline_input(line);
            if (full_line) {
                laure_free(line);
                line = full_line;
            }
        }
        if (lastc(line) == '.') lastc(line) = 0;
        if (lastc(line) == '%') lastc(line) = '\r';
        int res = laure_process_query(session, line);
        if (!res) break;
        laure_free(line);
    }
    
    // Cleanup enhanced REPL
    repl_cleanup();
    
    return 0;
}
