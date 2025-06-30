// smart scope for backtracking
// copyright Arseny Kriuchkov, 2022

#include "laurelang.h"
#include "memguard.h"
#include "laureimage.h"
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

uint HEAP_RUNTIME_ID = 0;

#ifdef SCOPE_LINKED

linked_scope_t *laure_scope_insert(
    laure_scope_t *scope,
    Instance *ptr
) {
    linked_scope_t *linked = laure_alloc(sizeof(linked_scope_t));
    linked->next = scope->linked;
    linked->ptr = ptr;
    linked->link = laure_scope_generate_link();
    scope->linked = linked;
    return linked;
}

linked_scope_t *search_glob_or_null(
    laure_scope_t *scope, 
    bool (*checker)(linked_scope_t*, void*),
    void *payload,
    bool search_glob
) {
    if (search_glob && scope != scope->glob) {
        // global instances are neither 
        // backtracked nor changed (exc special cases)
        return laure_scope_find(scope->glob, checker, payload, false, false);
    }
    return NULL;
}

linked_scope_t *laure_scope_find(
    laure_scope_t *scope, 
    bool (*checker)(linked_scope_t*, void*),
    void *payload,
    bool copy,
    bool search_glob
) {
    // trying to find in current pos
    laure_scope_iter(scope, ptr, {
        if (checker(ptr, payload)) {
            return ptr;
        }
    });
    if (scope->tmp) {
        // if this is subscope, trying searching
        // in scope parent;
        linked_scope_t *tmp_ptr = laure_scope_find(
            scope->tmp, 
            checker, payload, 
            false, search_glob
        );

        if (! tmp_ptr) return NULL;

        if (copy) {
            Instance *cp = instance_deepcopy(tmp_ptr->ptr->name, tmp_ptr->ptr);
            // copying in current pos to backtrack
            // further.
            return laure_scope_insert_l(scope, cp, tmp_ptr->link);
        } else {
            return tmp_ptr;
        }
    }
    return search_glob_or_null(scope, checker, payload, search_glob);
}

// find shortcuts

bool linked_chk_key(linked_scope_t *linked, string key) {
    return streq(linked->ptr->name, key);
}

bool linked_chk_link(linked_scope_t *linked, ulong *ptr_link) {
    return linked->link == *ptr_link;
}

Instance *laure_scope_find_by_key(laure_scope_t *scope, string key, bool search_glob) {
    linked_scope_t *l = laure_scope_find(scope, linked_chk_key, key, true, search_glob);
    return l ? l->ptr : NULL;
}

Instance *laure_scope_find_by_key_l(laure_scope_t *scope, string key, ulong *link, bool search_glob) {
    linked_scope_t *l = laure_scope_find(scope, linked_chk_key, key, true, search_glob);
    if (! l) return NULL;
    if (link) *link = l->link;
    return l->ptr;
}

Instance *laure_scope_find_by_link(laure_scope_t *scope, ulong link, bool search_glob) {
    ulong li[1];
    li[0] = link;
    linked_scope_t *l = laure_scope_find(scope, linked_chk_link, li, true, search_glob);
    return l ? l->ptr : NULL;
}

// change shortcuts

struct change_pd {
    union {
        string key;
        ulong link;
    };
    ulong new_link;
};

bool linked_chk_key_change_lid(linked_scope_t *linked, struct change_pd *payload) {
    if (! streq(linked->ptr->name, payload->key)) return false;
    linked->link = payload->new_link;
    return true;
}

bool linked_chk_lid_change_lid(linked_scope_t *linked, struct change_pd *payload) {
    if (linked->link != payload->link) return false;
    linked->link = payload->new_link;
    return true;
}

Instance *laure_scope_change_link_by_key(laure_scope_t *scope, string key, ulong new_link, bool search_glob) {
    struct change_pd pd;
    pd.key = key;
    pd.new_link = new_link;
    return laure_scope_find(scope, linked_chk_key_change_lid, &pd, true, search_glob);
} 

Instance *laure_scope_change_link_by_link(laure_scope_t *scope, ulong link, ulong new_link, bool search_glob) {
    struct change_pd pd;
    pd.link = link;
    pd.new_link = new_link;
    return laure_scope_find(scope, linked_chk_lid_change_lid, &pd, true, search_glob);
}

