/*
 * SPDX-FileCopyrightText: 2026 Project LemonLime
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "safefile.h"

#include <QFile>
#include <QFileInfo>

#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

#ifdef Q_OS_UNIX

	// Opens `path` without following the final symlink and only accepts a regular
	// file with a single link. The caller owns the returned descriptor.
	auto openRegularFile(const QString &path, int flags) -> int {
		const QByteArray nativePath = QFile::encodeName(path);
		// O_NONBLOCK is essential: opening a FIFO without it blocks here, before
		// fstat() has a chance to reject the file type.
		const int fd = ::open(nativePath.constData(), flags | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);

		if (fd < 0)
			return -1;

		struct stat info {};
		if (::fstat(fd, &info) != 0 || ! S_ISREG(info.st_mode) || info.st_nlink != 1) {
			::close(fd);
			return -1;
		}

		// O_NONBLOCK has no effect on regular files, but drop it so callers get a
		// normal blocking stream.
		const int statusFlags = ::fcntl(fd, F_GETFL);
		if (statusFlags != -1)
			::fcntl(fd, F_SETFL, statusFlags & ~O_NONBLOCK);

		return fd;
	}

#else

	auto isRegularFile(const QString &path) -> bool {
		const QFileInfo info(path);
		return info.exists() && info.isFile() && ! info.isSymLink() && ! info.isJunction();
	}

#endif

} // namespace

auto isContestantRegularFile(const QString &path) -> bool {
#ifdef Q_OS_UNIX
	const int fd = openRegularFile(path, O_RDONLY);

	if (fd < 0)
		return false;

	::close(fd);
	return true;
#else
	return isRegularFile(path);
#endif
}

auto openContestantReadOnly(const QString &path) -> FILE * {
#ifdef Q_OS_UNIX
	const int fd = openRegularFile(path, O_RDONLY);

	if (fd < 0)
		return nullptr;

	FILE *file = ::fdopen(fd, "r");

	if (file == nullptr)
		::close(fd);

	return file;
#else
	if (! isRegularFile(path))
		return nullptr;

	return std::fopen(QFile::encodeName(path).constData(), "r");
#endif
}

auto readContestantFileTail(const QString &path, qint64 maxBytes) -> QByteArray {
	QByteArray result;

	if (maxBytes <= 0)
		return result;

	// Keep the temporary allocation bounded even if a caller passes a huge value.
	const qint64 limit = qMin<qint64>(maxBytes, 1024 * 1024);

#ifdef Q_OS_UNIX
	const int fd = openRegularFile(path, O_RDONLY);

	if (fd < 0)
		return result;

	struct stat info {};

	if (::fstat(fd, &info) == 0) {
		const qint64 size = static_cast<qint64>(info.st_size);
		const qint64 toRead = qMin(size, limit);

		if (toRead > 0) {
			const off_t offset = static_cast<off_t>(size - toRead);
			result.resize(static_cast<int>(toRead));
			const ssize_t readBytes = ::pread(fd, result.data(), static_cast<size_t>(toRead), offset);

			if (readBytes < 0)
				result.clear();
			else
				result.resize(static_cast<int>(readBytes));
		}
	}

	::close(fd);
#else
	if (! isRegularFile(path))
		return result;

	QFile file(path);

	if (! file.open(QIODevice::ReadOnly))
		return result;

	const qint64 size = file.size();

	if (size > limit)
		file.seek(size - limit);

	result = file.read(limit);
#endif

	return result;
}
