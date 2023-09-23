VERSION = 0.4.3

# paths
prefix = /usr/local

# flags
DEBUGFLAGS = -g -O0 -Wall -Werror -ansi ${INCS} -DVERSION=\"${VERSION}\"
CFLAGS = -Os -Wall -Werror -ansi ${INCS} -DVERSION=\"${VERSION}\"

# compiler
CC = cc
