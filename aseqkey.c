/* aseqkey.c -- turn alsa midi notes into x11 key presses
 * 
 * Copyright 2011 Caleb Reach
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <unistd.h>
#include <alsa/asoundlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

#define CHK(stmt, msg) if(stmt) {puts("ERROR: "#msg); exit(1);}
#define ACHK(stmt, msg) CHK((stmt) < 0, msg)

typedef struct {
  unsigned char note;
  unsigned int* keys;
  size_t        key_count;
} note_map;

static note_map*  notes;
static size_t     note_count;
static Display*   display;
static snd_seq_t* seq;

static void handle_event(snd_seq_event_t* ev) {
  Bool press;
  int  i;

  note_map* note = 0;
  for (i=0; i<note_count; i++) {
    if (notes[i].note == ev->data.note.note) {
      note = notes+i;
      break;
    }
  }
  if (!note)
    return;

  switch (ev->type) {
  case SND_SEQ_EVENT_NOTEON:
    if (ev->data.note.velocity) {
      press = True;
      break;
    }

  case SND_SEQ_EVENT_NOTEOFF:
    press = False;
    break;

  default:
    return;
  };

  for (i=0; i<note->key_count; i++)
    XTestFakeKeyEvent(display, note->keys[i], press, 0);

  XFlush(display);
}

static void help(char* name) {
  printf("usage: %s [-d] -p port [-n <note> [-k <key> ] ...] ... \n", name);
  exit(1);
}

int main(int argc, char* argv[]) {
  display = XOpenDisplay(0);
  CHK(!display, "Could not open display");

  int daemonize = 0;
  note_map* note = 0;
  char* port = 0;

  int c;
  while ((c = getopt(argc, argv, "dp:n:k:")) != -1)
    switch (c) {
    case 'd':
      daemonize = 1;
      break;

    case 'p':
      port = optarg;
      break;

    case 'n':
      notes = realloc(notes, sizeof(*notes)*(++note_count));
      note = notes + note_count-1;
      note->note = atoi(optarg);
      note->keys = 0;
      note->key_count = 0;
      break;

    case 'k':
      if (!note) help(*argv);
      KeySym sym = XStringToKeysym(optarg);
      CHK(!sym, "Invalid key");
      note->keys = realloc(note->keys, sizeof(*note->keys)*(++note->key_count));
      note->keys[note->key_count-1] = XKeysymToKeycode(display, sym);
      break;

    default:
      help(*argv);
    }

  if (!port) help(*argv);

  int in_port;
  snd_seq_addr_t addr;

  ACHK(snd_seq_open(&seq, "default", SND_SEQ_OPEN_INPUT, 0),
      "Could not open sequencer");

  ACHK(snd_seq_set_client_name(seq, "midikey"),
      "Could not set client name");
  ACHK(in_port = snd_seq_create_simple_port(seq, "midikey",
                                           SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
                                           SND_SEQ_PORT_TYPE_APPLICATION),
      "Could not open port");
  ACHK(snd_seq_parse_address(seq, &addr, port),
      "Invalid port name");
  ACHK(snd_seq_connect_from(seq, in_port, addr.client, addr.port),
      "Could not connect ports");

  if (daemonize)
    daemon(0,0);

  snd_seq_event_t *ev;
  for (;;) {
    ACHK(snd_seq_event_input(seq, &ev), "Could not read event");
    handle_event(ev);
  }
}
