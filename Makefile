CFLAGS = '-O2'
LDFLAGS = `pkg-config --libs alsa x11 xtst`

all: aseqkey
