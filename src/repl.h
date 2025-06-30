#ifndef REPL_H
#define REPL_H

#include "laurelang.h"
#include <readline/readline.h>
#include <readline/history.h>

#ifndef VERSION
#define VERSION "0.1.0"
#endif

// Enhanced REPL configuration
#define REPL_HISTORY_FILE ".laure_history"
#define REPL_MAX_HISTORY 1000
#define REPL_PROMPT_MAX_LEN 256
#define REPL_LINE_MAX_LEN 2048

// Enhanced color scheme for REPL
#define REPL_PROMPT_COLOR "\033[1;36m"     // Bright cyan
#define REPL_SUCCESS_COLOR "\033[1;32m"    // Bright green  
#define REPL_ERROR_COLOR "\033[1;31m"      // Bright red
#define REPL_WARNING_COLOR "\033[1;33m"    // Bright yellow
#define REPL_INFO_COLOR "\033[1;34m"       // Bright blue
#define REPL_HINT_COLOR "\033[0;90m"       // Dark gray
#define REPL_CONTINUATION_COLOR "\033[0;36m" // Cyan
#define REPL_RESET_COLOR "\033[0m"         // Reset

// Unicode characters for enhanced display
#define REPL_ARROW_RIGHT "→"
#define REPL_ARROW_DOWN "↓"
#define REPL_CHECK_MARK "✓"
#define REPL_CROSS_MARK "✗"
#define REPL_WARNING_SIGN "⚠"
#define REPL_INFO_SIGN "ℹ"
#define REPL_PROMPT_SYMBOL "λ"

// REPL state structure
typedef struct {
    char current_prompt[REPL_PROMPT_MAX_LEN];
    char history_file[256];
    bool colors_enabled;
    bool hints_enabled;
    bool multiline_mode;
    int command_count;
    bool show_timing;
    bool show_memory_usage;
} repl_state_t;

// Enhanced error context
typedef struct {
    char *query;
    char *suggestion;
    char *context_line;
    int error_position;
    int error_length;
    int line_number;
    int column_number;
    char *variable_context;
    char *backtrace_summary;
} laure_repl_error_context_t;

// Function declarations
void repl_init(void);
void repl_cleanup(void);
void repl_load_history(void);
void repl_save_history(void);
void repl_setup_completion(void);

// Prompt functions
void laure_repl_set_prompt(const char *prompt);
void laure_repl_update_prompt(laure_session_t *session);
void laure_repl_print_welcome(void);

// Input handling
char* laure_repl_readline(void);
bool laure_repl_should_continue_line(const char *line);
char* laure_repl_handle_multiline_input(const char *initial_line);

// Display functions
void laure_repl_print_result(qresp response, const char *query, double elapsed_time);
void laure_repl_print_error(const char *query, laure_repl_error_context_t *context);
void laure_repl_print_continuation_prompt(int line_number);
void laure_repl_print_timing_info(double elapsed_time);
void laure_repl_print_memory_info(void);

// Command completion
char** laure_repl_command_completion(const char *text, int start, int end);
char* laure_repl_command_generator(const char *text, int state);

// Utility functions
void laure_repl_clear_screen(void);
void laure_repl_show_help(void);
void laure_repl_toggle_feature(const char *feature);
bool laure_repl_is_special_command(const char *line);

// Enhanced error analysis
laure_repl_error_context_t* laure_repl_analyze_error(laure_error *error, const char *query);
void laure_repl_free_error_context(laure_repl_error_context_t *context);
char* laure_repl_generate_suggestion(laure_error *error);
char* laure_repl_extract_backtrace_summary(void);

// Configuration
void repl_enable_colors(bool enable);
void repl_enable_hints(bool enable);
void repl_enable_timing(bool enable);
void repl_enable_memory_info(bool enable);

// Global REPL state
extern repl_state_t *REPL_STATE;

#endif