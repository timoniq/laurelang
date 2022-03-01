# globals

```c
extern laure_trace       *LAURE_TRACE;
extern laure_session_t   *LAURE_SESSION;
extern char              *LAURE_CURRENT_ADDRESS;
extern char              *LAURE_INTERPRETER_PATH;
extern uint               LAURE_GC_COLLECTED;
extern uint               LAURE_RECURSION_DEPTH;
```

LAURE_TRACE - current trace tree

LAURE_SESSION - current session

LAURE_CURRENT_ADDRESS - runtime address

LAURE_INTERPRETER_PATH

LAURE_GC_COLLECTED - how many instances were garbage collected

LAURE_RECURSION_DEPTH - current recursion depth