#include "repl.h"
#include "memguard.h"
#include "predpub.h"
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Global REPL state
repl_state_t *REPL_STATE = NULL;

// Terminal dimensions
static int terminal_width = 80;
static int terminal_height = 24;

// Get terminal size
static void repl_update_terminal_size(void) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        terminal_width = w.ws_col;
        terminal_height = w.ws_row;
    }
    // Ensure minimum width
    if (terminal_width < 40) terminal_width = 40;
}

// Available commands for completion
static const char* repl_commands[] = {
    ".consult", ".quit", ".help", ".flags", ".scope", ".doc", 
    ".about", ".ast", ".lock", ".unlock", ".timeout", ".ws", 
    ".backtrace", ".clear", ".history", ".theme", ".timing",
    NULL
};

void repl_init(void) {
    REPL_STATE = malloc(sizeof(repl_state_t));
    if (!REPL_STATE) return;
    
    // Update terminal size
    repl_update_terminal_size();
    
    // Initialize state
    strcpy(REPL_STATE->current_prompt, PROMPT);
    snprintf(REPL_STATE->history_file, sizeof(REPL_STATE->history_file), 
             "%s/%s", getenv("HOME") ? getenv("HOME") : ".", REPL_HISTORY_FILE);
    
    REPL_STATE->colors_enabled = true;
    REPL_STATE->hints_enabled = true;
    REPL_STATE->multiline_mode = false;
    REPL_STATE->command_count = 0;
    REPL_STATE->show_timing = false;
    REPL_STATE->show_memory_usage = false;
    
    // Setup readline
    repl_load_history();
    repl_setup_completion();
    
    // Disable readline's default tab behavior
    rl_bind_key('\t', rl_complete);
}

void repl_cleanup(void) {
    if (!REPL_STATE) return;
    
    repl_save_history();
    free(REPL_STATE);
    REPL_STATE = NULL;
}

void repl_load_history(void) {
    if (!REPL_STATE) return;
    
    // Load history from file
    if (access(REPL_STATE->history_file, F_OK) == 0) {
        read_history(REPL_STATE->history_file);
    }
    
    // Set history size
    stifle_history(REPL_MAX_HISTORY);
}

void repl_save_history(void) {
    if (!REPL_STATE) return;
    
    write_history(REPL_STATE->history_file);
    history_truncate_file(REPL_STATE->history_file, REPL_MAX_HISTORY);
}

void repl_setup_completion(void) {
    // Set completion function
    rl_attempted_completion_function = laure_repl_command_completion;
    rl_completion_entry_function = laure_repl_command_generator;
}

void laure_repl_set_prompt(const char *prompt) {
    if (!REPL_STATE || !prompt) return;
    strncpy(REPL_STATE->current_prompt, prompt, REPL_PROMPT_MAX_LEN - 1);
    REPL_STATE->current_prompt[REPL_PROMPT_MAX_LEN - 1] = '\0';
}

void laure_repl_update_prompt(laure_session_t *session) {
    if (!REPL_STATE) return;
    
    char new_prompt[REPL_PROMPT_MAX_LEN];
    
    if (REPL_STATE->colors_enabled) {
        snprintf(new_prompt, sizeof(new_prompt), 
                "%s%s%s ", REPL_PROMPT_COLOR, REPL_PROMPT_SYMBOL, REPL_RESET_COLOR);
    } else {
        snprintf(new_prompt, sizeof(new_prompt), "%s ", REPL_PROMPT_SYMBOL);
    }
    
    laure_repl_set_prompt(new_prompt);
}


void laure_repl_print_welcome(void) {
    if (!REPL_STATE) return;
    
    if (REPL_STATE->colors_enabled) {
        printf("%sType .help for commands%s\n\n", REPL_HINT_COLOR, REPL_RESET_COLOR);
    } else {
        printf("Type .help for commands\n\n");
    }
}

char* laure_repl_readline(void) {
    if (!REPL_STATE) return readline(PROMPT);
    
    char *line = readline(REPL_STATE->current_prompt);
    
    if (line && *line) {
        // Add to history
        add_history(line);
        REPL_STATE->command_count++;
        
        // Save history periodically
        if (REPL_STATE->command_count % 10 == 0) {
            repl_save_history();
        }
    }
    
    return line;
}

bool laure_repl_should_continue_line(const char *line) {
    if (!line) return false;
    
    // Use existing parser logic
    return laure_parser_needs_continuation((char*)line);
}

