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
#include "btmessage.h"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Multiline_Output.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

namespace {

/* The message: read-only, but it can be selected and copied. Return goes
 * to the default button, and Ctrl-C with nothing selected copies it all. */
class messageText_c : public Fl_Multiline_Output {
public:
  messageText_c(int x, int y, int w, int h) : Fl_Multiline_Output(x, y, w, h) {}

  int handle(int e) override {
    if (e == FL_KEYBOARD || e == FL_SHORTCUT) {
      const int key = Fl::event_key();
      if (key == FL_Enter || key == FL_KP_Enter)
        return 0;
      if (key == 'c' && (Fl::event_state() & (FL_COMMAND | FL_CTRL)) && insert_position() == mark()) {
        Fl::copy(value(), size(), 1);
        return 1;
      }
      if (e == FL_SHORTCUT)
        return 0;
    }
    return Fl_Multiline_Output::handle(e);
  }
};

struct dialog_c {
  int result = 0;
};

void buttonCb(Fl_Widget * w, void * v) {
  static_cast<dialog_c *>(w->window()->user_data())->result = (int)(fl_intptr_t)v;
  w->window()->hide();
}

void windowCb(Fl_Widget * w, void *) {
  static_cast<dialog_c *>(w->user_data())->result = 0;
  w->hide();
}

std::string format(const char * fmt, va_list ap) {
  va_list copy;
  va_copy(copy, ap);
  const int n = vsnprintf(nullptr, 0, fmt, copy);
  va_end(copy);
  if (n <= 0)
    return "";
  std::string s((size_t)n + 1, '\0');
  vsnprintf(&s[0], s.size(), fmt, ap);
  s.resize((size_t)n);
  return s;
}

/* The layout of FLTK's own dialogs (Fl_Message): the icon at the left, the
 * message beside it, the buttons below, b0 on the right. */
int show(const std::string & text, const char * icon, const char * b0, const char * b1, const char * b2) {
  Fl::pushed(0);

  Fl_Group * previous = Fl_Group::current();
  Fl_Group::current(nullptr);

  const int ICON = 50;
  const Fl_Fontsize size = fl_message_size_ == -1 ? FL_NORMAL_SIZE : fl_message_size_;

  /* the message, as wide as its longest line, as FLTK's are */
  fl_font(fl_message_font_, size);
  int textW = 0, textH = 0;
  fl_measure(text.c_str(), textW, textH, 0);
  const int PAD_W = 14, PAD_H = 10;
  int messageW = std::max(textW + PAD_W, 340);
  int messageH = std::max(textH + PAD_H, 30);

  const char * labels[3] = {b0, b1, b2};
  int buttonW[3] = {0, 0, 0};
  int buttonH = 25;
  fl_font(FL_HELVETICA, FL_NORMAL_SIZE);
  for (int i = 0; i < 3; i++)
    if (labels[i]) {
      int w = 0, h = 0;
      fl_measure(labels[i], w, h);
      buttonW[i] = w + 30 + (i == 1 ? 20 : 0);
      buttonH = std::max(buttonH, h + 10);
    }
  const int buttonsW = buttonW[0] + buttonW[1] + buttonW[2] - 10;
  int maxW = std::max(messageW + 10 + ICON, buttonsW);
  /* buttons that reach under the icon go below it */
  if (buttonsW > messageW && messageH < ICON)
    messageH = ICON;
  messageW = maxW - 10 - ICON;
  const int W = maxW + 20;
  const int H = buttonH + 30 + messageH;

  dialog_c dialog;
  Fl_Window * win = new Fl_Window(W, H);
  win->user_data(&dialog);
  win->callback(windowCb, &dialog);

  const Fl_Widget * tmpl = fl_message_icon();
  Fl_Box * iconBox = new Fl_Box(10, 10, ICON, ICON, icon);
  iconBox->box(tmpl->box());
  iconBox->labelfont(tmpl->labelfont());
  iconBox->labelsize(ICON - 10);
  iconBox->color(tmpl->color());
  iconBox->labelcolor(tmpl->labelcolor());
  iconBox->align(tmpl->align());

  messageText_c * message = new messageText_c(20 + ICON, 10, messageW, messageH);
  message->box(FL_FLAT_BOX);
  message->color(FL_BACKGROUND_COLOR);
  message->textfont(fl_message_font_);
  message->textsize(size);
  message->value(text.c_str());
  message->insert_position(0);

  /* left to right, for Tab */
  Fl_Button * buttons[3] = {nullptr, nullptr, nullptr};
  int x = W;
  for (int i = 0; i < 3; i++)
    if (labels[i]) {
      x -= buttonW[i];
      buttons[i] = i == 1 ? new Fl_Return_Button(x, H - 10 - buttonH, buttonW[i] - 10, buttonH, labels[i])
                          : new Fl_Button(x, H - 10 - buttonH, buttonW[i] - 10, buttonH, labels[i]);
      buttons[i]->align(FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
      buttons[i]->callback(buttonCb, (void *)(fl_intptr_t)i);
    }
  for (int i = 2; i >= 0; i--)
    if (buttons[i])
      win->add(buttons[i]);
  win->end();
  win->set_modal();
  win->size_range(W, H, W, H);

  if (buttons[1])
    buttons[1]->take_focus();
  if (fl_message_hotspot())
    win->hotspot(buttons[0] ? buttons[0] : buttons[1]);
  else
    win->free_position();

  Fl_Window * grab = Fl::grab();
  if (grab)
    Fl::grab(nullptr);
  win->show();
  Fl_Group::current(previous);
  while (win->shown())
    Fl::wait();
  if (grab)
    Fl::grab(grab);

  const int result = dialog.result;
  delete win;
  return result;
}

} // namespace

void bt_message(const char * fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  const std::string text = format(fmt, ap);
  va_end(ap);
  show(text, "i", nullptr, fl_close, nullptr);
}

void bt_alert(const char * fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  const std::string text = format(fmt, ap);
  va_end(ap);
  show(text, "!", nullptr, fl_close, nullptr);
}

int bt_choice(const char * fmt, const char * b0, const char * b1, const char * b2, ...) {
  va_list ap;
  va_start(ap, b2);
  const std::string text = format(fmt, ap);
  va_end(ap);
  return show(text, "?", b0, b1, b2);
}
