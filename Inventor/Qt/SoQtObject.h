#ifndef SOQTOBJECT_H
#define SOQTOBJECT_H

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

#include <assert.h>

#include <Inventor/SbBasic.h>
#include <Inventor/SbString.h>
#include <Inventor/SoType.h>

#include <Inventor/Qt/SoQtBasic.h>

// *************************************************************************

class SOQT_DLL_API SoQtObject {
  static SoType classTypeId;

public:
  static void initClass(void);
  static SoType getClassTypeId(void);
  virtual SoType getTypeId(void) const = 0;
  SbBool isOfType(SoType type) const;

  static void init(void);

  // FIXME: gcc-4 generates a warning when a class has virtual functions 
  // but no virtual destructor. Currently this warning is suppressed using 
  // the -Wno-non-virtual-dtor option, but this should be addressed for the
  // next major version... 20060404 kyrah

#if (SOQT_MAJOR_VERSION > 1)
#error Resolve missing virtual destructor issue for the new major release!
#endif

}; // SoQtObject

// *************************************************************************

// For a discussion about this #define, see Coin's SbBasic.h.

#define SOQT_SUN_CC_4_0_SOTYPE_INIT_BUG 0 /* assume compiler is ok for now */

#if SOQT_SUN_CC_4_0_SOTYPE_INIT_BUG
#define SOQT_STATIC_SOTYPE_INIT
#else
#define SOQT_STATIC_SOTYPE_INIT = SoType::badType()
#endif

// *************************************************************************

// The getTypeId() method should be abstract for abstract objects, but doing
// that would cause custom components derived from abstract components to
// have to include the typed object header / source, which could be a
// problem if the custom component wasn't written for Coin in the first
// place.

#define SOQT_OBJECT_ABSTRACT_HEADER(classname, parentname) \
public: \
  static void initClass(void); \
  static SoType getClassTypeId(void); \
  virtual SoType getTypeId(void) const /* = 0 (see comment above) */; \
private: \
  typedef parentname inherited; \
  static SoType classTypeId

#define SOQT_OBJECT_HEADER(classname, parentname) \
public: \
  static void initClass(void); \
  static SoType getClassTypeId(void); \
  virtual SoType getTypeId(void) const; \
  static void * createInstance(void); \
private: \
  typedef parentname inherited; \
  static SoType classTypeId

#define SOQT_OBJECT_ABSTRACT_SOURCE(classname) \
void classname::initClass(void) { \
  assert(classname::classTypeId == SoType::badType()); \
  classname::classTypeId = \
    SoType::createType(inherited::getClassTypeId(), \
                        SO__QUOTE(classname)); \
} \
SoType classname::getClassTypeId(void) { \
  return classname::classTypeId; \
} \
SoType classname::getTypeId(void) const { \
  return classname::classTypeId; \
} \
SoType classname::classTypeId SOQT_STATIC_SOTYPE_INIT

#define SOQT_OBJECT_SOURCE(classname) \
void classname::initClass(void) { \
  assert(classname::classTypeId == SoType::badType()); \
  classname::classTypeId = \
    SoType::createType(inherited::getClassTypeId(), \
                        SO__QUOTE(classname), \
                        classname::createInstance); \
} \
SoType classname::getClassTypeId(void) { \
  return classname::classTypeId; \
} \
SoType classname::getTypeId(void) const { \
  return classname::classTypeId; \
} \
void * classname::createInstance(void) { \
  assert(classname::classTypeId != SoType::badType()); \
  return (void *) new classname; \
} \
SoType classname::classTypeId SOQT_STATIC_SOTYPE_INIT

// *************************************************************************

#endif // ! SOQTOBJECT_H
