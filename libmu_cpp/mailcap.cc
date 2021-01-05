/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2004-2021 Free Software Foundation, Inc.

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

#include <mailutils/cpp/mailcap.h>

using namespace mailutils;

//
// Mailcap
//

Mailcap :: Mailcap (const Stream& stm)
{
  int status;

  status = mu_mailcap_create (&mailcap);
  if (status == 0)
    {
      status = mu_mailcap_parse (mailcap, stm.stm, NULL);
      if (status == MU_ERR_PARSE)
	status = 0; /* FIXME */
    }
  if (status)
    throw Exception ("Mailcap::Mailcap", status);
}

Mailcap :: Mailcap (const mu_mailcap_t mailcap)
{
  if (mailcap == 0)
    throw Exception ("Mailcap::Mailcap", EINVAL);

  this->mailcap = mailcap;
}

Mailcap :: ~Mailcap ()
{
  mu_mailcap_destroy (&mailcap);
}

size_t
Mailcap :: entries_count ()
{
  size_t count = 0;
  int status = mu_mailcap_get_count (mailcap, &count);
  if (status)
    throw Exception ("Mailcap::entries_count", status);
  return count;
}

MailcapEntry&
Mailcap :: find_entry (const std::string& name)
{
  mu_mailcap_entry_t c_entry;

  int status = mu_mailcap_find_entry (mailcap, name.c_str (), &c_entry);
  if (status)
    throw Exception ("Mailcap::find_entry", status);

  MailcapEntry* entry = new MailcapEntry (c_entry);
  return *entry;
}

//
// MailcapEntry
//

MailcapEntry :: MailcapEntry (mu_mailcap_entry_t entry)
{
  if (entry == 0)
    throw Exception ("MailcapEntry::MailcapEntry", EINVAL);

  this->entry = entry;
}

size_t
MailcapEntry :: fields_count ()
{
  size_t count = 0;
  int status = mu_mailcap_entry_fields_count (entry, &count);
  if (status)
    throw Exception ("MailcapEntry::fields_count", status);
  return count;
}

std::string
MailcapEntry :: get_field (const std::string& name)
{
  char const *value;
  int status = mu_mailcap_entry_sget_field (entry, name.c_str (), &value);
  if (status)
    throw Exception ("MailcapEntry::get_field", status);
  return std::string (value ? value : "");
}

std::string
MailcapEntry :: get_typefield ()
{
  char const *value;
  int status = mu_mailcap_entry_sget_type (entry, &value);
  if (status)
    throw Exception ("MailcapEntry::get_typefield", status);
  return std::string (value);
}

std::string
MailcapEntry :: get_viewcommand ()
{
  char const *value;
  int status = mu_mailcap_entry_sget_command (entry, &value); 
  if (status)
    throw Exception ("MailcapEntry::get_viewcommand", status);
  return std::string (value);
}