void add_grab(ulong link, laure_scope_t *from, laure_scope_t *to) {
    Instance *from_ins = laure_scope_find_by_link(from, link, true);
    if (! from_ins) return;
    Instance *to_ins = instance_deepcopy(from_ins->name, from_ins);
    laure_scope_insert_l(to, to_ins, link);
}

// creates a dependant copy
// element will be copied from main scope
// each time the request to find one appears
laure_scope_t *laure_scope_create_copy(control_ctx *cctx, laure_scope_t *scope) {
    laure_scope_t *nscope = laure_alloc(sizeof(laure_scope_t));
    nscope->idx = scope->idx + 1;
    nscope->repeat = scope->repeat;
    nscope->glob = scope->glob;
    nscope->linked = NULL;
    nscope->tmp = scope;
    nscope->nlink = scope->nlink;
    nscope->next = scope->next;
    nscope->owner = scope->owner;
    // grab_linked_iter(cctx, add_grab, scope, nscope);
    return nscope;
}

void laure_scope_show(laure_scope_t *scope) {
    printf("--- scope linked impl ---\n");
    printf("=== ID: %zi ISTMP: %d ===\n", scope->idx, scope->tmp != 0);
    laure_scope_iter(scope, element, {
        string repr = element->ptr->repr(element->ptr);
        printf("%lu: %s%s%s %s\n", element->link, BOLD_WHITE, element->ptr->name, NO_COLOR, repr);
        laure_free(repr);
    });
    printf("---\n");
}

laure_scope_t *laure_scope_create_global() {
    laure_scope_t *scope = laure_alloc(sizeof(laure_scope_t));
    scope->idx = 1;

    if (! LAURE_LINK_ID) {
        scope->nlink = laure_alloc(sizeof(unsigned long));
        *scope->nlink = 1;
        LAURE_LINK_ID = scope->nlink;
    } else
        scope->nlink = LAURE_LINK_ID;
    
    scope->linked = NULL;
    scope->repeat = 0;
    scope->tmp = NULL;
    scope->glob = scope;
    scope->next = NULL;
    scope->owner = NULL;
    return scope;
}

laure_scope_t *laure_scope_new(laure_scope_t *global, laure_scope_t *next) {
    laure_scope_t *scope = laure_alloc(sizeof(laure_scope_t));
    scope->idx = next->idx + 1;
    scope->nlink = global->nlink;
    scope->glob = global;
    scope->linked = NULL;
    scope->tmp = NULL;
    scope->repeat = 0;
    scope->next = next;
    scope->owner = NULL;
    return scope;
}

void laure_scope_free_linked(linked_scope_t *linked) {
    if (! linked) return;
    if (linked->next) laure_scope_free_linked(linked->next);
    if (linked->ptr) {
        image_free(linked->ptr->image);
        laure_free(linked->ptr);
    }
    laure_free(linked);
}

void laure_scope_free(laure_scope_t *scope) {
    if (! scope) return;
    laure_scope_free_linked(scope->linked);
    laure_free(scope);
}

linked_scope_t *laure_scope_insert_l(
    laure_scope_t *scope,
    Instance *ptr,
    ulong link
) {
    if (! scope) return NULL;
    linked_scope_t *linked = laure_alloc(sizeof(linked_scope_t));
    linked->next = scope->linked;
    linked->ptr = ptr;
    linked->link = link;
    scope->linked = linked;
    return linked;
}

#else

laure_cell NONE_CELL = {NULL, 0, 0};

laure_cell laure_scope_insert(
    laure_scope_t *scope,
    Instance *ptr
) {
    if (scope->count == max_cells - 1) {
        printf("limit of %d elements in scope exceeded.\n", max_cells);
        exit(0);
    }
    laure_cell cell;
    cell.ptr = ptr;
    cell.link = laure_scope_generate_link();
    cell.idx = scope->count;
    scope->cells[scope->count++] = cell;
    return cell;
}

