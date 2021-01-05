;;;; -*- scheme -*-
;;;; GNU Mailutils -- a suite of utilities for electronic mail
;;;; Copyright (C) 2002-2021 Free Software Foundation, Inc.
;;;;
;;;; GNU Mailutils is free software; you can redistribute it and/or modify
;;;; it under the terms of the GNU General Public License as published by
;;;; the Free Software Foundation; either version 3, or (at your option)
;;;; any later version.
;;;; 
;;;; GNU Mailutils is distributed in the hope that it will be useful,
;;;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;;;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;;; GNU General Public License for more details.
;;;; 
;;;; You should have received a copy of the GNU General Public License along
;;;; with GNU Mailutils.  If not, see <http://www.gnu.org/licenses/>.
;;;;
(define-module (mailutils ancilla))
(use-modules ((mailutils mailutils))
	     ((ice-9 regex)))

(define-public (message-format dest msg)
  "Formats a message hiding all mutable information (email and date)."
  "The @samp{dest} argument has the same meaning as in @samp{format}."
  (let ((s (with-output-to-string (lambda () (display msg)))))
    (cond
     ((string-match "^(#<message )\".+@.+\" \"(((Sun|Mon|Tue|Wed|Thu|Fri|Sat) (Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) [ 0123][0-9] [[:digit:]]{2}:[[:digit:]]{2})|UNKNOWN)\" ([[:digit:]]+) ([[:digit:]]+)>$" s) =>
      (lambda (m)
	(format dest "~a\"X@Y\" \"Dow Mon Day HH:MM\" ~a ~a>"
		(match:substring m 1)
	        (match:substring m 6)
	        (match:substring m 7)))))))

(define-public (string->message str)
  (mu-message-from-port (open-input-string str)))

(define-public (file->message file)
  (mu-message-from-port (open-file file "r")))

(define-public (test-message)
  (string->message
   "From user@example.org Fri Jun  8 14:30:40 2018
From: user@example.org
To: someone@example.com
Subject: De omnibus rebus et quibusdam aliis

Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod
tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam,
quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo
consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse
cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat
non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.
"))

(define-public (sort-headers hdr)
  (sort hdr (lambda (a b)
	      (if (string-ci=? (car a) (car b))
		  (string<? (cdr a) (cdr b))
		  (string-ci<? (car a) (car b))))))
