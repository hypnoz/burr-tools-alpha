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
/*
 * The rod picker and the stacking-valid strip of the stacking Puzzle tab.
 * Built in mainwindow_layout.cpp and updated by the other mainwindow files.
 */
#ifndef __RODBARS_H__
#define __RODBARS_H__

#include "Layouter.h"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Slider.H>
#include <FL/fl_draw.H>

#include <string>

/* Full-width coloured strip that states whether the stacking on the Puzzle
 * tab obeys its rod set. The reason wraps, so the strip asks for as many
 * lines as the text needs at its current width. When a resize changes that
 * line count it fires its callback, so the owner can lay the tab out again. */
class StackValidBar_c : public Fl_Box, public layoutable_c {
  std::string text;
  int askedHeight;
  static void cb_relayout(void * v) {
    static_cast<StackValidBar_c*>(v)->do_callback();
  }
  int heightFor(int W) const {
    fl_font(labelfont(), labelsize());
    int tw = W - 12;
    if (tw < 40)
      tw = 40;
    int th = 0;
    fl_measure(text.c_str(), tw, th, 0);
    return th + 8;
  }
public:
  StackValidBar_c(int gx, int gy, int gw, int gh)
    : Fl_Box(0, 0, 100, 20), layoutable_c(gx, gy, gw, gh), askedHeight(0) {
    box(FL_FLAT_BOX);
    labelcolor(FL_WHITE);
    labelfont(FL_HELVETICA_BOLD);
    align(FL_ALIGN_INSIDE | FL_ALIGN_WRAP | FL_ALIGN_LEFT);
    setState(true, "");
  }
  ~StackValidBar_c(void) { Fl::remove_timeout(cb_relayout, this); }
  /* True when the text changed. */
  bool setState(bool valid, const std::string & reason) {
    std::string t = valid ? "Valid" : "Invalid";
    if (!reason.empty())
      t += " - " + reason;
    Fl_Color c = valid ? fl_rgb_color(46, 139, 87) : fl_rgb_color(192, 40, 40);
    if (t == text && c == color())
      return false;
    text = t;
    label(text.c_str());
    color(c);
    redraw();
    return true;
  }
  virtual void getMinSize(int *width, int *height) const {
    *width = 40;
    *height = heightFor(w() > 40 ? w() : 200);
  }
  virtual void draw(void) {
    draw_box();
    draw_label(x() + 6, y(), w() - 12, h(), align());
  }
  virtual void resize(int X, int Y, int W, int H) {
    Fl_Box::resize(X, Y, W, H);
    int need = heightFor(W);
    if (need != H && need != askedHeight) {
      askedHeight = need;
      Fl::remove_timeout(cb_relayout, this);
      Fl::add_timeout(0, cb_relayout, this);
    } else if (need == H) {
      askedHeight = need;
    }
  }
};

/* Tick marks beside the brick editor's Z slider, turned to run under a
 * horizontal slider. LineSpacer's horizontal branch draws with its axes
 * swapped, so this is a corrected copy of the vertical case. */
class RodTickBar_c : public Fl_Widget {
  int lines;
public:
  RodTickBar_c(int x, int y, int w, int h) : Fl_Widget(x, y, w, h), lines(3) {
    color(FL_BACKGROUND_COLOR);
  }
  void setLines(int n) {
    if (n < 1)
      n = 1;
    lines = n;
    redraw();
  }
  void draw(void) {
    fl_color(color());
    fl_rectf(x(), y(), w(), h());
    if (lines <= 1)
      return;
    fl_color(FL_BLACK);
    int gap = 4;
    int span = w() - 2 * gap - 1;
    if (span < 1)
      span = 1;
    /* Same tick width as the Entities Z slider. */
    int thick = span / (lines - 1) / 2;
    if (thick > 3)
      thick = 3;
    if (thick < 1)
      thick = 1;
    for (int i = 0; i < lines; i++) {
      int xpos = x() + gap + span * i / (lines - 1);
      fl_rectf(xpos - thick / 2, y(), thick, h());
    }
  }
};