laure_cell laure_scope_find(
    laure_scope_t *scope, 
    bool (*checker)(laure_cell, void*),
    void *payload,
    bool copy,
    bool search_glob
) {
    laure_scope_iter(scope, cellptr, {
        if (checker(*cellptr, payload)) {
            return *cellptr;
        }
    });
    if (scope->glob != scope && search_glob) {
        return laure_scope_find(scope->glob, checker, payload, copy, false);
    }
    return NONE_CELL;
}

bool cell_chk_key(laure_cell cell, string key) {
    return str_eq(cell.ptr->name, key);
}

bool cell_chk_link(laure_cell cell, ulong *ptr_link) {
    return cell.link == *ptr_link;
}

Instance *laure_scope_find_by_key(laure_scope_t *scope, string key, bool search_glob) {
    laure_cell cell = laure_scope_find(scope, cell_chk_key, key, false, search_glob);
    if (cell.ptr == NULL) return NULL;
    return cell.ptr;
}

Instance *laure_scope_find_by_key_l(laure_scope_t *scope, string key, ulong *link, bool search_glob) {
    laure_cell cell = laure_scope_find(scope, cell_chk_key, key, false, search_glob);
    if (cell.ptr == NULL) {
        *link = 0;
        return NULL;
    }
    *link = cell.link;
    return cell.ptr;
}

Instance *laure_scope_find_by_link(laure_scope_t *scope, ulong link, bool search_glob) {
    laure_cell cell = laure_scope_find(scope, cell_chk_link, &link, false, search_glob);
    if (cell.ptr == NULL) return NULL;
    return cell.ptr;
}

struct change_pd {
    laure_scope_t *scope;
    union {
        string key;
        ulong link;
    };
    ulong new_link;
};

bool cell_chk_key_change_lid(laure_cell cell, struct change_pd *payload) {
    if (! str_eq(cell.ptr->name, payload->key)) return false;
    payload->scope->cells[cell.idx].link = payload->new_link;
    return true;
}

bool cell_chk_lid_change_lid(laure_cell cell, struct change_pd *payload) {
    if (cell.link != payload->link) return false;
    payload->scope->cells[cell.idx].link = payload->new_link;
    return true;
}

Instance *laure_scope_change_link_by_key(laure_scope_t *scope, string key, ulong new_link, bool search_glob) {
    struct change_pd pd;
    pd.key = key;
    pd.new_link = new_link;
    pd.scope = scope;
    laure_cell cell = laure_scope_find(scope, cell_chk_key_change_lid, &pd, false, search_glob);
    if (cell.link == 0) return NULL;
    return cell.ptr;
}

Instance *laure_scope_change_link_by_link(laure_scope_t *scope, ulong link, ulong new_link, bool search_glob) {
    struct change_pd pd;
    pd.link = link;
    pd.new_link = new_link;
    pd.scope = scope;
    laure_cell cell = laure_scope_find(scope, cell_chk_lid_change_lid, &pd, false, search_glob);
    if (cell.link == 0) return NULL;
    return cell.ptr;
}

// creates a deep copy
laure_scope_t *laure_scope_create_copy(control_ctx *cctx, laure_scope_t *scope) {
    laure_scope_t *nscope = laure_alloc(sizeof(laure_scope_t));
    nscope->next = scope->next;
    nscope->count = scope->count;
    nscope->glob = scope->glob;
    nscope->idx = scope->idx;
    nscope->nlink = scope->nlink;
    nscope->repeat = scope->repeat;
    nscope->owner = scope->owner;
    for (uint idx = 0; idx < scope->count; idx++) {
        nscope->cells[idx].idx = idx;
        nscope->cells[idx].link = scope->cells[idx].link;
        nscope->cells[idx].ptr = instance_deepcopy(scope->cells[idx].ptr->name, scope->cells[idx].ptr);
    }
    return nscope;
}

void laure_scope_show(laure_scope_t *scope) {
    printf("--- scope stodgy impl ---\n");
    printf("=== ID: %u COUNT: %u ===\n", scope->idx, scope->count);
    laure_scope_iter(scope, cellptr, {
        string repr = cellptr->ptr->repr(cellptr->ptr);
        if (! cellptr->ptr->locked)
            printf("%lu: %s%s%s %s\n", cellptr->link, BOLD_WHITE, cellptr->ptr->name, NO_COLOR, repr);
        else
            printf("%lu: %s%s%s%s %s\n", cellptr->link, BOLD_DEC, BLUE_COLOR, cellptr->ptr->name, NO_COLOR, repr);
        laure_free(repr);
    });
    printf("---\n");
}