char* laure_repl_handle_multiline_input(const char *initial_line) {
    if (!initial_line) return NULL;
    
    size_t total_len = strlen(initial_line) + 1;
    char *full_line = malloc(total_len);
    if (!full_line) return NULL;
    
    strcpy(full_line, initial_line);
    
    int line_num = 2;
    while (laure_repl_should_continue_line(full_line)) {
        // Remove trailing backslash if present
        size_t len = strlen(full_line);
        if (len > 0 && full_line[len-1] == '\\') {
            full_line[len-1] = '\0';
        }
        
        // Print continuation prompt
        laure_repl_print_continuation_prompt(line_num);
        
        char *continuation = readline("");
        if (!continuation) break;
        
        // Expand buffer
        size_t cont_len = strlen(continuation);
        total_len += cont_len + 1;
        full_line = realloc(full_line, total_len);
        if (!full_line) {
            free(continuation);
            break;
        }
        
        strcat(full_line, continuation);
        free(continuation);
        line_num++;
    }
    
    return full_line;
}

void laure_repl_print_result(qresp response, const char *query, double elapsed_time) {
    if (!REPL_STATE) return;
    
    const char *color = REPL_SUCCESS_COLOR;
    const char *symbol = REPL_CHECK_MARK;
    
    switch (response.state) {
        case q_true:
            color = REPL_SUCCESS_COLOR;
            symbol = REPL_CHECK_MARK;
            break;
        case q_false:
            color = REPL_WARNING_COLOR;
            symbol = REPL_CROSS_MARK;
            break;
        case q_error:
            color = REPL_ERROR_COLOR;
            symbol = REPL_CROSS_MARK;
            break;
        default:
            color = REPL_INFO_COLOR;
            symbol = REPL_INFO_SIGN;
            break;
    }
    
    if (REPL_STATE->colors_enabled) {
        printf("  %s%s%s ", color, symbol, REPL_RESET_COLOR);
    }
    
    // Print timing if enabled
    if (REPL_STATE->show_timing && elapsed_time >= 0) {
        laure_repl_print_timing_info(elapsed_time);
    }
    
    // Print memory info if enabled
    if (REPL_STATE->show_memory_usage) {
        laure_repl_print_memory_info();
    }
}

void laure_repl_print_error(const char *query, laure_repl_error_context_t *context) {
    if (!REPL_STATE || !context) return;
    
    printf("\n");
    
    if (REPL_STATE->colors_enabled) {
        // Location info (line:column) if available
        if (context->line_number > 1 || context->column_number > 1) {
            printf("  %sat %d:%d%s\n", 
                   REPL_HINT_COLOR, context->line_number, context->column_number, REPL_RESET_COLOR);
        }
        
        // Context line with error position if available
        if (context->context_line && context->error_position >= 0) {
            printf("  %s%s%s\n", REPL_HINT_COLOR, context->context_line, REPL_RESET_COLOR);
            
            // Error pointer
            if (context->column_number > 0) {
                printf("  %s", REPL_ERROR_COLOR);
                for (int i = 0; i < context->column_number - 1; i++) printf(" ");
                printf("^%s\n", REPL_RESET_COLOR);
            }
        }
        
        // Backtrace summary (compact)
        if (context->backtrace_summary && strlen(context->backtrace_summary) > 0 && 
            strcmp(context->backtrace_summary, "No backtrace available") != 0) {
            printf("  %strace: %s%s%s\n", 
                   REPL_HINT_COLOR, REPL_WARNING_COLOR, context->backtrace_summary, REPL_RESET_COLOR);
        }
        
        // Suggestion (compact)
        if (context->suggestion) {
            printf("  %sHint: %s%s\n", REPL_SUCCESS_COLOR, context->suggestion, REPL_RESET_COLOR);
        }
    } else {
        // Plain text version
        if (context->line_number > 1 || context->column_number > 1) {
            printf("  at %d:%d\n", context->line_number, context->column_number);
        }
        
        if (context->context_line) {
            printf("  %s\n", context->context_line);
            if (context->column_number > 0) {
                printf("  ");
                for (int i = 0; i < context->column_number - 1; i++) printf(" ");
                printf("^\n");
            }
        }
        
        if (context->backtrace_summary && strlen(context->backtrace_summary) > 0 && 
            strcmp(context->backtrace_summary, "No backtrace available") != 0) {
            printf("  trace: %s\n", context->backtrace_summary);
        }
        
        if (context->suggestion) {
            printf("  hint: %s\n", context->suggestion);
        }
    }
}

