/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef __BTMESSAGE_H__
#define __BTMESSAGE_H__

#if defined(__GNUC__) || defined(__clang__)
#define BT_PRINTF(f, a) __attribute__((format(printf, f, a)))
#else
#define BT_PRINTF(f, a)
#endif

/*
 * fl_message, fl_alert and fl_choice as the program uses them, laid out the
 * same, but with the message in a read-only text field: it can be selected
 * with the mouse and copied with Ctrl-C (Cmd-C on macOS). With nothing
 * selected, Ctrl-C copies the whole message, as FLTK's dialogs do.
 */

/* A message with a Close button. */
void bt_message(const char * fmt, ...) BT_PRINTF(1, 2);

/* A warning or error with a Close button. */
void bt_alert(const char * fmt, ...) BT_PRINTF(1, 2);

/* A question with up to three buttons, b0 on the right; b1 is the default.
 * Returns the button pressed, 0 also for Escape or the close box. */
int bt_choice(const char * fmt, const char * b0, const char * b1, const char * b2, ...)
    BT_PRINTF(1, 5);

#endif