// Scope item for interactive browser
typedef struct {
    char *name;
    char *type;
    char *signature; 
    char *description;
    char *full_repr;
    bool is_internal;
    Instance *instance;
} scope_item_t;

// Terminal control functions
static struct termios orig_termios;

static void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void restore_terminal() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void get_terminal_size(int *width, int *height) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        *width = w.ws_col;
        *height = w.ws_row;
    } else {
        *width = 100;  // fallback
        *height = 24;  // fallback
    }
}

static void print_wrapped_text(const char *text, const char *prefix, int table_width) {
    if (!text) return;
    
    int max_line_width = table_width - 4; // Account for "│ " and " │"
    int prefix_len = strlen(prefix);
    int content_width = max_line_width - prefix_len;
    
    char *text_copy = strdup(text);
    char *line_start = text_copy;
    
    bool first_line = true;
    while (*line_start) {
        // Find end of current line within width limit
        char *line_end = line_start + content_width;
        bool truncated = false;
        
        if (strlen(line_start) > content_width) {
            // Find last space before width limit to avoid breaking words
            while (line_end > line_start && *line_end != ' ' && *line_end != '\n') {
                line_end--;
            }
            if (line_end == line_start) {
                // No space found, just break at width limit
                line_end = line_start + content_width;
            }
            truncated = true;
        } else {
            line_end = line_start + strlen(line_start);
        }
        
        // Print the line
        char line_buffer[content_width + 1];
        int line_len = line_end - line_start;
        strncpy(line_buffer, line_start, line_len);
        line_buffer[line_len] = '\0';
        
        // Remove trailing spaces and newlines
        while (line_len > 0 && (line_buffer[line_len-1] == ' ' || line_buffer[line_len-1] == '\n')) {
            line_buffer[--line_len] = '\0';
        }
        
        if (first_line) {
            int spaces = max_line_width - prefix_len - strlen(line_buffer);
            printf("│ %s%s%*s │\n", prefix, line_buffer, spaces, "");
            first_line = false;
        } else {
            int spaces = max_line_width - strlen(line_buffer);
            printf("│ %s%*s │\n", line_buffer, spaces, "");
        }
        
        // Move to next line
        if (truncated) {
            line_start = line_end;
            // Skip spaces at beginning of next line
            while (*line_start == ' ') line_start++;
        } else {
            break;
        }
    }
    
    free(text_copy);
}

