/*
 * SomnoTrace - pure helpers for the GitHub update-availability check
 * Copyright (C) 2026 Ilya Kruchinin <https://github.com/ilyakruchinin>
 *
 * This file is part of SomnoTrace.
 *
 * SomnoTrace is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * SomnoTrace is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ADDITIONAL TERM (GPLv3 Section 7(b)): Redistributions must preserve the
 * attribution "Based on SomnoTrace, originally created by Ilya Kruchinin
 * (https://github.com/ilyakruchinin)." See the NOTICE file for details.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

/* True when `slug` is a plausible "owner/repo" slug: exactly one '/',
 * non-empty segments, charset [A-Za-z0-9_.-]. Rejects the sentinel strings
 * "local build" / "unknown CI build" (no slash, spaces) and anything odd. */
bool upd_slug_is_repo(const char *slug);

/* Extract the tag from a GitHub /releases/latest 302 Location header.
 * Expects ".../releases/tag/<tag>"; copies <tag> into out (NUL-terminated,
 * stopping at '?'/'#'). Returns 0 on success, -1 when no /tag/ tail exists
 * (e.g. redirect to bare /releases = repo has no stable release). */
int upd_extract_tag(const char *location, char *out, size_t out_len);

/* Update exists when the latest tag differs from the running version.
 * Raw string compare on purpose: CI tag builds produce byte-identical
 * "vX.Y.Z" strings, and any difference — including "-dev+N"/"+dirty" —
 * means the running binary is not that release artifact. */
bool upd_is_update(const char *tag, const char *fw_ver);