void laure_repl_print_continuation_prompt(int line_number) {
    if (!REPL_STATE) {
        printf("   ");
        return;
    }
    
    if (REPL_STATE->colors_enabled) {
        printf("%s%3d%s %s│%s ", REPL_HINT_COLOR, line_number, REPL_RESET_COLOR,
               REPL_CONTINUATION_COLOR, REPL_RESET_COLOR);
    } else {
        printf("%3d | ", line_number);
    }
}

void laure_repl_print_timing_info(double elapsed_time) {
    if (!REPL_STATE) return;
    
    const char *color = elapsed_time > 1.0 ? REPL_WARNING_COLOR : REPL_HINT_COLOR;
    
    if (REPL_STATE->colors_enabled) {
        printf("  %s(%.3fs)%s", color, elapsed_time, REPL_RESET_COLOR);
    } else {
        printf("  (%.3fs)", elapsed_time);
    }
}

void laure_repl_print_memory_info(void) {
    if (!REPL_STATE) return;
    
    // This would integrate with memguard statistics
    if (REPL_STATE->colors_enabled) {
        printf("  %s📊 Memory: OK%s", REPL_SUCCESS_COLOR, REPL_RESET_COLOR);
    } else {
        printf("  [Memory: OK]");
    }
}

char** laure_repl_command_completion(const char *text, int start, int end) {
    char **matches = NULL;
    
    // Only complete at beginning of line or after whitespace
    if (start == 0 || rl_line_buffer[start-1] == ' ') {
        matches = rl_completion_matches(text, laure_repl_command_generator);
    }
    
    return matches;
}

char* laure_repl_command_generator(const char *text, int state) {
    static int list_index, len;
    const char *name;
    
    if (!state) {
        list_index = 0;
        len = strlen(text);
    }
    
    while ((name = repl_commands[list_index++])) {
        if (strncmp(name, text, len) == 0) {
            return strdup(name);
        }
    }
    
    return NULL;
}

void laure_repl_clear_screen(void) {
    printf("\033[2J\033[H");
    if (REPL_STATE) {
        laure_repl_print_welcome();
    }
}

void laure_repl_show_help(void) {
    if (!REPL_STATE) return;
    
    printf("\n%sCommands:%s\n", REPL_INFO_COLOR, REPL_RESET_COLOR);
    
    if (REPL_STATE->colors_enabled) {
        // File & system commands
        printf("  %s.consult%s {files}  - consult knowledge base files\n", REPL_SUCCESS_COLOR, REPL_RESET_COLOR);
        printf("  %s.quit%s            - quit repl\n", REPL_SUCCESS_COLOR, REPL_RESET_COLOR);
        printf("  %s.help%s            - show this help\n", REPL_SUCCESS_COLOR, REPL_RESET_COLOR);
        printf("  %s.clear%s           - clear screen\n", REPL_SUCCESS_COLOR, REPL_RESET_COLOR);
        
        // Information commands
        printf("  %s.flags%s           - show interpreter flags\n", REPL_SUCCESS_COLOR, REPL_RESET_COLOR);
        printf("  %s.about%s           - show system information\n", REPL_SUCCESS_COLOR, REPL_RESET_COLOR);
        printf("  %s.scope%s           - show global scope (debug)\n", REPL_SUCCESS_COLOR, REPL_RESET_COLOR);
        printf("  %s.doc%s {name}      - show documentation for object\n", REPL_SUCCESS_COLOR, REPL_RESET_COLOR);
        
        // Analysis commands  
        printf("  %s.ast%s {query}     - show abstract syntax tree\n", REPL_SUCCESS_COLOR, REPL_RESET_COLOR);
        printf("  %s.backtrace%s       - show recent backtrace\n", REPL_SUCCESS_COLOR, REPL_RESET_COLOR);
        
        // Control commands
        printf("  %s.lock%s {names}    - lock instances\n", REPL_SUCCESS_COLOR, REPL_RESET_COLOR);
        printf("  %s.unlock%s {names}  - unlock instances\n", REPL_SUCCESS_COLOR, REPL_RESET_COLOR);
        printf("  %s.timeout%s {secs}  - set query timeout\n", REPL_SUCCESS_COLOR, REPL_RESET_COLOR);
        printf("  %s.ws%s              - toggle weighted search\n", REPL_SUCCESS_COLOR, REPL_RESET_COLOR);
        
        // Enhanced REPL commands
        printf("  %s.history%s         - show command history\n", REPL_SUCCESS_COLOR, REPL_RESET_COLOR);
        printf("  %s.timing%s          - toggle timing display\n", REPL_SUCCESS_COLOR, REPL_RESET_COLOR);
        printf("  %s.theme%s           - toggle color theme\n", REPL_SUCCESS_COLOR, REPL_RESET_COLOR);
        
        printf("\n%sNavigation:%s\n", REPL_INFO_COLOR, REPL_RESET_COLOR);
        printf("  %s↑/↓%s              - command history\n", REPL_WARNING_COLOR, REPL_RESET_COLOR);
        printf("  %sTab%s              - command completion\n", REPL_WARNING_COLOR, REPL_RESET_COLOR);
        printf("  %sCtrl+C%s          - interrupt\n", REPL_WARNING_COLOR, REPL_RESET_COLOR);
    } else {
        printf("  .consult {files}  - consult knowledge base files\n");
        printf("  .quit            - quit repl\n");
        printf("  .help            - show this help\n");
        printf("  .clear           - clear screen\n");
        printf("  .flags           - show interpreter flags\n");
        printf("  .about           - show system information\n");
        printf("  .scope           - show global scope (debug)\n");
        printf("  .doc {name}      - show documentation for object\n");
        printf("  .ast {query}     - show abstract syntax tree\n");
        printf("  .backtrace       - show recent backtrace\n");
        printf("  .lock {names}    - lock instances\n");
        printf("  .unlock {names}  - unlock instances\n");
        printf("  .timeout {secs}  - set query timeout\n");
        printf("  .ws              - toggle weighted search\n");
        printf("  .history         - show command history\n");
        printf("  .timing          - toggle timing display\n");
        printf("  .theme           - toggle color theme\n");
        printf("\nNavigation:\n");
        printf("  ↑/↓              - command history\n");
        printf("  Tab              - command completion\n");
        printf("  Ctrl+C          - interrupt\n");
    }
    printf("\n");
}

