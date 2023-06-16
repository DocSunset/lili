VERSION = 0.4.0

# paths
PREFIX = /usr/local

# flags
DEBUGFLAGS = -g -O0 -Wall -Werror -ansi ${INCS} -DVERSION=\"${VERSION}\"
CFLAGS = -Os -Wall -Werror -ansi ${INCS} -DVERSION=\"${VERSION}\"

# compiler
CC = cc
