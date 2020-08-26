#ifndef SOQT_FULLVIEWER_H
#define SOQT_FULLVIEWER_H

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

#include <Inventor/Qt/viewers/SoQtViewer.h>

class SoQtPopupMenu;

// *************************************************************************

class SOQT_DLL_API SoQtFullViewer : public SoQtViewer {
  SOQT_OBJECT_ABSTRACT_HEADER(SoQtFullViewer, SoQtViewer);

public:
  enum BuildFlag {
    BUILD_NONE       = 0x00,
    BUILD_DECORATION = 0x01,
    BUILD_POPUP      = 0x02,
    BUILD_ALL        = (BUILD_DECORATION | BUILD_POPUP)
  };

  void setDecoration(const SbBool on);
  SbBool isDecoration(void) const;

  void setPopupMenuEnabled(const SbBool on);
  SbBool isPopupMenuEnabled(void) const;

  QWidget * getAppPushButtonParent(void) const;
  void addAppPushButton(QWidget * newButton);
  void insertAppPushButton(QWidget * newButton, int index);
  void removeAppPushButton(QWidget * oldButton);
  int findAppPushButton(QWidget * oldButton) const;
  int lengthAppPushButton(void) const;

  QWidget * getRenderAreaWidget(void) const;

  virtual void setViewing(SbBool on);

  virtual void setComponentCursor(const SoQtCursor & cursor);

protected:
  SoQtFullViewer(QWidget * parent,
                    const char * name,
                    SbBool embed,
                    BuildFlag flag,
                    Type type,
                    SbBool build);
  ~SoQtFullViewer();

  virtual void sizeChanged(const SbVec2s & size);

  QWidget * buildWidget(QWidget * parent);

  virtual void buildDecoration(QWidget * parent);
  virtual QWidget * buildLeftTrim(QWidget * parent);
  virtual QWidget * buildBottomTrim(QWidget * parent);
  virtual QWidget * buildRightTrim(QWidget * parent);
  QWidget * buildAppButtons(QWidget * parent);
  QWidget * buildViewerButtons(QWidget * parent);
  virtual void createViewerButtons(QWidget * parent, SbPList * buttonlist);

  virtual void buildPopupMenu(void);
  virtual void setPopupMenuString(const char * title);
  virtual void openPopupMenu(const SbVec2s position);

  virtual void leftWheelStart(void);
  virtual void leftWheelMotion(float);
  virtual void leftWheelFinish(void);
  float getLeftWheelValue(void) const;
  void setLeftWheelValue(const float value);

  virtual void bottomWheelStart(void);
  virtual void bottomWheelMotion(float);
  virtual void bottomWheelFinish(void);
  float getBottomWheelValue(void) const;
  void setBottomWheelValue(const float value);

  virtual void rightWheelStart(void);
  virtual void rightWheelMotion(float);
  virtual void rightWheelFinish(void);
  float getRightWheelValue(void) const;
  void setRightWheelValue(const float value);

  void setLeftWheelString(const char * const name);
  QWidget * getLeftWheelLabelWidget(void) const;
  void setBottomWheelString(const char * const name);
  QWidget * getBottomWheelLabelWidget(void) const;
  void setRightWheelString(const char * const name);
  const char * getRightWheelString() const;
  QWidget * getRightWheelLabelWidget(void) const;

  virtual SbBool processSoEvent(const SoEvent * const event);

protected:
  QWidget * leftWheel;
  QWidget * rightWheel;
  QWidget * bottomWheel;

  QWidget * leftDecoration;
  QWidget * rightDecoration;
  QWidget * bottomDecoration;

  QWidget * leftWheelLabel;
  char * leftWheelStr;
  float leftWheelVal;

  QWidget * rightWheelLabel;
  char * rightWheelStr;
  float rightWheelVal;

  QWidget * bottomWheelLabel;
  char * bottomWheelStr;
  float bottomWheelVal;

  SoQtPopupMenu * prefmenu;

private:
  // Private class for implementation hiding. The idiom we're using is
  // a variant of what is known as the "Cheshire Cat", and is also
  // described as the "Bridge" pattern in «Design Patterns» by Gamma
  // et al (aka The Gang Of Four).
  class SoQtFullViewerP * pimpl;

  friend class SoGuiFullViewerP;
  friend class SoQtFullViewerP;



// FIXME: get rid of non-templatized code. 20020108 mortene.

#ifdef __COIN_SOXT__ // FIXME: get rid of non-templatized code. 20020108 mortene.
protected:
  Widget buildFunctionsSubmenu(Widget popup);
  Widget buildDrawStyleSubmenu(Widget popup);

  char * popupTitle;
  SbBool popupEnabled;
  SbPList * viewerButtonWidgets;
#endif // __COIN_SOXT__
};

// *************************************************************************

#endif // ! SOQT_FULLVIEWER_H
