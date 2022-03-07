#include "laurelang.h"
#include "laureimage.h"
#include "predpub.h"
#include <readline/readline.h>
#include <errno.h>
#include <signal.h>

#define string char*
#define up printf("\033[A")
#define down printf("\n")
#define erase printf("\33[2K\r")
#define PATH_MAX 1024

#define str_eq(s1, s2) (s1 != NULL && s2 != NULL && strcmp(s1, s2) == 0)
#define str_starts(s, start) (strncmp(start, s, strlen(start)) == 0)

#define VERSION        "0.1"
#define BUGTRACKER_URL "github.com/timoniq/laurelang"

const struct laure_flag flags[] = {
    // 0
    {1, "-f", "Set main filename", true},
    {2, "-q", "Startup query", true},
    {3, "-norepl", "Don't run REPL after startup", false},
    {4, "--version", "Show version and quit", false},
    {5, "-clean", "Do not load std", false},
    {6, "-signal", "Usage with -q <..>, returns error code if predicate fails", false},
    {7, "--library", "Manually set lib_path", true},
    {8, "-D", "Add dyn flag", true}
};

struct cmd_info {
    int id;
    string name;
    int argc; // -1 is any
    string help;
};

const struct cmd_info commands[] = {
    {0, ".consult", -1, "Consults files. Any number of args, each arg is a filename."},
    {1, ".quit", 0, "Quits laurelang REPL."},
    {2, ".help", 0, "Shows this message."},
    {3, ".flags", 0, "Shows all available flags for laure interpreter."},
    {4, ".stack", 0, "Shows global scope values; needed for debug."},
    {5, ".gc", 0, "Runs garbage collector."},
    {6, ".doc", 1, "Shows documentation for object."},
    {7, ".getinfo", 0, "Shows information about reasoning system."},
    {8, ".ast", -1, "Shows AST of query passed."},
    {9, ".lock", -1, "Locks instances."},
    {10, ".unlock", -1, "Unlocks instances."},
    {11, ".timeout", 1, "Sets timeout (in seconds)"}
};

struct filename_linked {
    string filename;
    struct filename_linked *next;
};

bool   FLAG_NOREPL = false;
bool   FLAG_CLEAN = false;
bool   FLAG_SIGNAL = false;
string FLAG_QUERY = NULL;
string FLAG_LIBRARY = NULL;

string                  INTERPRETER_PATH = NULL;
struct filename_linked *FILENAMES = NULL;
char                   _PATH[PATH_MAX] = {0};
char                    DPROMPT[32] = PROMPT;

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
    
    result = malloc(sizeof(string) * count);

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

string convert_filepath(string filepath) {
    string new;
    if (str_starts(filepath, "@/")) {
        filepath++;
        new = malloc(strlen(lib_path) + strlen(filepath));
        strcpy(new, lib_path);
        strcat(new, filepath);
    } else {
        new = strdup(filepath);
    }
    return new;
}

