#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>

char ATSIGN = '@';
int line_number = 1;

typedef struct List
{
    void * data;
    struct List * successor;
} list;

/* a list must be initialized with data */
list * list_new(void * d)
{
    list * l = malloc(sizeof(list));
    l->data = d;
    l->successor = NULL;
    return l;
}

/* we need a double pointer so that if we are passed lst == NULL we mutate *lst
 * so that it points to a new list. This happens in dict_add if the new chunk
 * is hashed into a bucket not already containing a pointer to a list */
void list_push_back(list ** lst, void * elem)
{
    list * a = list_new(elem);
    if (*lst == NULL)
    {
        *lst = a;
    }
    else
    {
        list * l = *lst;
        while (l->successor != NULL) l = l->successor; /* go to the end of l */
        l->successor = a;
    }
}

void list_pop_back(list ** lst)
{
    if (*lst == NULL) return;
    if ((*lst)->successor == NULL)
    {
        free((void *)(*lst));
        *lst = NULL;
        return;
    }
    else
    {
        list * l1 = *lst;
        list * l2 = (*lst)->successor;
        while (l2->successor != NULL) 
        {
            l1 = l2;
            l2 = l2->successor;
        }
        free((void *)l2);
        l1->successor = NULL;
        return;
    }
}

void list_push(list ** lst, void * elem)
{
    list * p = list_new(elem);
    if (*lst == NULL)
    {
        *lst = p;
    }
    else
    {
        list * l = *lst;
        *lst = p;
        p->successor = l;
    }
}

void list_pop(list ** lst)
{
    list * p = *lst;
    if (p == NULL) return;
    list * l = p->successor;
    *lst = l;
    free((void *)p);
}
typedef struct CodeChunk
{
    char * name;
    list * contents;
} code_chunk;
typedef struct ChunkContents
{
    char * string;
    code_chunk * reference;
    int partial_line;
} chunk_contents;
typedef enum ContentType {code, reference} content_t;

content_t contents_type(chunk_contents * c)
{
    if (c->reference != NULL) return reference;
    else return code;
}

chunk_contents * code_contents_new(char * code)
{
    chunk_contents * c = malloc(sizeof(chunk_contents));
    c->string = code;
    c->reference = NULL;
    c->partial_line = 0;
    return c;
}

chunk_contents * reference_contents_new(char * indent, code_chunk * ref)
{
    chunk_contents * c = malloc(sizeof(chunk_contents));
    c->string = indent;
    c->reference = ref;
    c->partial_line = 0;
    return c;
}

code_chunk * code_chunk_new(char * name)
{
    code_chunk * chunk = malloc(sizeof(code_chunk));
    chunk->name     = name;
    chunk->contents = NULL;
    return chunk;
}

/* http://www.cse.yorku.ca/~oz/hash.html */
unsigned long hash(unsigned char *str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++)) hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

typedef struct Dict
{
    list ** array;
    size_t size;
} dict;

dict * dict_new(size_t size)
{
    dict * d = malloc(sizeof(dict));
    d->array = malloc(size * sizeof(list));
    memset(d->array, (size_t)NULL, size * sizeof(list));
    d->size = size;
    return d;
}

void dict_add(dict * d, code_chunk * c)
{
    unsigned long h = hash((unsigned char *)c->name) % d->size;
    list_push_back(&(d->array[h]), (void *) c);
}

code_chunk * dict_get(dict * d, char * name)
{
    unsigned long h = hash((unsigned char *)name) % d->size;
    list * l = d->array[h];
    while (l != NULL)
    {
        code_chunk * c = l->data;
        if (strcmp(name, c->name) == 0) return c;
        else l = l->successor;
    }
    return NULL;
}

void exit_fail_if(int condition, char * message, ...)
{
    if (!condition) return;
    va_list args;
    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);
    exit(EXIT_FAILURE);
}
char * extract_name(char ** source)
{
    char * s = *source;
    char terminus = *s++;
    if (terminus == '{') terminus = '}';
    char * destination = s;

    for (; *s != terminus; ++s)
        exit_fail_if ((*s == '\n' || *s == '\0'), 
                "Error: unterminated name on line %d\n", line_number);
    exit_fail_if (destination == s,
                "Error: empty name on line %d\n",
                line_number);

    *s = '\0';
    *source = s + 1;
    return destination;
}
int advance_to_next_line(char ** source)
{
    char * s = *source;
    while (*s != '\n') if (*s++ == '\0') return 0;
    *source = s + 1;
    ++line_number;
    return 1;
}
void code_chunk_print(FILE * f, dict * d, code_chunk * c, list * indents)
{
    list * l;
    for (l = c->contents; l != NULL; l = l->successor)
    {
        chunk_contents * contents = l->data;
        if (contents_type(contents) == code)
        {
            if (*contents->string != '\0') /* (1) */
            {
                /* print indents on non-empty lines */
                list * i;
                for (i = indents; i != NULL; i = i->successor)
                {
                    fputs((char *)i->data, f);
                }
                fputs(contents->string, f); 
            }


            if (contents->partial_line) /* (2) */
            {
                l = l->successor;
                exit_fail_if(l == NULL, "Error: partial line without successor in chunk '%s':\n    %s", c->name, contents->string);
                contents = l->data;
                fputs(contents->string, f);
            }

            fputc('\n', f); /* (3) */
        }
        else if (contents_type(contents) == reference)
        {
            code_chunk * next_c = contents->reference;
            list_push_back(&indents, (void *)contents->string);
            code_chunk_print(f, d, next_c, indents);
            list_pop_back(&indents);
        }
    }
}

