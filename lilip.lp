# lilip: the little literate program transpiler

Inspired by the many formidable "*web" literate programming tools, `lilip` aims
to provide the absolute bare minimum of functionality needed to enable writing
programs in this style.  Specifically, `lilip` will parse literate source code
marked up with simple control sequences in order to extract embedded machine
source code (tangle) and interpolate cross reference links into the literate
source (weave).  `lilip` knows nothing about the machine source code language,
has no runtime configuration, and makes as few assumptions as possible about
the typesetting markup used for prose passages.  By modifying a few lines of
`lilip`s literate source code, the program can be easily adapted to use any
typesetting markup language for the generated cross references. [smu
markdown][github.com/Gottox/smu] is produced by default.

`lilip`s control markup consists of just eight control sequences:

@:~

~=help text
const char * help =
"Control sequences: \
\
Control sequences are permitted to appear at any point in the source file, \
except for flags which must appear inside of a code chunk.\
\
\
@:new control character       Redefines the control character to search for from\
                              @ to whatever immediately follows the : sign.\
                              This is useful if your machine source has lots of\
                              @ signs.\
\
@=chunk name                  Begin a chunk declaration. The chunk name is set\
                              to the remainder of the line after the = excluding\
                              leading and trailing white space.\
\
@-FLAG                        Set the flag named FLAG (see below).\
\
@                             End a chunk declaration. The @ must be immediately\
                              followed by a newline character or the end of the \
                              file without intervening white space.\
\
@{chunk invocation}           Paste in a chunk (tangle) or link to it (weave).\
\
@*{chunk invocation}          As above, but paste the contents of the chunk\
                              while weaving as well as linking.\
\
@(shell command expansion)    Expand a shell command into the file and continue\
                              processing the expanded file recursively.\
\
@@                            Escape sequence. A literal @ sign with no special\
                              meaning to lilip that will be copied as an @ to\
                              any output tangled or woven documents.\
\
Flags:\
\
These can appear during a code chunk definition to set options regarding its\
presentation in the weave, and how it should be treated during tangling.\
\
\
-t filename                   Indicate this is an outline of a file to tangle.\
                              The code in this chunk and all its children will\
                              be output to the given file, overwriting it if it\
                              already exists. \
\
-n                            Suppresses inclusion of this chunk in the weave.\
                              This is overridden if the chunk is @*{invoked}\
                              with a dereference star.\
\
-l LANG                       Indicates the language for syntax highlighting\
                              if different from the parent chunk. It should be\
                              explicitly specified for any top level chunks to\
                              tangle.\n\
";
~

~=overview
~-t lilip.l.c
~{globals}
~{includes}
~{forward declarations}
~{data types}
~{functions}

int main(char ** argv, int argc)
{
    int tangle, weave;
    char * source;
    char * buffer;
    int file_size;
    dict * d;
    list * tangles;
~{help text}

    ~{parse command line arguments}

    if (! (tangle || weave)) return 0; /* nothing to do! */

    ~{load file into `char * source`}

    buffer = malloc(file_size + 1); /* for temporary storage */
    memset(buffer, 0, file_size + 1);

    d = dict_new(128); /* for storing chunks */

    ~{extract code chunks}

    if (tangle)
    {
        ~{output code chunks recursively}
    }

    if (weave)
    {
        ~{extract cross references}
        ~{echo source with cross references added}
    }

}
~

The approach flows as follows:
- If not currently building a code chunk, scan until you find a definition.
- When a code chunk definition begins, and make a new code chunk to hold it.
- Parse any special flags within the code chunk.
- Otherwise, copy the input to the buffer until the code chunk ends, then
  duplicate the contents of the buffer to the code chunk.
- Repeat until you reach the end of the file.

~=extract code chunks
{
    char * b = buffer;
    char * s = source;
    code_chunk * current_chunk = NULL;
    
    while (s != source + file_size)
    {
        ~{scan to the next control sequence}
    
        switch (*s++) /* the character after the ATSIGN determines the command */
        {
        ~{extraction case: control string change}
        ~{extraction case: new code chunk definition}
        ~{extraction case: code chunk flag}
        ~{extraction case: end of code chunk}
        ~{extraction case: chunk invocation}
        /* shell command expansions were handled earlier */
        default:
            ~{extraction case: escape sequence}
            fprintf(stderr, 
                    "Error: Unrecognized control sequence 'ATSIGN%c' on line %d\n", 
                    *(s - 1), line_number);
            exit(1);
        }
        
    }
}
~

