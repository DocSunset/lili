#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>

char ATSIGN = 64; /* ascii for at sign */
int line_number = 0;
const char CTRL = 7; /* ascii ^G, aka BEL */

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
void list_append(list ** lst, void * addend)
{
    list * a = list_new(addend);
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

typedef struct CodeChunk
{
    char * name;
    list * contents;
    int tangle;
} code_chunk;

/* every code chunk must have a name */
code_chunk * code_chunk_new(char * name)
{
    code_chunk * chunk = malloc(sizeof(code_chunk));
    chunk->name     = name;
    chunk->contents = NULL;
    chunk->tangle   = 0;
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
    list_append(&(d->array[h]), (void *) c);
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
    fprintf(stderr, message, args);
    va_end(args);
    exit(EXIT_FAILURE);
}

char * extract_name(char * s, char terminus)
{
    char * destination;
    char * selection_start = s;
    char * selection_end;

    while(1)
    {
        if (*s == terminus)
        {
            selection_end = s;
            break;
        }
        else exit_fail_if ((*s == '\n' || *s == '\0'),
                    "Error: unterminated name on line %d\n",
                    line_number);
        ++s;
    }

    exit_fail_if ((selection_end == selection_start),
                "Error: empty name on line %d\n",
                line_number);

    *selection_end = '\0';
    destination = malloc(strlen(selection_start) + 1);
    strcpy(destination, selection_start);
    *selection_end = terminus;
    return destination;
}

void code_chunk_print(FILE * f, dict * d, code_chunk * c, char * indent)
{
    list * l;
    for (l = c->contents; l != NULL; l = l->successor)
    {
        char * s = l->data;
        char * start_of_line = s;
        while (*s != '\0')
        {
            if (*s == CTRL)
            {
                ++s;
                if (*s != '{') fputc(*s, f); /* escaped ATSIGN, print it */
                else /* this must be a code invocation */
                {
                    char * name = extract_name(++s, '}');
                    code_chunk * next_c = dict_get(d, name);

                    if (next_c == NULL)
                    {
                        fprintf(stderr,
                                "Warning: '%s' could not be found in invocation within `%s`\n",
                                name, c->name);
                    }
                    else 
                    {
                        char * next_indent;
                        char tmp;
                        char * indent_end = start_of_line; 
                        while (isspace(*indent_end)) ++indent_end;
                        tmp = *indent_end;
                        *indent_end = '\0'; /* temporarily terminate line at first non-space char */
                        next_indent = malloc(strlen(start_of_line) + strlen(indent) + 1);
                        strcpy(next_indent, indent); /* copy current indent to next_indent */
                        strcat(next_indent, start_of_line); /* append space from current line to next_indent */
                        *indent_end = tmp; /* restore first non-space char */
                
                        code_chunk_print(f, d, next_c, next_indent);
                    }
                    while(*s != '}') ++s; /* scan s to the '}' */
                    /* we don't need to exit_fail_if we find the end of file during
                     * this loop, because extract_name would have already found
                     * this error */
                    
                    /* fall through to the outer ++s so s points after the '}' */

                    /* we should probably free name here */
                }
            }
            else 
            {
                fputc(*s, f);
                if (*s == '\n') 
                {
                    /* we can assume there is always another line after a '\n'
                     * because on the final line the '\n' will have been turned
                     * into a '\0' to terminate the string */
                    start_of_line = s + 1;
                    fprintf(f, "%s", indent); /* print indent on the new line */
                }
            }
            ++s;
        }
        /* if there are more contents to append, separate them with a newline */
        if (l->successor != NULL) fputc('\n', f); 
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
set to '@' by default.\n\
\n\
ATSIGN:new control character  Redefines ATSIGN to whatever immediately follows\n\
                              the : sign. This is useful if your machine source\n\
                              language uses lots of @ signs, for example.\n\
\n\
ATSIGN='chunk name'           Begin a regular chunk declaration. The chunk name\n\
                              must be surrounded by matching delimiter\n\
                              characters, which can be anything. The chunk\n\
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
ATSIGN                        End a chunk declaration. The ATSIGN must be \n\
                              immediately followed by a newline character or the\n\
                              end of the file without intervening white space.\n\
                              The whole line on which the terminating ATSIGN is\n\
                              found is ignored, i.e. not considered part of the\n\
                              chunk definition.\n\
\n\
ATSIGN{chunk invocation}      Invoke a chunk to be recursively expanded into any\n\
                              tangled output files.\n\
\n\
ATSIGNATSIGN                  Escape sequence. A literal ATSIGN sign with no\n\
                              special meaning to lilit that will be copied as an\n\
                              ATSIGN to any output tangled documents.\n\
";

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
            if (*s == '\n')
            {
                ++line_number;
            }
            else if (*s == ATSIGN)
            {
                *s = CTRL; /* set ATSIGN to CTRL s */
                ++s;
    
                if (*s == '=' || *s == '#' || *s == '+')
                {
                    /* s points to the character after ATSIGN/CTRL on entry */
                    char * final_newline_candidate;
                    int tangle = *s == '#'; /* 'ATSIGN#' means tangle */
                    int append = *s++ == '+'; /* 'ATSIGN+' means append */
                    char terminus = *s++; /* chunk name should be 'wrapped' in "matching" *characters* */
                    char * name = extract_name(s, terminus);
                    
                    int new_chunk = 1;
                    code_chunk * c = dict_get(d, name);
                    if (append)
                    {
                        if (c == NULL) c = code_chunk_new(name);
                        else new_chunk = 0; /* should probably free name here */
                    }
                    else 
                    {
                        if (c != NULL) 
                            fprintf(stderr, 
                                "warning, redefinition of chunk %s on line %d\n", 
                                name, line_number);
                        c = code_chunk_new(name);
                    }
                    c->tangle = tangle;
                    
                    while (*s++ != '\n') /* scan `s` to one past the end of line */
                    {
                        exit_fail_if(*s == '\0', 
                                "Error: file ended before beginning the definition of chunk '%s'\n",
                                c->name);
                    }
                    ++line_number;
                    
                    /* s points to the first character of the chunk definition */
                    list_append(&(c->contents), (void *)s);
                    final_newline_candidate = s;
                    while(1)
                    {
                        if (*s == '\n')
                        {
                            ++line_number;
                            final_newline_candidate = s;
                        }
                        else if (*s == ATSIGN) 
                        {
                            *s = CTRL; /* set ATSIGN to CTRL s */
                            ++s;
                            if (*s == ATSIGN) {} /* escape, skip over the second ATSIGN */
                                                 /* so it won't be turned into a CTRL*/
                            else if (*s == '\n' || *s == '\0')
                            {
                                /* end and record chunk */
                                *final_newline_candidate = '\0'; /* yes, it was the final newline */
                                                                 /* null terminate the contents */
                                if (new_chunk) dict_add(d, c);
                                if (c->tangle) list_append(&tangles, (void *)c);
                                break;
                            }
                        }
                        else exit_fail_if ((*s == '\0'),
                                    "Error: file ended during definition of chunk '%s'\n",
                                    c->name);
                        ++s;
                    }
                }
                else if (*s == ':')
                {
                    ++s;
                    exit_fail_if (( *s == ':' || *s == '=' || *s == '#' || *s == '\n' || *s == '{'  || *s == '+'),
                                "Error: cannot redefine ATSIGN to a character used in control sequences on line %d\n",
                                line_number);
                    ATSIGN = *s;
                }
            }
            ++s;
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
        code_chunk_print(f, d, c, "");
        fclose(f);
    }
    return 0;
}