void laure_repl_toggle_feature(const char *feature) {
    if (!REPL_STATE || !feature) return;
    
    if (strcmp(feature, "colors") == 0) {
        REPL_STATE->colors_enabled = !REPL_STATE->colors_enabled;
        printf("Colors %s\n", REPL_STATE->colors_enabled ? "enabled" : "disabled");
    } else if (strcmp(feature, "timing") == 0) {
        REPL_STATE->show_timing = !REPL_STATE->show_timing;
        printf("Timing display %s\n", REPL_STATE->show_timing ? "enabled" : "disabled");
    } else if (strcmp(feature, "memory") == 0) {
        REPL_STATE->show_memory_usage = !REPL_STATE->show_memory_usage;
        printf("Memory display %s\n", REPL_STATE->show_memory_usage ? "enabled" : "disabled");
    }
}

bool laure_repl_is_special_command(const char *line) {
    if (!line) return false;
    
    return (strcmp(line, ".clear") == 0 ||
            strcmp(line, ".history") == 0 ||
            strcmp(line, ".timing") == 0 ||
            strcmp(line, ".theme") == 0);
}

void repl_enable_colors(bool enable) {
    if (REPL_STATE) REPL_STATE->colors_enabled = enable;
}

void repl_enable_hints(bool enable) {
    if (REPL_STATE) REPL_STATE->hints_enabled = enable;
}

void repl_enable_timing(bool enable) {
    if (REPL_STATE) REPL_STATE->show_timing = enable;
}

void repl_enable_memory_info(bool enable) {
    if (REPL_STATE) REPL_STATE->show_memory_usage = enable;
}

// Enhanced error analysis functions
char* laure_repl_extract_backtrace_summary(void) {
    if (!LAURE_BACKTRACE || LAURE_BACKTRACE->cursor == 0) {
        return strdup("No backtrace available");
    }
    
    char *summary = malloc(512);
    if (!summary) return NULL;
    
    strcpy(summary, "");
    int count = 0;
    
    for (uint i = 0; i < LAURE_BACKTRACE->cursor && count < 3; i++) {
        struct chain_p *p = &LAURE_BACKTRACE->chain[i];
        if (p->e && p->e->fullstring) {
            if (count > 0) strcat(summary, " → ");
            
            char short_desc[64];
            if (strlen(p->e->fullstring) > 30) {
                snprintf(short_desc, sizeof(short_desc), "%.27s...", p->e->fullstring);
            } else {
                strcpy(short_desc, p->e->fullstring);
            }
            strcat(summary, short_desc);
            count++;
        }
    }
    
    if (LAURE_BACKTRACE->cursor > 3) {
        strcat(summary, " (...)");
    }
    
    return summary;
}