~=scan to the next control sequence
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
~

The chunk name, and the language after a -l flag are expected to occupy the
remainder of a line, which is copied to the respective fields of the
`current_chunk` by calling this function:

~=duplicate line
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
~

A similar function is used for flags that take no arguments, in order to skip
to the next line for processing:

~=ignore remainder of line
void ignore_remainder_of_line(char ** s)
{
    char * selection_start = *s;
    char * selection_end = strchr(selection_start, '\n');
    *s += selection_end - selection_start + 1; /* + 1 for \n */
    line_number += 1;
}
~

~=extract invocation name
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
~

~=extraction case: control string change
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
~

~=extraction case: new code chunk definition
case '=':
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
    break;
~

~=extraction case: code chunk flag
case '-':
    switch (*s++)
    {
    case 't': /* tangle this */
        current_chunk->tangle = 1;
        current_chunk->filename = duplicate_line_and_increment(&s);
        break;
    case 'n': /* no weave */
        current_chunk->weave = 0;
        ignore_remainder_of_line(&s);
        break;
    case 'l': /* language */
        if (weave) /* language is only useful to set syntax highlight in weave*/
        {
            current_chunk->language = duplicate_line_and_increment(&s);
            if (current_chunk->language == NULL)
            {
                fprintf(stderr,
                        "Error: expected code chunk language on line %d.\n",
                        line_number);
                exit(1);
            }
        }
        else ignore_remainder_of_line(&s);
        break; 
    default:
        fprintf(stderr,
                "Error: unknown flag '-%c' on line %d\n",
                *(s - 1), line_number);
        exit(1);
    }
    break;
~

~=extraction case: end of code chunk
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
~

~=extraction case: chunk invocation
case '*':
case '{':
    if (current_chunk == NULL) break; /* invocation in prose ignored while extracting */
    else 
    {
        b += sprintf(b, &CTRL);
        if (*(s - 1) == '*') 
        {
            b += sprintf(b, "*");
            ++s;
        }
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
~

~=extraction case: escape sequence
if (*(s - 1) == ATSIGN)
{
    if (current_chunk == NULL) break; /* ignore escape in prose */
    b += sprintf(b, &ATSIGN);
    break;
}
~

~=echo source with cross references added
//fwrite(source, 1, file_size, stdout);
~

~=load file into `char * source`
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
~

~=includes
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
~

~=forward declarations
~

The ***_new functions enforce a few invariants: a `code_chunk` must always have
a name, and a `list` must always have a `code_chunk`.

~=data types
typedef struct List list;

typedef struct CodeChunk
{
    char * name;
    char * contents;
    int tangle;
    char * filename;
    int weave;
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
    chunk->weave    = 1;
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

/* http://www.cse.yorku.ca/~~oz/hash.html */
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

~

~=parse command line arguments
tangle = 1;
weave = 1;
const char * filename = "lilip.lp";
~

~=functions
~{ignore remainder of line}
~{duplicate line}
~{extract invocation name}
~{code chunk print}
~

~=output code chunks recursively
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
~

~=code chunk print
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
~

~=extract cross references
~

~=globals
char ATSIGN = 64; /* ascii for at sign */
const char CTRL = 7; /* ascii ^G, aka BEL */
/* Since the control sequence leader character can be changed with 'ATSIGN:',
 * it's necessary to swap all the ATSIGNs that are found while extracting to
 * some stable character to prevent bugs when tangling and weaving. It is
 * assumed that ascii 7 (^G, BEL) will never occur in any source code file,
 * so we can transliterate ATSIGNs to 7 without having to worry about escapes.
 * If anyone ever tries to run `lilip` on a file with actual ascii 7 embedded in
 * the text, it will undoubtedly cause errors. If this ever happens, the
 * solution would be to detect actual ascii 7 while extracting and embed some
 * kind of escape sequence.
 */
int line_number = 0;
~