// Interactive scope browser with real-time search and navigation
void laure_scope_interactive(laure_scope_t *scope) {
    // Collect all scope items
    scope_item_t *items = NULL;
    int total_items = 0;
    int capacity = 100;
    
    items = laure_alloc(sizeof(scope_item_t) * capacity);
    
    laure_scope_iter(scope, cellptr, {
        if (total_items >= capacity) {
            capacity *= 2;
            items = laure_realloc(items, sizeof(scope_item_t) * capacity);
        }
        
        string name = cellptr->ptr->name;
        string repr = cellptr->ptr->repr(cellptr->ptr);
        bool is_internal = str_starts(name, "__") || str_starts(name, "_");
        
        items[total_items].name = strdup(name);
        items[total_items].full_repr = strdup(repr);
        items[total_items].is_internal = is_internal;
        items[total_items].instance = cellptr->ptr;
        
        // Use instance image type for proper classification
        const char *type_str = "name";
        if (cellptr->ptr->image) {
            enum ImageT img_type = IMAGET(cellptr->ptr->image);
            if (img_type == PREDICATE_FACT) {
                type_str = "predicate";
            } else if (img_type == CONSTRAINT_FACT) {
                type_str = "constraint";
            }
        }
        items[total_items].type = strdup(type_str);
        
        // Use full representation as signature
        items[total_items].signature = strdup(repr ? repr : "");
        
        // Use actual documentation if available, otherwise show representation
        // Replace newlines with spaces to prevent display issues
        const char *doc = cellptr->ptr->doc ? cellptr->ptr->doc : repr;
        char *clean_doc = strdup(doc ? doc : "");
        if (clean_doc) {
            for (char *p = clean_doc; *p; p++) {
                if (*p == '\n' || *p == '\r') {
                    *p = ' ';
                }
            }
        }
        items[total_items].description = clean_doc;
        
        total_items++;
        laure_free(repr);
    });
    
    // Get terminal size
    int term_width, term_height;
    get_terminal_size(&term_width, &term_height);
    
    // Interactive browser state
    char search_buffer[256] = "";
    int search_len = 0;
    int selected_index = 0;
    int scroll_offset = 0;
    bool show_internal = false;
    bool running = true;
    
    // Terminal control
    enable_raw_mode();
    printf("\033[?1049h"); // Enter alternative screen buffer
    printf("\033[?25l");   // Hide cursor
    
    while (running) {
        // Clear screen
        printf("\033[2J\033[H");
        
        // Filter items based on search and internal toggle
        int filtered_count = 0;
        int *filtered_indices = laure_alloc(sizeof(int) * total_items);
        
        for (int i = 0; i < total_items; i++) {
            if (!show_internal && items[i].is_internal) continue;
            if (search_len > 0 && !strstr(items[i].name, search_buffer)) continue;
            filtered_indices[filtered_count++] = i;
        }
        
        // Ensure selected index is valid
        if (filtered_count == 0) {
            selected_index = 0;
        } else if (selected_index >= filtered_count) {
            selected_index = filtered_count - 1;
        }
        if (selected_index < 0) {
            selected_index = 0;
        }
        
        // Adjust scroll to keep selection visible  
        int display_height = term_height - 8; // Leave room for header, search, controls
        if (display_height < 5) display_height = 5; // minimum
        if (selected_index < scroll_offset) {
            scroll_offset = selected_index;
        } else if (selected_index >= scroll_offset + display_height) {
            scroll_offset = selected_index - display_height + 1;
        }
        
        // Use correct table width that matches the actual border
        int table_width = 100; // The actual width of the border
        
        printf("┌────────────────────────────────────────────────────────────────────────────────────────────────┐\n");
        
#ifdef SCOPE_LINKED
        int spaces_needed = table_width - 4 - strlen("laurelang Interactive Scope Browser") - strlen("Build: Linked");
        printf("│ %s%slaurelang Interactive Scope Browser%s%*sBuild: %s%sLinked%s │\n", 
               GREEN_COLOR, BOLD_WHITE, NO_COLOR, spaces_needed, "", BLUE_COLOR, BOLD_WHITE, NO_COLOR);
#else
        int spaces_needed = table_width - 4 - strlen("laurelang Interactive Scope Browser") - strlen("Build: Stodgy");
        printf("│ %s%slaurelang Interactive Scope Browser%s%*sBuild: %s%sStodgy%s │\n", 
               GREEN_COLOR, BOLD_WHITE, NO_COLOR, spaces_needed, "", BLUE_COLOR, BOLD_WHITE, NO_COLOR);
#endif
        printf("├────────────────────────────────────────────────────────────────────────────────────────────────┤\n");
        
        // Search bar - right-align the status info
        // Build the status part to calculate its length
        char status_part[100];
        snprintf(status_part, sizeof(status_part), "Total: %d  Showing: %d  Internal: %s",
                 total_items, filtered_count, show_internal ? "ON" : "OFF");
        
        // Calculate space between search field and right-aligned status
        int search_field_len = strlen("Search: ") + strlen(search_buffer);
        int status_len = strlen(status_part);
        int middle_space = table_width - 2 - search_field_len - status_len - 6;
        
        printf("│ Search: %s%s%s%*s%s │\n",
               YELLOW_COLOR, search_buffer, NO_COLOR,
               middle_space, "", status_part);
        
        printf("├─────────────────────┬────────────┬─────────────────────────┬─────────────────────────────────┤\n");
        printf("│ %-19s │ %-10s │ %-23s │ %-31s │\n", 
               "Name", "Type", "Signature", "Description");
        printf("├─────────────────────┼────────────┼─────────────────────────┼─────────────────────────────────┤\n");
        
        // Display filtered items
        for (int i = 0; i < display_height && i + scroll_offset < filtered_count; i++) {
            int item_idx = filtered_indices[i + scroll_offset];
            scope_item_t *item = &items[item_idx];
            
            bool is_selected = (i + scroll_offset == selected_index);
            const char *bg_color = is_selected ? "\033[47m\033[30m" : "";
            const char *reset_color = is_selected ? "\033[0m" : "";
            
            // Colors - only color the type column
            const char *type_color = NO_COLOR;
            
            if (strcmp(item->type, "predicate") == 0) {
                type_color = GREEN_COLOR;
            } else if (strcmp(item->type, "constraint") == 0) {
                type_color = YELLOW_COLOR;
            } else { // "name" type
                type_color = BLUE_COLOR;
            }
            
            // Safe display with snprintf
            char display_name[21], display_type[11], display_sig[25], display_desc[33];
            snprintf(display_name, sizeof(display_name), "%.19s", item->name ? item->name : "");
            snprintf(display_type, sizeof(display_type), "%.10s", item->type ? item->type : "");
            snprintf(display_sig, sizeof(display_sig), "%.23s", item->signature ? item->signature : "");
            snprintf(display_desc, sizeof(display_desc), "%.31s", item->description ? item->description : "");
            
            printf("│%s %-19s%s │ %s%-10s%s%s │ %-23s │ %-31s %s│\n",
                   bg_color, display_name, reset_color,
                   type_color, display_type, NO_COLOR, reset_color,
                   display_sig,
                   display_desc,
                   reset_color);
        }
        
        // Fill remaining lines  
        for (int i = filtered_count - scroll_offset; i < display_height; i++) {
            printf("│                     │            │                         │                                 │\n");
        }
        
        printf("└─────────────────────┴────────────┴─────────────────────────┴─────────────────────────────────┘\n");
        
        // Help
        printf("\n%sControls:%s ↑/↓ Navigate  %sEnter%s View Details  %sTab%s Toggle Internal  %sType%s Search  %sq/ESC%s Quit\n",
               GRAY_COLOR, NO_COLOR, GREEN_COLOR, NO_COLOR, GREEN_COLOR, NO_COLOR, 
               GREEN_COLOR, NO_COLOR, GREEN_COLOR, NO_COLOR);
        
        // Get input
        fflush(stdout);
        
        // Simple input handling (this is a basic implementation)
        int ch = getchar();
        
        if (ch == 'q') { // 'q' to quit
            running = false;
        } else if (ch == '\t') { // Tab key to toggle internal items
            show_internal = !show_internal;
            selected_index = 0;
            scroll_offset = 0;
        } else if (ch == '\n' || ch == '\r') {
            // Show detailed view for selected item
            if (filtered_count > 0 && selected_index < filtered_count) {
                int item_idx = filtered_indices[selected_index];
                scope_item_t *item = &items[item_idx];
                
                printf("\033[2J\033[H");
                printf("┌──────────────────────────────────────────────────────────────────────────────────────────────────┐\n");
                
                // Title: calculate how much space is left after "│ Detailed View: [name] │"
                int title_space = table_width - 2 - strlen(item->name) - 1;
                printf("│ %s%s%s%s%*s│\n", 
                       GREEN_COLOR, BOLD_WHITE, item->name, NO_COLOR, title_space, "");
                
                printf("├──────────────────────────────────────────────────────────────────────────────────────────────────┤\n");
                
                // Name: calculate space left after "│ Name: [name] │"
                int name_space = table_width - 2 - strlen("Name: ") - strlen(item->name) - 1;
                printf("│ %sName:%s %s%*s│\n", BOLD_WHITE, NO_COLOR, item->name, name_space, "");
                
                // Type: calculate space left after "│ Type: [type] │"
                int type_space = table_width - 2 - strlen("Type: ") - strlen(item->type) - 1;
                printf("│ %sType:%s %s%*s│\n", BOLD_WHITE, NO_COLOR, item->type, type_space, "");
                
                // Signature: calculate space left after "│ Signature: [signature] │"
                int sig_space = table_width - 2 - strlen("Signature: ") - strlen(item->signature) - 1;
                printf("│ %sSignature:%s %s%*s│\n", BOLD_WHITE, NO_COLOR, item->signature, sig_space, "");
                
                // Description header: calculate space left after "│ Description: │"
                int desc_header_space = table_width - 2 - strlen("Description:") - 1;
                printf("│ %sDescription:%s%*s│\n", BOLD_WHITE, NO_COLOR, desc_header_space, "");
                
                // Description content: wrap text to fit within borders
                const char *desc = item->description ? item->description : "";
                int max_line_content = table_width - 4; // Leave room for "│ " and " │"
                
                char desc_copy[1000];
                strncpy(desc_copy, desc, sizeof(desc_copy) - 1);
                desc_copy[sizeof(desc_copy) - 1] = '\0';
                
                char *line_start = desc_copy;
                while (strlen(line_start) > 0) {
                    if (strlen(line_start) <= max_line_content) {
                        // Last line - calculate exact spacing needed
                        int content_space = table_width - 2 - strlen(line_start) - 1;
                        printf("│ %s%*s│\n", line_start, content_space, "");
                        break;
                    } else {
                        // Find a good break point within max_line_content
                        int break_point = max_line_content - 1;
                        while (break_point > 0 && line_start[break_point] != ' ') {
                            break_point--;
                        }
                        if (break_point == 0) break_point = max_line_content - 1;
                        
                        char line[break_point + 2];
                        strncpy(line, line_start, break_point + 1);
                        line[break_point + 1] = '\0';
                        
                        int content_space = table_width - 2 - strlen(line) - 1;
                        printf("│ %s%*s│\n", line, content_space, "");
                        
                        line_start += break_point + 1;
                        while (*line_start == ' ') line_start++; // Skip spaces
                    }
                }
                
                // Full Representation header: calculate space left
                int repr_header_space = table_width - 2 - strlen("Full Representation:") - 1;
                printf("│ %sFull Representation:%s%*s│\n", BOLD_WHITE, NO_COLOR, repr_header_space, "");
                
                // Full representation content: calculate exact spacing
                const char *repr = item->full_repr ? item->full_repr : "";
                int repr_content_space = table_width - 2 - strlen(repr) - 1;
                printf("│ %s%*s│\n", repr, repr_content_space, "");
                
                // Empty line: calculate exact spacing
                int empty_space = table_width - 2;
                printf("│%*s│\n", empty_space, "");
                printf("└──────────────────────────────────────────────────────────────────────────────────────────────────┘\n");
                printf("\nPress any key to return...");
                getchar();
            }
        } else if (ch == '\033') { // Arrow keys start with ESC
            int next1 = getchar();
            if (next1 == '[') {
                int next2 = getchar();
                if (next2 == 'A') { // Up arrow
                    if (selected_index > 0) {
                        selected_index--;
                        // Adjust scroll if needed
                        if (selected_index < scroll_offset) {
                            scroll_offset = selected_index;
                        }
                    }
                } else if (next2 == 'B') { // Down arrow
                    if (selected_index < filtered_count - 1) {
                        selected_index++;
                        // Adjust scroll if needed
                        int display_height = 20; // Approximate display height
                        if (selected_index >= scroll_offset + display_height) {
                            scroll_offset = selected_index - display_height + 1;
                        }
                    }
                }
            } else {
                // ESC pressed (quit)
                running = false;
            }
        } else if (ch >= 32 && ch <= 126) { // Printable characters for search
            if (search_len < sizeof(search_buffer) - 1) {
                search_buffer[search_len++] = ch;
                search_buffer[search_len] = '\0';
                selected_index = 0;
                scroll_offset = 0;
            }
        } else if (ch == 127 || ch == 8) { // Backspace
            if (search_len > 0) {
                search_buffer[--search_len] = '\0';
                selected_index = 0;
                scroll_offset = 0;
            }
        }
        
        laure_free(filtered_indices);
    }
    
    // Restore terminal
    printf("\033[?25h");   // Show cursor
    printf("\033[?1049l"); // Exit alternative screen buffer
    restore_terminal();
    printf("\n");
    
    // Cleanup
    for (int i = 0; i < total_items; i++) {
        laure_free(items[i].name);
        laure_free(items[i].type);
        laure_free(items[i].signature);
        laure_free(items[i].description);
        laure_free(items[i].full_repr);
    }
    laure_free(items);
}

