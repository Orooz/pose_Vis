#ifndef COIN_SOGUICOLOREDITOR_H
#define COIN_SOGUICOLOREDITOR_H

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

#include <Inventor/fields/SoSFBool.h>
#include <Inventor/fields/SoSFEnum.h>
#include <Inventor/fields/SoSFColor.h>
#include <Inventor/nodekits/SoBaseKit.h>

class SoGuiColorEditor : public SoBaseKit {
  typedef SoBaseKit inherited;
  SO_KIT_HEADER(SoGuiColorEditor);
  SO_KIT_CATALOG_ENTRY_HEADER(root);

public:
  static void initClass(void);
  SoGuiColorEditor(void);

  enum Sliders {
    NONE,
    INTENSITY,
    RGB,
    HSV,
    RGB_V,
    RGB_HSV
  };

  enum Update {
    CONTINUOUS,
    AFTER_ACCEPT
  };

  SoSFEnum sliders;
  SoSFEnum update;
  SoSFBool wysiwyg;
  SoSFColor color;

protected:
  ~SoGuiColorEditor(void);

private:
  void * internals;

};

#endif // !COIN_SOGUICOLOREDITOR_H
