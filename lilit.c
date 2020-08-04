#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
char ATSIGN = 64; /* ascii for at sign */
const char CTRL = 7; /* ascii ^G, aka BEL */
/* Since the control sequence leader character can be changed with 'ATSIGN:',
 * it's necessary to swap all the ATSIGNs that are found while extracting to
 * some stable character to prevent bugs when tangling and weaving. It is
 * assumed that ascii 7 (^G, BEL) will never occur in any source code file,
 * so we can transliterate ATSIGNs to 7 without having to worry about escapes.
 * If anyone ever tries to run `lilit` on a file with actual ascii 7 embedded in
 * the text, it will undoubtedly cause errors. If this ever happens, the
 * solution would be to detect actual ascii 7 while extracting and embed some
 * kind of escape sequence.
 */
int line_number = 0;
typedef struct List list;

typedef struct CodeChunk
{
    char * name;
    char * contents;
    int tangle;
    char * filename;
    char * language;
    struct CodeChunk * parent;
    list * children;
} code_chunk;


code_chunk * code_chunk_new(char * name)
{
    code_chunk * chunk = malloc(sizeof(code_chunk));
    chunk->name     = name;
    chunk->contents = NULL;
    chunk->tangle   = 0;
    chunk->filename = NULL;
    chunk->language = NULL;
    chunk->parent   = NULL;
    chunk->children = NULL;
    return chunk;
}

/*
int code_chunk_is_ref(const code_chunk * c)
{
    if (c->contents == NULL) return 1;
    else return 0;
}
*/

void code_chunk_free(code_chunk ** c)
{
    free((*c)->name);
    free((*c)->contents);
    free((*c)->language);
    free((*c));
    *c = NULL;
}

struct List
{
    code_chunk * chunk;
    struct List * successor;
};

list * list_new(code_chunk * c)
{
    list * l = malloc(sizeof(list));
    l->chunk = c;
    l->successor = NULL;
}

void list_append(list * l, list * addend)
{
    if (l == NULL)
    {
        fprintf(stderr, "Error: attempt to list append to NULL on line %d\n", line_number);
        exit(1);
    }
    while (l->successor != NULL) l = l->successor; /* go to the end of l */
    l->successor = addend;
}

/* http://www.cse.yorku.ca/~oz/hash.html */
unsigned long hash(unsigned char *str)
{
    unsigned long hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

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
    list * l = list_new(c);
    if (d->array[h] == NULL) d->array[h] = l;
    else list_append(d->array[h], l);
}

code_chunk * dict_get(dict * d, char * name)
{
    unsigned long h = hash(name) % d->size;
    list * l = d->array[h];
    while (l != NULL)
    {
        if (strcmp(name, l->chunk->name) == 0) return l->chunk;
        else l = l->successor;
    }
    return NULL;
}