laure_scope_t *laure_scope_create_global() {
    laure_scope_t *scope = laure_alloc(sizeof(laure_scope_t));
    scope->idx = 1;
    scope->count = 0;

    if (! LAURE_LINK_ID) {
        scope->nlink = laure_alloc(sizeof(unsigned long));
        *scope->nlink = 1;
        LAURE_LINK_ID = scope->nlink;
    } else
        scope->nlink = LAURE_LINK_ID;

    scope->glob = scope;
    scope->next = NULL;
    scope->repeat = 0;
    return scope;
}

laure_scope_t *laure_scope_new(laure_scope_t *global, laure_scope_t *next) {
    laure_scope_t *scope = laure_alloc(sizeof(laure_scope_t));
    scope->idx = next ? next->idx + 1 : global->idx + 1;
    scope->count = 0;
    scope->glob = global;
    scope->next = next;
    scope->nlink = global->nlink;
    scope->repeat = 0;
    scope->owner = NULL;
    return scope;
}

void laure_scope_free_linked(laure_cell cell) {}

laure_cell laure_scope_insert_l(
    laure_scope_t *scope,
    Instance *ptr,
    ulong link
) {
    if (! scope) return NONE_CELL;
    if (scope->count == max_cells - 1) {
        printf("limit of %d elements in scope exceeded.\n", max_cells);
        exit(0);
    }
    laure_cell cell;
    cell.ptr = ptr;
    cell.link = link;
    cell.idx = scope->count;
    scope->cells[scope->count++] = cell;
    return cell;
}

