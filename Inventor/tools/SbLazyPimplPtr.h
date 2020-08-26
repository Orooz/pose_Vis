#ifndef COIN_SBLAZYPIMPLPTR_H
#define COIN_SBLAZYPIMPLPTR_H

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

// The SbLazyPimplPtr<> class is loosely based on the boost::pimpl_ptr<>-
// proposal of 2005 by Asger Mangaard.
//
// It is reimplemented here as separate classes, not with policy-based
// templates, because we still try to support compilers that don't support
// template templates (e.g. Visual C++ 6).
//
// See also SbPimplPtr<>, our implementation of the immediate-creation
// policy.

#include <Inventor/SbBasic.h>

/* ********************************************************************** */

#if defined(_MSC_VER) && (_MSC_VER < 1400) /* MSVC <8 */
#pragma warning(push)
#pragma warning(disable:4251) // for DLL-interface warning
#endif /* MSVC <8 */

template <class T>
class COIN_DLL_API SbLazyPimplPtr {
public:
  SbLazyPimplPtr(void);
  SbLazyPimplPtr(T * initial);
  SbLazyPimplPtr(const SbLazyPimplPtr<T> & copy);
  ~SbLazyPimplPtr(void);

  void set(T * value);
  T & get(void) const;

  SbLazyPimplPtr<T> & operator = (const SbLazyPimplPtr<T> & copy);

  SbBool operator == (const SbLazyPimplPtr<T> & rhs) const;
  SbBool operator != (const SbLazyPimplPtr<T> & rhs) const;

  const T * operator -> (void) const;
  T * operator -> (void);

protected:
  T * getNew(void) const;

protected:
  mutable T * ptr;

}; // SbLazyPimplPtr

#ifdef COIN_INTERNAL
// the implementation is in the .hpp class
#include <Inventor/tools/SbLazyPimplPtr.hpp>
#endif // COIN_INTERNAL

#if defined(_MSC_VER) && (_MSC_VER < 1400) /* MSVC <8 */
#pragma warning(pop)
#endif /* MSVC <8 */

/* ********************************************************************** */

#endif // !COIN_SBLAZYPIMPLPTR_H