const char * help =
"lilit: the little literate programming tool -- version %s\n\
\n\
    USAGE: %s file\n\
\n\
    lilit extracts machine source code from literate source code.\n\
\n\
All control sequences begin with a special character called ATSIGN, which is \n\
set to '@' by default. Except for escaped ATSIGNs, all control sequences consume\n\
(i.e. cause to be ignored) the remainder of the line following the end of the\n\
control sequence.\n\
\n\
ATSIGN:new control character  Redefines ATSIGN to whatever immediately follows\n\
                              the : sign. This is useful if your machine source\n\
                              language uses lots of @ signs, for example. This\n\
                              control sequence is ignored if it occurs inside\n\
                              a code chunk definition.\n\
\n\
ATSIGN='chunk name'           Begin a regular chunk definition. The chunk name\n\
                              must be surrounded by matching single (') or\n\
                              double (\") quotes (no backticks). The chunk\n\
                              definition itself begins on the next line.\n\
                              Anything preceeding the ATSIGN and following the\n\
                              final quote sign is ignored, allowing this control\n\
                              sequence to be wrapped in typesetting markup.\n\
\n\
ATSIGN#'chunk name'           Begin a tangle chunk definition. This is similar\n\
                              to a regular chunk, except the name of the chunk\n\
                              is also interpreted as a file name, and the chunk\n\
                              is recursively expanded into the file with that\n\
                              name, overwriting any existing file.\n\
\n\
ATSIGN+'chunk name'           Append to a chunk. The code starting on the\n\
                              next line will be added at the end of the chunk\n\
                              named by 'chunk name'. This is useful e.g. for\n\
                              adding #include directives to the top of a c file.\n\
                              If no such chunk already exists, then this control\n\
                              sequence is equivalent to a normal chunk\n\
                              definition (ATSIGN='name').\n\
\n\
ATSIGN{chunk invocation}      Invoke a chunk to be recursively expanded into any\n\
                              tangled output files. Anything preceeding the\n\
                              ATSIGN is considered as indentation, and every\n\
                              line of code in the recursively expanded chunk\n\
                              will have this indent prepended to it. Anything\n\
                              following the closing brace is ignored, however it\n\
                              is recommended not to put anything there for\n\
                              compatibility with possible future extensions that\n\
                              may make use of this space (i.e. text following\n\
                              the closing brace is reserved).\n\
\n\
ATSIGNATSIGN                  Escape sequence. The rest of the line following\n\
                              the initial ATSIGN (including the second ATSIGN)\n\
                              is treated as normal code to copy to output\n\
                              tangled documents. This allows ATSIGN and special\n\
                              control sequences to be passed through to the\n\
                              output documents.\n\
\n\
ATSIGN/                       End an ongoing chunk declaration.\n\
\n\
ATSIGN[anything else]         An ATSIGN followed by any other character is\n\
                              ignored. However, it is recommended to avoid such\n\
                              character sequences for compatibility with future\n\
                              extensions.\n\
\n";