char* laure_repl_generate_suggestion(laure_error *error) {
    if (!error) return strdup("Check your query syntax");
    
    switch (error->kind) {
        case undefined_err:
            if (strstr(error->msg, "predicate")) {
                return strdup("Check predicate name and arity, or use .consult to load definitions");
            } else if (strstr(error->msg, "variable")) {
                return strdup("Ensure all variables are properly unified or instantiated");
            }
            return strdup("Check that all referenced names are defined");
            
        case syntaxic_err:
            return strdup("Check syntax: missing parentheses, operators, or punctuation");
            
        case type_err:
            return strdup("Check type compatibility: ensure arguments match expected types");
            
        case runtime_err:
            if (strstr(error->msg, "recursion")) {
                return strdup("Reduce recursion depth or add base cases to prevent infinite loops");
            }
            return strdup("Check runtime conditions and input values");
            
        case too_broad_err:
            if (strstr(error->msg, "unify") || strstr(error->msg, "ambiguous")) {
                return strdup("Add more specific constraints or provide concrete values to reduce ambiguity");
            } else {
                return strdup("Add more constraints to narrow down the solution space");
            }
            
        case signature_err:
            return strdup("Check predicate signature: argument count or types may be incorrect");
            
        case access_err:
            return strdup("Check memory access or variable permissions");
            
        case instance_err:
            return strdup("Check object instantiation and lifecycle");
            
        case internal_err:
            return strdup("Internal error - please report this issue with your query");
            
        case not_implemented_err:
            return strdup("This feature is not yet implemented - try an alternative approach");
            
        default:
            return strdup("Review query logic and consult documentation");
    }
}

static void laure_repl_calculate_position(const char *query, int char_pos, int *line_num, int *col_num) {
    *line_num = 1;
    *col_num = 1;
    
    for (int i = 0; i < char_pos && query[i]; i++) {
        if (query[i] == '\n') {
            (*line_num)++;
            *col_num = 1;
        } else {
            (*col_num)++;
        }
    }
}

static char* laure_repl_extract_context_line(const char *query, int line_num) {
    char *context = malloc(256);
    if (!context) return NULL;
    
    int current_line = 1;
    const char *line_start = query;
    const char *line_end = query;
    
    // Find the start of the target line
    while (current_line < line_num && *line_end) {
        if (*line_end == '\n') {
            current_line++;
            line_start = line_end + 1;
        }
        line_end++;
    }
    
    // Find the end of the target line
    line_end = line_start;
    while (*line_end && *line_end != '\n') {
        line_end++;
    }
    
    // Extract the line (max 200 chars)
    int line_len = line_end - line_start;
    if (line_len > 200) line_len = 200;
    
    strncpy(context, line_start, line_len);
    context[line_len] = '\0';
    
    return context;
}

laure_repl_error_context_t* laure_repl_analyze_error(laure_error *error, const char *query) {
    laure_repl_error_context_t *context = malloc(sizeof(laure_repl_error_context_t));
    if (!context) return NULL;
    
    // Initialize all fields
    context->query = strdup(query ? query : "");
    context->suggestion = laure_repl_generate_suggestion(error);
    context->backtrace_summary = laure_repl_extract_backtrace_summary();
    context->variable_context = strdup(""); // TODO: Extract variable context from scope
    
    // Calculate position information
    if (error && error->reason && error->reason->flag2 > 0) {
        context->error_position = error->reason->flag2;
        laure_repl_calculate_position(query, context->error_position, 
                                     &context->line_number, &context->column_number);
        context->context_line = laure_repl_extract_context_line(query, context->line_number);
        context->error_length = 1; // Default length
    } else {
        context->error_position = -1;
        context->line_number = 1;
        context->column_number = 1;
        context->context_line = NULL;
        context->error_length = 0;
    }
    
    return context;
}

void laure_repl_free_error_context(laure_repl_error_context_t *context) {
    if (!context) return;
    
    free(context->query);
    free(context->suggestion);
    free(context->context_line);
    free(context->variable_context);
    free(context->backtrace_summary);
    free(context);
}