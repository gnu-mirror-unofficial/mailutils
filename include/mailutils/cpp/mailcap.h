/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2004-2019 Free Software Foundation, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General
   Public License along with this library.  If not, see 
   <http://www.gnu.org/licenses/>. */

#ifndef _MUCPP_MAILCAP_H
#define _MUCPP_MAILCAP_H

#include <errno.h>
#include <mailutils/mailcap.h>
#include <mailutils/cpp/error.h>
#include <mailutils/cpp/stream.h>

namespace mailutils
{

class MailcapEntry
{
 private:
  char buf[256];

 protected:
  mu_mailcap_entry_t entry;

 public:
  MailcapEntry (mu_mailcap_entry_t);

  size_t fields_count ();
  std::string get_typefield ();
  std::string get_viewcommand ();
  std::string get_field (const std::string& name);

  inline std::string operator [] (const std::string& name) {
    return this->get_field (name);
  }
};

class Mailcap
{
 protected:
  mu_mailcap_t mailcap;

 public:
  Mailcap (const Stream&);
  Mailcap (const mu_mailcap_t);
  ~Mailcap ();

  size_t entries_count ();
  MailcapEntry& find_entry (const std::string& name);

  inline MailcapEntry& operator [] (const std::string& name) {
    return this->find_entry (name);
  }
};

}

#endif // not _MUCPP_MAILCAP_H