void laure_scope_free(laure_scope_t *scope) {
    for (uint idx = 0; idx < scope->count; idx++) {
        Instance *instance = scope->cells[idx].ptr;
        image_free(instance->image);
        laure_free(instance);
    }
    laure_free(scope);
}

#endif

ulong laure_scope_generate_link() {
    if (! LAURE_LINK_ID) {
        LAURE_LINK_ID = laure_alloc(sizeof(ulong));
        *LAURE_LINK_ID = (unsigned long)1;
    }
    ulong link = *LAURE_LINK_ID;
    *LAURE_LINK_ID = *LAURE_LINK_ID + 1;
    return link;
}

string laure_scope_generate_unique_name() {
    ulong link = laure_scope_generate_link();
    char buff[16];
    snprintf(buff, 16, "$%lu", link);
    return strdup( buff );
}

string laure_scope_get_owner(laure_scope_t *scope) {
    return scope->owner;
}

Instance *laure_scope_find_var(laure_scope_t *scope, laure_expression_t *var, bool search_glob) {
    if (var->s == NULL && var->flag2) {
        if (var->flag2 >= VAR_LINK_LIMIT) {
            // access from heap
            uint heap_id = VAR_LINK_LIMIT - var->flag2;
            assert(heap_id < ID_MAX);
            return HEAP_TABLE[heap_id];
        }
        return laure_scope_find_by_link(scope, var->flag2, search_glob);
    }
    assert(var->s);
    return laure_scope_find_by_key(scope, var->s, search_glob);
}

ulong laure_set_heap_value(Instance *value, uint id) {
    assert(id < ID_MAX);
    HEAP_TABLE[id] = value;
    return (ulong)(VAR_LINK_LIMIT + id);
}

/*
Instance *laure_search_heap_value_by_name(string name) {
    for (uint i = LAURE_COMPILER_ID_OFFSET; i < ID_MAX; i++) {
        if (! HEAP_TABLE[i]) continue;
        else if (str_eq(HEAP_TABLE[i]->name, name)) return HEAP_TABLE[i];
    }
    return NULL;
}
*/

/*
void laure_heap_add_value(Instance *value) {
    HEAP_TABLE[LAURE_COMPILER_ID_OFFSET + HEAP_RUNTIME_ID++] = value;
}
*/