void ignore_remainder_of_line(char ** s)
{
    char * selection_start = *s;
    char * selection_end = strchr(selection_start, '\n');
    *s += selection_end - selection_start + 1; /* + 1 for \n */
    line_number += 1;
}
char * duplicate_line_and_increment(char ** s)
{
    char * selection_start = *s;
    char * destination;
    char * selection_end;

    while (isspace(*selection_start)) ++selection_start; 
    selection_end = strchr(selection_start, '\n');
    while (isspace(*selection_end)) --selection_end; 
    ++selection_end; /* point one past the last non-whitespace character */

    if (selection_end - selection_start == 0) return NULL;

    *selection_end = '\0';
    destination = malloc(strlen(selection_start) + 1);
    strcpy(destination, selection_start);
    *selection_end = '\n';

    *s = selection_end + 1; /* s points after newline */
    line_number += 1;

    return destination;
}
char * extract_invocation_name(char * s)
{
    char * selection_start = s;
    char * destination;
    char * selection_end = strchr(selection_start, '}');
    if (selection_end == NULL)
    {
        fprintf(stderr,
                "Error: unterminated chunk invocation on line %d\n",
                line_number);
        exit(1);
    }
    if (selection_end - selection_start == 0) 
    {
        fprintf(stderr,
                "Error: empty chunk invocation on line %d\n",
                line_number);
        exit(1);
    }
    *selection_end = '\0';
    destination = malloc(strlen(selection_start) + 1);
    strcpy(destination, selection_start);
    *selection_end = '}';
    return destination;
}
void code_chunk_print(FILE * f, dict * d, code_chunk * c, char * indent, int indented)
{
    char * s = c->contents;
    /* https://stackoverflow.com/questions/17983005/c-how-to-read-a-string-line-by-line */
    while(s && *s)
    {
        if (!indented) fprintf(f, "%s", indent);
        char * invocation;
        char * next_newline_char = strchr(s, '\n');
        if (next_newline_char) *next_newline_char = '\0';  /* terminate the current line */

        invocation = strchr(s, CTRL);
        while (invocation != NULL)
        {
            code_chunk * next_c;
            char * name;
            char tmp;
            char * indent_end;
            char * next_indent;

            /* print whatever is before the invocation */
            *invocation = '\0';
            fprintf(f, "%s", s);
            *invocation = CTRL;

            /* check if the CTRL character is from an escape sequence */
            if (!(*(invocation + 1) == '{' || *(invocation + 1) == '*'))
            {
                ++s;
                invocation = strchr(s, CTRL);
                continue;
            }

            /* build the next indent */
            indent_end = s; while (isspace(*indent_end)) ++indent_end;
            tmp = *indent_end;
            *indent_end = '\0';
            next_indent = malloc(strlen(s) + strlen(indent) + 1);
            strcpy(next_indent, s); 
            strcat(next_indent, indent);
            *indent_end = tmp;

            /* print the invocation itself */
            invocation = strchr(invocation, '{') + 1; 
            name = extract_invocation_name(invocation);
            next_c = dict_get(d, name);
            if (next_c == NULL)
            {
                fprintf(stderr,
                        "Warning: invocation of chunk '%s' could not be found\n",
                        name);
            }
            else code_chunk_print(f, d, next_c, next_indent, 1);

            s = strchr(s, '}') + 1;
            invocation = strchr(s, CTRL);
        }
        /* print a whole line with no invocation, or the remainder of the line
         * following any number of invocations */
        fprintf(f, "%s", s); 

        if (next_newline_char) 
        {
            *next_newline_char = '\n';  /* restore newline-char */
            fprintf(f, "\n");
            indented = 0;
        }
        s = next_newline_char ? (next_newline_char+1) : NULL;
    }
}
const char * help =
"Control sequences: \n\
\n\
Control sequences are permitted to appear at any point in the source file, \n\
except for flags which must appear inside of a code chunk.\n\
\n\
\n\
@:new control character       Redefines the control character to search for from\n\
                              @ to whatever immediately follows the : sign.\n\
                              This is useful if your machine source has lots of\n\
                              @ signs.\n\
\n\
@=chunk name                  Begin a regular chunk declaration.\n\
\n\
@#chunk name                  Begin a tangle chunk declaration. This is similar\n\
                              to a regular chunk, except the name of the chunk\n\
                              is also interpreted as a file name, and the chunk\n\
                              is recursively expanded into the file with that\n\
                              name, overwriting any existing file.\n\
\n\
@                             End a chunk declaration. The @ must be immediately\n\
                              followed by a newline character or the end of the \n\
                              file without intervening white space.\n\
\n\
@{chunk invocation}           Invoke a chunk to be recursively expanded into any\n\
                              tangled output files.\n\
\n\
@@                            Escape sequence. A literal @ sign with no special\n\
                              meaning to lilit that will be copied as an @ to\n\
                              any output tangled or woven documents.\n\
\n\
";

