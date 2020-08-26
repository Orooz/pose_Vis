#ifndef SOQT_DEVICE_H
#define SOQT_DEVICE_H

// 

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\**************************************************************************/

// *************************************************************************
//
// Toolkit-specific typedef and include(s). Put these before any Coin
// and/or SoQt includes, in case there are any dependency bugs in
// the underlying native toolkit set of include files versus the
// compiler environment's include files.

#include <Inventor/Qt/SoQtBasic.h> // Contains __COIN_SOQT__ define.

#ifdef __COIN_SOQT__
#include <qevent.h>
typedef void SoQtEventHandler(QWidget *, void *, QEvent *, bool *);
#endif // __COIN_SOQT__
#ifdef __COIN_SOXT__
#include <X11/Intrinsic.h>
typedef void SoQtEventHandler(QWidget *, XtPointer, XEvent *, Boolean *);
#endif // __COIN_SOXT__
#ifdef __COIN_SOGTK__
#include <gtk/gtk.h>
typedef gint SoQtEventHandler(QWidget *, QEvent *, gpointer);
#endif // __COIN_SOGTK__
#ifdef __COIN_SOWIN__
#include <windows.h>
typedef LRESULT SoQtEventHandler(QWidget *, UINT, WPARAM, LPARAM);
#endif // __COIN_SOWIN__

// *************************************************************************

#include <Inventor/SbLinear.h>
#include <Inventor/Qt/SoQtObject.h>

class SoEvent;

// *************************************************************************

class SOQT_DLL_API SoQtDevice : public SoQtObject {
  SOQT_OBJECT_ABSTRACT_HEADER(SoQtDevice, SoQtObject);

public:
  virtual ~SoQtDevice();

  virtual void enable(QWidget * w, SoQtEventHandler * handler, void * closure) = 0;
  virtual void disable(QWidget * w, SoQtEventHandler * handler, void * closure) = 0;

  virtual const SoEvent * translateEvent(QEvent * event) = 0;

  void setWindowSize(const SbVec2s size);
  SbVec2s getWindowSize(void) const;

  static void initClasses(void);

protected:
  SoQtDevice(void);

  void setEventPosition(SoEvent * event, int x, int y) const;
  static SbVec2s getLastEventPosition(void);

  void addEventHandler(QWidget *, SoQtEventHandler *, void *);
  void removeEventHandler(QWidget *, SoQtEventHandler *, void *);
  void invokeHandlers(QEvent * event);

private:
  class SoQtDeviceP * pimpl;
  friend class SoQtDeviceP;
};

// *************************************************************************

#endif // !SOQT_DEVICE_H
