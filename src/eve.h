/* -*- mode: c; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: t -*-
 * vim: ts=4 sw=4 noet ai cindent syntax=cpp
 *
 * Conky, a system monitor, based on torsmo
 *
 * Copyright (c) 2008 Asbjørn Zweidorff Kjær
 * Copyright (c) 2005-2010 Brenden Matthews, Philip Kovacs, et. al.
 *	(see AUTHORS)
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef _EVE_H
#define _EVE_H

#define EVEURL_TRAINING "http://api.eve-online.com/char/SkillInTraining.xml.aspx"
#define EVEURL_SKILLTREE "http://api.eve-online.com/eve/Skilltree.xml.aspx"
#define EVE_OUTPUT_FORMAT "%s %d in %s"

void scan_eve(struct text_object *, const char *);
void print_eve(struct text_object *, char *, int);
void free_eve(struct text_object *);

#endif /* _EVE_H */