int laure_process_query(laure_session_t *session, string line) {

    LAURE_CLOCK = clock();

    while(strlen(line) > 0 && line[0] == ' ')
        line++;
    
    string startline = strdup(line);

    if (line[0] == '.') {
        args_parsed args = args_parse(line);
        bool found = false;
        for (int i = 0; i < sizeof(commands) / sizeof(struct cmd_info); i++) {
            struct cmd_info cmd = commands[i];
            if (str_eq(cmd.name, args.argv[0])) {
                found = true;
                if (cmd.argc != args.argc - 1 && cmd.argc != -1) {
                    printf("  Requires %d args (got %d)\n", cmd.argc, args.argc - 1);
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
                FILE *fs = fopen(path, "r");
                if (! fs) {
                    printf("  %sUnable to open `%s`%s\n", RED_COLOR, path, NO_COLOR);
                    free(path);
                    free(args.argv);
                    return 1;
                }

                fclose(fs);
                string full_path = strdup( realpath(path, _PATH) );
                laure_consult_recursive(session, full_path);
                printf("  %s%s%s: consulted\n", GREEN_COLOR, args.argv[j], NO_COLOR);
                free(path);
            }
            break;
        }
        case 1: {
            return 0;
        }
        case 2: {
            printf("help:\n");
            for (int i = 0; i < sizeof(commands) / sizeof(struct cmd_info); i++) {
                struct cmd_info cmd = commands[i];
                printf("  %s - %s\n", cmd.name, cmd.help);
            }
            break;
        }
        case 3: {
            printf("flags:\n");
            for (int i = 0; i < sizeof(flags) / sizeof(struct laure_flag); i++) {
                struct laure_flag flag = flags[i];
                printf("  %s - %s [%s]\n", flag.name, flag.doc, flag.readword ? "takes arg" : "no args");
            }
            printf("  -D {flagname}={value}\n");
            break;
        }
        case 4: {
            laure_stack_show(session->stack);
            break;
        }
        case 5: {
            uint collected = laure_gc_run(session->stack);
            printf("  Destroyed %zi garbage, summary %zi\n", collected, LAURE_GC_COLLECTED);
            break;
        }
        case 6: {
            Instance *ins = laure_stack_get(session->stack, args.argv[1]);

            if (!ins) {
                printf("  sorry, can't find instance named `%s`\n", args.argv[1]);
                free(args.argv);
                return 1;
            }

            string doc = instance_get_doc(ins);

            printf("   ");

            if (!doc) {
                printf("  sorry, no documentation for this instance");
            } else {
                bool color_set = false;
                for (int i = 0; i < strlen(doc); i++) {
                    char c = doc[i];
                    if (c == '\n') {
                        printf("\n   ", GRAY_COLOR, NO_COLOR);
                    } else if (c == '`') {
                        printf("%s", color_set ? NO_COLOR : YELLOW_COLOR);
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
            printf("  laurelang %s\n", VERSION);
            printf("  running `%s`\n", LAURE_INTERPRETER_PATH);
            printf("  bugtracker %s\n", BUGTRACKER_URL);
            printf("  build info:\n");
            printf("    stack build: %s\n", 
            #ifdef FEATURE_LINKED_SCOPE
            "linked (production)"
            #else
            "stodgy (debug)"
            #endif
            );
            printf("    lib_path: `%s`\n", lib_path);
            printf("    trace limit: %d\n", LAURE_TRACE_LIMIT);
            printf("    recursion depth limit: %d\n", LAURE_RECURSION_DEPTH_LIMIT);
            printf("    compiled at: %s %s\n", __DATE__, __TIME__);
            printf("  garbage collected: %d\n", LAURE_GC_COLLECTED);
            if (LAURE_TIMEOUT != 0)
                printf("  timeout: %zis\n", LAURE_TIMEOUT);
            else printf("  timeout: no\n");
            printf("  flags: [ ");
            if (FLAG_CLEAN) printf("CLEAN ");
            if (FLAG_NOREPL) printf("NOREPL ");
            if (FLAG_QUERY) printf("QUERY ");
            if (FLAG_SIGNAL) printf("SIGNAL ");
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
                    Instance *to_lock = laure_stack_get(session->stack, name);
                    if (! to_lock) {
                        printf("  %s %sis undefined%s\n", name, RED_COLOR, NO_COLOR);
                    } else {
                        instance_lock(to_lock);
                        printf("  %s %slocked%s\n", name, GREEN_COLOR, NO_COLOR);
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
                    Instance *to_unlock = laure_stack_get(session->stack, name);
                    if (! to_unlock) {
                        printf("  %s %sis undefined%s\n", name, RED_COLOR, NO_COLOR);
                    } else {
                        instance_unlock(to_unlock);
                        printf("  %s %sunlocked%s\n", name, GREEN_COLOR, NO_COLOR);
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
            printf("  timeout was set to %zis\n", LAURE_TIMEOUT);
            break;
        }
        default: break;
    }
    // ----------
            }
        }
        if (! found) {
            printf("  Unknown command `%s`, use `.help`\n", args.argv[0]);
        }
        free(args.argv);
        return 1;
    }

    int code = 1;
    
    laure_parse_result res = laure_parse(line);
    if (!res.is_ok) {
        printf("  %ssyntax_error%s %s\n", RED_COLOR, NO_COLOR, res.err);
        code = 2;
    } else {

        #ifdef DEBUG
        laure_expression_show(res.exp, 0);
        #endif

        laure_expression_set *expset = laure_expression_compose_one(res.exp);

        control_ctx *cctx = laure_control_ctx_get(session, expset);
        qresp response = laure_eval(cctx, expset);
        // laure_gc_run(session->stack);

        if (!laure_is_silent(cctx)) {
            if (response.state == q_true) {
            printf("  true\n");
            } else if (response.state == q_false) {
                code = 2;
                printf("  false\n");
            } else if (response.state == q_error) {
                code = 2;
                printf("  %serror%s %s\n", RED_COLOR, NO_COLOR, response.error);
            }
        }
        free(cctx);
    }
    return code;
}

void add_filename(string str) {
    struct filename_linked filename = {str, NULL};
    struct filename_linked *ptr = malloc(sizeof(struct filename_linked));
    *ptr = filename;
    if (! FILENAMES)
        FILENAMES = ptr;
    else {
        struct filename_linked *l = FILENAMES;
        while (l->next) {l = l->next;};
        l->next = ptr;
    }
}

int main(int argc, char *argv[]) {

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
                    printf("%s\n", value);
                    value++;
                    if (value[0] == '\"' && lastc(value) == '\"') {
                        value++; lastc(value) = 0;
                    }
                    flagname[strlen(word) - strlen(value) - 1] = 0;
                    printf("[%s] = [%s]\n", flagname, value);
                    add_dflag(flagname, value);
                    break;
                }
            }}}

            if (!found)
                printf("  %sFlag %s is undefined%s\n", RED_COLOR, str, NO_COLOR);
            }
            // ----------
        }
    }

    printf("(laurelang v%s (c) timoniq)\n", VERSION);
    LAURE_INTERPRETER_PATH = INTERPRETER_PATH;

    string prompt = getenv("LLPROMPT");
    if (prompt) strcpy(DPROMPT, prompt);

    string timeout_s = getenv("LLTIMEOUT");
    if (timeout_s) LAURE_TIMEOUT = atoi(timeout_s);

    laure_session_t *session = laure_session_new();
    laure_register_builtins(session);
    LAURE_SESSION = session;

    if (! FLAG_CLEAN) {
        string lp = FLAG_LIBRARY ? FLAG_LIBRARY : lib_path;
        string real_path = realpath(lp, _PATH);
        if (! real_path) {
            printf("Can't load standard lib from `%s`\n", lp);
        } else {
            string path = strdup( real_path );
            laure_consult_recursive(session, path);
        }
    }

    struct filename_linked *filenames = FILENAMES;
    while (filenames) {
        string path = convert_filepath(filenames->filename);
        FILE *fs = fopen(path, "r");
        if (! fs) {
            printf("  %sUnable to open `%s`%s\n", RED_COLOR, path, NO_COLOR);
            if (FLAG_SIGNAL) return SIGABRT;
            filenames = filenames->next;
            continue;
        }
        fclose(fs);

        string full_path = strdup( realpath(path, _PATH) );
        laure_consult_recursive(session, full_path);
        printf("  %s%s%s: consulted\n", GREEN_COLOR, filenames->filename, NO_COLOR);
        free(path);
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

    string line;
    while ((line = readline(DPROMPT)) != NULL) {
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
            free(line);
            
            line = strdup( nline );
            lp = true;
        }
        if (lp) {up; up; erase; printf("%s%s\n", DPROMPT, line); erase;}
        int res = laure_process_query(session, line);
        if (!res) break;
        free(line);
    }

    return 0;
}