int main(int argc, char ** argv)
{
    int file_size;
    char * source;
    char * buffer;
    dict * d;
    list * tangles;
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
    buffer = malloc(file_size + 1); /* for temporary storage */
    memset(buffer, 0, file_size + 1);
    d = dict_new(128); /* for storing chunks */
    {
        char * b = buffer;
        char * s = source;
        code_chunk * current_chunk = NULL;
        
        while (s != source + file_size)
        {
            if (*s != ATSIGN)
            {
                char * next_control_char = strchr(s, ATSIGN);
                char * next_newline_char = strchr(s, '\n'); 
                while(next_newline_char < next_control_char)
                {
                    ++line_number;
                    next_newline_char = strchr(next_newline_char + 1, '\n'); 
                }
                
                if (current_chunk != NULL)
                {
                    if (next_control_char == NULL)
                    {
                        fprintf(stderr, 
                                "Error: Expected terminating ATSIGN before end of file\n");
                        exit(1);
                    }
                    strncpy(b, s, next_control_char - s);
                    b += next_control_char - s;
                }
                
                if (next_control_char == NULL) break;
                s = next_control_char + 1; 
            }
            else ++s;
            /* `s` points at the next character after ATSIGN */
            /* ATSIGNs are converted to ^G so that we can find them again later in case 
             * ATSIGN is changed to some other character */
            *(s - 1) = CTRL; 
        
            switch (*s++) /* the character after the ATSIGN determines the command */
            {
            case ':':
                if  (  *s == ':' 
                    || *s == '=' 
                    || *s == '-' 
                    || *s == '\n' 
                    || *s == '{' 
                    || *s == '*'
                    )
                {
                    fprintf(stderr,
                            "Error: cannot redefine ATSIGN to a character used in control sequences on line %d\n",
                            line_number);
                    exit(1);
                }
                ATSIGN = *s;
                ignore_remainder_of_line(&s);
                break;
            case '=':
            case '#':
                if (current_chunk != NULL)
                {
                    fprintf(stderr, 
                            "Error: code chunk defined inside code chunk on line %d.\n",
                            line_number);
                    exit(1);
                }
            
                current_chunk = code_chunk_new(duplicate_line_and_increment(&s));
            
                if (current_chunk->name == NULL)
                {
                    fprintf(stderr,
                            "Error: expected code chunk name on line %d.\n",
                            line_number);
                    exit(1);
                }
            
                if (*(s - 1) == '#') current_chunk->tangle = 1;
                break;
            case '\n':
            case '\0':
                /* copy buffer to current_chunk->contents */
                if (*(b - 1) == '\n') *(b - 1) = '\0';
                current_chunk->contents = malloc(b - buffer);
                strncpy(current_chunk->contents, buffer, b - buffer);
            
                dict_add(d, current_chunk);
                if (current_chunk->tangle) 
                {
                    list * l = list_new(current_chunk);
                    if (tangles == NULL) tangles = l;
                    else list_append(tangles, l);
                }
                current_chunk = NULL;
            
                /* reset buffer */
                memset(buffer, 0, b - buffer);
                b = buffer;
                break;
            case '{':
                if (current_chunk == NULL) break; /* invocation in prose ignored while extracting */
                else 
                {
                    b += sprintf(b, "{");
            
                    /* invocation in code chunk added to chunk->children while extracting */
                    list * l;
            
                    char * name = extract_invocation_name(s);
                    if (name == NULL)
                    {
                        fprintf(stderr,
                                "Error: expected chunk name in invocation on line %d\n",
                                line_number);
                        exit(1);
                    }
            
                    l = list_new(code_chunk_new(name));
                    if (current_chunk->children == NULL) current_chunk->children = l;
                    else list_append(current_chunk->children, l);
                } 
                break;
            default:
                if (*(s - 1) == ATSIGN)
                {
                    if (current_chunk == NULL) break; /* ignore escape in prose */
                    b += sprintf(b, &ATSIGN);
                    break;
                }
                fprintf(stderr, 
                        "Error: Unrecognized control sequence '%c%c' on line %d\n", 
                        ATSIGN, *(s - 1), line_number);
                exit(1);
            }
            
        }
    }
    for(; tangles != NULL; tangles = tangles->successor)
    {
        FILE * f;
        code_chunk * c = tangles->chunk;
        f = fopen(c->filename, "w");
        if (f == NULL)
        {
            fprintf(stderr,
                    "Warning: failed to open file '%s', skipping tangle '%s'\n",
                    c->filename, c->name);
            continue;
        }
        code_chunk_print(f, d, c, "", 0);
        fclose(f);
    }
}