/* The Entities Z control is an Fl_Slider, 15px on its short side, trough
 * color 237, with a 5px tick strip beside it. This is that slider with
 * FL_HOR_SLIDER and the ticks underneath, shortened for a "Rod: N" label. */
class RodIndexBar_c : public Fl_Group, public layoutable_c {
  Fl_Slider * slider;
  RodTickBar_c * ticks;
  Fl_Box * caption;
  int rods;
  int labelW;
public:
  /* With a pocket column the first place on the slider is the pocket,
   * which stands left of rod 1 on the board. */
  bool pocketFirst = false;
private:
  void syncCaption(void) {
    char buf[16];
    if (pocketFirst && value() == 0)
      snprintf(buf, sizeof(buf), "Rod: P");
    else
      snprintf(buf, sizeof(buf), "Rod: %d", value() + (pocketFirst ? 0 : 1));
    caption->copy_label(buf);
  }
  static void cb_slider(Fl_Widget *, void * v) {
    static_cast<RodIndexBar_c*>(v)->syncCaption();
    static_cast<RodIndexBar_c*>(v)->do_callback();
  }
public:
  RodIndexBar_c(int gx, int gy, int gw, int gh)
    : Fl_Group(0, 0, 120, 20), layoutable_c(gx, gy, gw, gh), rods(3) {
    box(FL_NO_BOX);
    fl_font(FL_HELVETICA, FL_NORMAL_SIZE);
    int tw = 0, th = 0;
    fl_measure("Rod: 16", tw, th, 0);
    labelW = tw + 8;
    caption = new Fl_Box(0, 0, labelW, 15);
    caption->box(FL_NO_BOX);
    caption->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    caption->copy_label("Rod: 1");
    slider = new Fl_Slider(labelW, 0, 40, 15);
    slider->type(FL_HOR_SLIDER);
    slider->color((Fl_Color)237);
    slider->selection_color(FL_WHITE);
    slider->step(1);
    slider->bounds(0, 2);
    slider->value(0);
    slider->clear_visible_focus();
    slider->callback(cb_slider, this);
    squareKnob();
    ticks = new RodTickBar_c(labelW, 15, 40, 5);
    end();
    resizable(nullptr);
    stretchVCenter();
    setMinimumSize(labelW + 40, 20);
  }
  /* Knob length is a fraction of the track. Match the slider's height so
   * the button stays square, as on the Entities Z slider. */
  void squareKnob(void) {
    if (slider->w() < 1)
      return;
    double frac = (double)slider->h() / (double)slider->w();
    if (frac > 1)
      frac = 1;
    slider->slider_size(frac);
  }
  int value(void) const { return (int)(slider->value() + 0.5); }
  void value(int v) {
    int n = rods < 1 ? 1 : rods;
    if (v < 0)
      v = 0;
    if (v >= n)
      v = n - 1;
    slider->value(v);
    syncCaption();
  }
  void setCount(int n) {
    if (n < 1)
      n = 1;
    int keep = value();
    rods = n;
    slider->bounds(0, n - 1);
    if (keep >= n)
      keep = n - 1;
    if (keep < 0)
      keep = 0;
    slider->value(keep);
    ticks->setLines(n);
    syncCaption();
  }
  virtual void getMinSize(int *width, int *height) const {
    *width = labelW + 40;
    *height = 20;
  }
  virtual void resize(int X, int Y, int W, int H) {
    Fl_Widget::resize(X, Y, W, H);
    int sh = 15;
    if (sh > H)
      sh = H;
    int lw = labelW;
    int gap = 4;
    if (lw + gap > W - 20)
      lw = W - 20 - gap;
    if (lw < 1)
      lw = 1;
    caption->resize(X, Y, lw, sh);
    int sx = X + lw + gap;
    int sw = W - (lw + gap);
    if (sw < 1)
      sw = 1;
    slider->resize(sx, Y, sw, sh);
    squareKnob();
    int th = H - sh;
    if (th < 0)
      th = 0;
    ticks->resize(sx, Y + sh, sw, th);
  }
};

#endif
