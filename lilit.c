#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

char ATSIGN = 64; /* ascii for at sign */
const char CTRL = 7; /* ascii ^G, aka BEL */
int line_number = 0;

typedef struct CodeChunk
{
    char * name;
    char * contents;
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
}

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

/* http://www.cse.yorku.ca/~oz/hash.html */
unsigned long hash(unsigned char *str)
{
    unsigned long hash = 5381;
    int c;

    while (c = *str++) hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

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
}

void dict_add(dict * d, code_chunk * c)
{
    unsigned long h = hash(c->name) % d->size;
    list_append(&(d->array[h]), (void *) c);
}

code_chunk * dict_get(dict * d, char * name)
{
    unsigned long h = hash(name) % d->size;
    list * l = d->array[h];
    while (l != NULL)
    {
        code_chunk * c = l->data;
        if (strcmp(name, c->name) == 0) return c;
        else l = l->successor;
    }
    return NULL;
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
        else if (*s == '\n' || *s == '\0')
        {
            fprintf(stderr,
                    "Error: unterminated name on line %d\n",
                    line_number);
            exit(1);
        }
        ++s;
    }

    if (selection_end == selection_start) 
    {
        fprintf(stderr,
                "Error: empty name on line %d\n",
                line_number);
        exit(1);
    }

    *selection_end = '\0';
    destination = malloc(strlen(selection_start) + 1);
    strcpy(destination, selection_start);
    *selection_end = terminus;
    return destination;
}

void code_chunk_print(FILE * f, dict * d, code_chunk * c, char * indent)
{
    char * s = c->contents;
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
                            "Warning: invocation of chunk '%s' could not be found\n",
                            name);
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
                /* fall through to the outer ++s so s points after the '}' */
            }
        }
        else 
        {
            fputc(*s, f);
            if (*s == '\n') 
            {
                /* we can assume there is always another line after a '\n'
                 * because there is never a '\n' on the final line */
                start_of_line = s + 1;
                fprintf(f, "%s", indent); /* print indent on the new line */
            }
        }
        ++s;
    }
}

const char * help =
"Control sequences: \n\
\n\
Control sequences are permitted to appear at any point in the source file, \n\
except for flags which must appear inside of a code chunk. All control sequences\n\
begin with a special character called ATSIGN, which is set to '@' by default.\n\
\n\
\n\
ATSIGN:new control character  Redefines ATSIGN to whatever immediately follows\n\
                              the : sign. This is useful if your machine source\n\
                              has lots of @ signs, for example.\n\
\n\
ATSIGN='chunk name'           Begin a regular chunk declaration.\n\
\n\
ATSIGN#'chunk name'           Begin a tangle chunk declaration. This is similar\n\
                              to a regular chunk, except the name of the chunk\n\
                              is also interpreted as a file name, and the chunk\n\
                              is recursively expanded into the file with that\n\
                              name, overwriting any existing file.\n\
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
        printf("%s", help);
        exit(0);
    }
    char * filename = argv[1];
    
    {
        FILE * source_file = fopen(filename, "r");
        if (source_file == NULL)
        {
            fprintf(stderr, "Error: could not open file %s\n", filename);
            exit(1);
        }
        
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
                *s = CTRL; /* set ATSIGN to CTRL and increment s */
                ++s;
    
                if (*s == '=' || *s == '#')
                {
                    /* s points to the character after ATSIGN/CTRL on entry */
                    char * final_newline_candidate;
                    int tangle = *s++ == '#'; /* 'ATSIGN#' means tangle */
                    char terminus = *s++; /* chunk name should be 'wrapped' in "matching" *characters* */
                    code_chunk * c = code_chunk_new(extract_name(s, terminus));
                    c->tangle = tangle;
                    
                    while (*s++ != '\n') {} /* scan `s` to one past the end of line */
                    c->contents = s;
                    final_newline_candidate = s - 1;
                    
                    while(1)
                    {
                        if (*s == '\n')
                        {
                            ++line_number;
                            final_newline_candidate = s;
                        }
                        else if (*s == ATSIGN) 
                        {
                            *s = CTRL; /* set ATSIGN to CTRL and increment s */
                            ++s;
                            if (*s == ATSIGN) {} /* escape, skip over the second ATSIGN */
                                                 /* so it won't be turned into a CTRL*/
                            else if (*s == '\n' || *s == '\0')
                            {
                                /* end and record chunk */
                                *final_newline_candidate = '\0'; /* yes, it was the final newline */
                                                                 /* null terminate the contents */
                                dict_add(d, c);
                                if (c->tangle) list_append(&tangles, (void *)c);
                                break;
                            }
                        }
                        else if (*s == '\0')
                        {
                            fprintf(stderr,
                                    "Error: file ended during definition of chunk '%s'\n",
                                    c->name);
                            exit(1);
                        }
                        ++s;
                    }
                }
                else if (*s == ':')
                {
                    ++s;
                    if  (  *s == ':' 
                        || *s == '=' 
                        || *s == '#' 
                        || *s == '\n' 
                        || *s == '{' 
                        )
                    {
                        fprintf(stderr,
                                "Error: cannot redefine ATSIGN to a character used in control sequences on line %d\n",
                                line_number);
                        exit(1);
                    }
                    else ATSIGN = *s;
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
}