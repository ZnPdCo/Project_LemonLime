/*
 * SPDX-FileCopyrightText: 2026 Project LemonLime
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#pragma once

#include <QByteArray>
#include <QString>
#include <QtGlobal>
#include <cstdio>

// Files produced in the contestant's working directory are controlled by the
// sandboxed program. LemonLime must never open them by path without checking
// what they are: a symbolic link would be resolved by LemonLime itself
// (possibly outside the sandbox), a FIFO would block the judging thread
// forever, and a device node such as /dev/zero can make readAll() grow without
// bounds.
auto isContestantRegularFile(const QString &path) -> bool;
auto openContestantReadOnly(const QString &path) -> FILE *;
auto readContestantFileTail(const QString &path, qint64 maxBytes) -> QByteArray;
