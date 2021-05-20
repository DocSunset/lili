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

list * list_pop_back(list ** lst)
{
    if (*lst == NULL) return NULL;
    if ((*lst)->successor == NULL)
    {
        free((void *)(*lst));
        *lst = NULL;
        return NULL;
    }
    {
        list * l = (*lst)->successor;
        while (l->successor != NULL) l = l->successor;
        free((void *)l->successor);
        l->successor = NULL;
        return l;
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

typedef enum ContentType {code, reference} content_t;

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

content_t contents_type(chunk_contents * c)
{
    if (c->reference != NULL) return reference; 
    else return code;
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
            if (*contents->string != '\0')
            {
                /* print indents on non-empty lines */
                list * i;
                for (i = indents; i != NULL; i = i->successor)
                {
                    fputs((char *)i->data, f);
                }
            }
            fputs(contents->string, f); 
            if (contents->partial_line == 0) fputc('\n', f);
        }
        else if (contents_type(contents) == reference)
        {
            code_chunk * next_c = contents->reference;
            list_push(&indents, (void *)contents->string);
            code_chunk_print(f, d, next_c, indents);
            list_pop(&indents);
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
                              language uses lots of @ signs, for example.\n\
\n\
ATSIGN='chunk name'           Begin a regular chunk declaration. The chunk name\n\
                              must be surrounded by matching single (') or\n\
                              double (\") quotes (no backticks). The chunk\n\
                              definition itself begins on the next line.\n\
\n\
ATSIGN#'chunk name'           Begin a tangle chunk declaration. This is similar\n\
                              to a regular chunk, except the name of the chunk\n\
                              is also interpreted as a file name, and the chunk\n\
                              is recursively expanded into the file with that\n\
                              name, overwriting any existing file.\n\
\n\
ATSIGN+'chunk name'           Append to a chunk. The code starting on the\n\
                              next line will be added at the end of the chunk\n\
                              named by 'chunk name'. If no such chunk exists,\n\
                              then one will be created. This is useful e.g. for\n\
                              adding #include directives to the top of a c file.\n\
\n\
ATSIGN{chunk invocation}      Invoke a chunk to be recursively expanded into any\n\
                              tangled output files.\n\
\n\
ATSIGNATSIGN                  Escape sequence. A literal ATSIGN sign with no\n\
                              special meaning to lilit that will be copied as an\n\
                              ATSIGN to any output tangled documents.\n\
\n\
ATSIGN/                       End an ongoing chunk declaration.\n\
\n";

int main(int argc, char ** argv)
{
    int file_size;
    char * source;
    dict * d;
    list * tangles = NULL;

    if (argc < 2 || *argv[1] == '-' /* assume -h */) 
    {
        fprintf(stderr, help, VERSION, argv[0]);
        exit(EXIT_SUCCESS);
    }
    char * filename = argv[1];

    {
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
                    {
                        code_chunk * chunk;
                        int done = 0; 

                        if (*(s + 1) != '\'' && *(s + 1) != '\"') /* not a chunk definition */ break;

                        {
                            char type = *s++;
                            char * name = extract_name(&s);
                            int tangle = type == '#';
                            int append = type == '+';
                            
                            chunk = dict_get(d, name);
                            if (chunk == NULL) /* new chunk definition */
                            {
                                chunk = code_chunk_new(name);
                                dict_add(d, chunk);
                            }
                            else if (!append)
                            {
                                exit_fail_if(chunk->contents != NULL, 
                                        "Error: redefinition of chunk '%s' on line %d.\n    Maybe you meant to use a + chunk or accidentally used the same name twice.\n", 
                                        name, line_number);
                                /* todo: free existing chunk? */
                            }
                            if (tangle) list_push(&tangles, (void *)chunk);
                        }

                        exit_fail_if(!advance_to_next_line(&s), 
                                "Error: file ended before beginning of definition of chunk '%s' on line '%d'\n", 
                                chunk->name, line_number);

                        while (!done) 
                        {
                            char * start_of_line = s;
                            while(1) /* process one line */
                            {
                                if (*s == '\n')
                                {
                                    chunk_contents * full_line = code_contents_new(start_of_line);
                                    list_push_back(&chunk->contents, (void *)full_line);
                                    *s++ = '\0';
                                    ++line_number;
                                    break;
                                }
                                else if (*s == ATSIGN)
                                {
                                    ++s;
                                    if (*s == '{')
                                    {
                                        *(s - 1) = '\0'; /* terminate the indent string in place; s - 1 is the ATSIGN */
                                        char * name = extract_name(&s);
                                        code_chunk * ref = dict_get(d, name);
                                        if (ref == NULL) /* chunk hasn't been defined yet */
                                        {
                                            ref = code_chunk_new(name);
                                            dict_add(d, ref);
                                        }
                                        char * indent = start_of_line;
                                        list_push_back(&chunk->contents, (void *)reference_contents_new(indent, ref));
                                        exit_fail_if(!advance_to_next_line(&s), 
                                                "Error: file ended during definition of chunk '%s'\n    following invocation of chunk '%s' on line '%d'\n", 
                                                chunk->name, name, line_number);
                                    }
                                    else if (*s == ATSIGN)
                                    {
                                        chunk_contents * beginning_part = code_contents_new(start_of_line);
                                        chunk_contents * end_part = code_contents_new(s);
                                        char * at_the_atsign = s - 1;

                                        beginning_part->partial_line = 1;
                                        list_push_back(&chunk->contents, (void *)beginning_part);
                                        list_push_back(&chunk->contents, (void *)end_part);

                                        exit_fail_if(!advance_to_next_line(&s), 
                                                "Error: file ended during definition of chunk '%s'\n    following the escape sequence on line '%d'\n", 
                                                chunk->name, line_number);

                                        *at_the_atsign = '\0'; /* terminate beginning part */
                                        *(s - 1) = '\0'; /* terminate end part; s - 1 points to a newline character */
                                    }
                                    else if (*s == '/')
                                    {
                                        done = 1; /* break out of chunk definition loop */
                                        advance_to_next_line(&s);
                                    }
                                    /* else ignore this ATSIGN and carry on */
                                    break;
                                }
                                else exit_fail_if(*s == '\0', "File ended during definition of chunk %s", chunk->name);
                                /* else */ ++s;
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
                    ATSIGN = *s;
                    break;
                }
            }
        }
    }
    for(; tangles != NULL; tangles = tangles->successor)
    {
        FILE * f;
        code_chunk * c = tangles->data;
        f = fopen(c->name, "w");
        if (f == NULL)
        {
            fprintf(stderr,
                    "Warning: failed to open file '%s', skipping tangle '%s'\n",
                    c->name, c->name);
            continue;
        }
        code_chunk_print(f, d, c, NULL);
        fclose(f);
    }
    return 0;
}