int main(int argc, char ** argv)
{
    char * source;
    dict * d;
    list * tangles = NULL;

    if (argc < 2 || argc > 2 || *argv[1] == '-' /* assume -h */) 
    {
        fprintf(stderr, help, VERSION, argv[0]);
        exit(EXIT_SUCCESS);
    }
    char * filename = argv[1];

    {
        int file_size;
        FILE * source_file = fopen(filename, "r");
        exit_fail_if ((source_file == NULL), 
                "Error: could not open file %s\n", 
                filename);
        
        /* get file size */
        fseek(source_file, 0L, SEEK_END);
        file_size = ftell(source_file);
        rewind(source_file);
        
        /* copy file into memory */
        source = malloc(1 + file_size); /* one added for null termination */
        fread(source, 1, file_size, source_file); 
        fclose(source_file);
        
        source[file_size] = 0;
    }

    d = dict_new(128); /* for storing chunks */
    {
        char * s = source;
        while (*s != '\0')
        {
            if (*s == '\n') ++s, ++line_number;
            else if (*s++ == ATSIGN)
            {
                switch (*s)
                {
                case '#':
                case '=':
                case '+':
                    if (*(s + 1) != '\'' && *(s + 1) != '\"') 
                    {
                        printf("Warning: chunk definition sequence on line %d is missing a quote-delimited name. Ignoring\n", line_number);
                        break;
                    }
                    else
                    {
                        code_chunk * chunk;
                        {
                            /* (1) */
                            int tangle = *s == '#';
                            int append = *s == '+';

                            ++s; /* (2.a) */
                            char * name = extract_name(&s); /* (2.b) */
                            chunk = dict_get(d, name); /* (3) */

                            if (chunk == NULL) /* (4) new chunk definition */
                            {
                                chunk = code_chunk_new(name);
                                dict_add(d, chunk);
                            }
                            else if (!append) /* (5) */
                            {
                                exit_fail_if(chunk->contents != NULL, /* (6) */
                                        "Error: redefinition of chunk '%s' on line %d.\n    Maybe you meant to use a + chunk or accidentally used the same name twice.\n", 
                                        name, line_number);
                                /* todo: free existing chunk? */
                            }
                            if (tangle) list_push(&tangles, (void *)chunk); /* (7) */
                        }

                        exit_fail_if(!advance_to_next_line(&s), /* (8) */
                                "Error: file ended before beginning of definition of chunk '%s' on line '%d'\n", 
                                chunk->name, line_number);
                        {
                            char * start_of_line = s; /* (1) */
                            for (;;)
                            {
                                if (*s == '\n') /* (2.a) */
                                {
                                    chunk_contents * full_line = code_contents_new(start_of_line); /* (1) */
                                    list_push_back(&chunk->contents, (void *)full_line); /* (2) */
                                    ++line_number; /* (3) */
                                    *s++ = '\0'; /* (4) */
                                    start_of_line = s; /* (3.a) */
                                }
                                else if (*s == ATSIGN) /* (2.b) */
                                {
                                    ++s;
                                    if (*s == '/')
                                    {
                                        advance_to_next_line(&s);
                                        break;
                                    }
                                    else if (*s == '{')
                                    {
                                        code_chunk * ref;
                                        char * indent = start_of_line; /* (1.a) */

                                        {
                                            char * end_of_indent = s - 1; /* (1.b) */
                                            *end_of_indent = '\0'; /* (1.c) */
                                        }

                                        {
                                            char * name = extract_name(&s); /* (2.a) */
                                            ref = dict_get(d, name); /* (2.b) */
                                            if (ref == NULL) /* chunk hasn't been defined yet */
                                            {
                                                ref = code_chunk_new(name); /* (2.c) */
                                                dict_add(d, ref);
                                            }
                                            exit_fail_if(!advance_to_next_line(&s), /* (4) */
                                                    "Error: file ended during definition of chunk '%s'\n    following invocation of chunk '%s' on line '%d'\n", 
                                                    chunk->name, name, line_number);
                                        }

                                        list_push_back(&chunk->contents, (void *)reference_contents_new(indent, ref)); /* (3) */
                                    }
                                    else if (*s == ATSIGN)
                                    {
                                        chunk_contents * beginning_part = code_contents_new(start_of_line);
                                        chunk_contents * ending_part = code_contents_new(s);
                                        char * at_the_atsign = s - 1;

                                        exit_fail_if(!advance_to_next_line(&s), 
                                                "Error: file ended during definition of chunk '%s'\n    following the escape sequence on line '%d'\n", 
                                                chunk->name, line_number);

                                        /* (1) */
                                        *at_the_atsign = '\0'; /* terminate beginning part */
                                        *(s - 1) = '\0'; /* terminate end part; s - 1 points to a newline character */

                                        beginning_part->partial_line = 1; /* (2) */

                                        list_push_back(&chunk->contents, (void *)beginning_part);
                                        list_push_back(&chunk->contents, (void *)ending_part);
                                    }
                                    else /* (3.c) */
                                    {
                                        printf("Warning: unrecognized control sequence ATSIGN%c while parsing chunk on line %d\n",
                                                *s, line_number);
                                        continue; /* (3.d) */
                                    }
                                    start_of_line = s; /* (3.b) */
                                }
                                else /* (4) */ 
                                {
                                    exit_fail_if(*s == '\0', "File ended during definition of chunk %s", chunk->name);
                                    ++s;
                                }
                            }
                        }
                    }
                    break;
                case ':':
                    ++s;
                    exit_fail_if (( *s == '=' || *s == '#' || *s == '+' || *s == '{' 
                                 || *s == ':' || *s == '/' || *s == '\n'), 
                            "Error: cannot redefine ATSIGN to a character used in control sequences on line %d\n",
                            line_number);
                    ATSIGN = *s++;
                    break;
                default:
                    printf("Warning: unrecognized control sequence ATSIGN%c while scanning prose on line %d\n",
                            *s, line_number);
                }
            }
        }
    }
    for(; tangles != NULL; tangles = tangles->successor) /* (1) */
    {
        FILE * f;
        code_chunk * c = tangles->data;
        f = fopen(c->name, "w"); /* (2) */
        if (f == NULL)
        {
            fprintf(stderr,
                    "Warning: failed to open file '%s', skipping tangle '%s'\n",
                    c->name, c->name);
            continue;
        }
        code_chunk_print(f, d, c, NULL); /* (3) */
        fclose(f);
    }
    return 0;
}
