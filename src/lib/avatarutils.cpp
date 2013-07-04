/*********************************************************************************
 ** This file is part of QtContacts tracker storage plugin
 **
 ** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
 **
 ** Contact:  Nokia Corporation (info@qt.nokia.com)
 **
 ** GNU Lesser General Public License Usage
 ** This file may be used under the terms of the GNU Lesser General Public License
 ** version 2.1 as published by the Free Software Foundation and appearing in the
 ** file LICENSE.LGPL included in the packaging of this file.  Please review the
 ** following information to ensure the GNU Lesser General Public License version
 ** 2.1 requirements will be met:
 ** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 **
 ** In addition, as a special exception, Nokia gives you certain additional rights.
 ** These rights are described in the Nokia Qt LGPL Exception version 1.1, included
 ** in the file LGPL_EXCEPTION.txt in this package.
 **
 ** Other Usage
 ** Alternatively, this file may be used in accordance with the terms and
 ** conditions contained in a signed written agreement between you and Nokia.
 *********************************************************************************/

#include "avatarutils.h"

#include "logger.h"
#include "fileutils.h"
#include "settings.h"
#include "threadlocaldata.h"

#include <errno.h>

#include <QImageWriter>
#include <QCryptographicHash>

static QString
makeAvatarCacheFileName(const QImage &image, QFile::FileError *error)
{
    const QDir avatarCacheDir = qctAvatarLocalDataDir();

    // Create avatar cache directory when needed.
    if (not avatarCacheDir.mkpath(QLatin1String("."))) {
        qctWarn(QString::fromLatin1("Cannot create avatar cache folder %1: %2").
                arg(avatarCacheDir.path(), QString::fromLocal8Bit(strerror(errno))));

        if (error) {
            switch(errno) {
            case ENOSPC: case EMFILE:
                *error = QFile::ResourceError;
                break;
            case EPERM:
                *error = QFile::PermissionsError;
                break;
            default:
                *error = QFile::UnspecifiedError;
                break;
            }
        }

        return QString();
    }

    // Create filename from cryptographic hash of the pixel data. Probability that two
    // different pictures on the same device have the same hash code, is significantly
    // smaller than probability that we mess up because of miscalculations caused by
    // electrostatics and flipping bits. That's why we can apply this cheap trick.

    const char *const rawPixelData = reinterpret_cast<const char *>(image.constBits());
    const QByteArray pixels = QByteArray::fromRawData(rawPixelData, image.byteCount());
    const QByteArray pixelHash = QCryptographicHash::hash(pixels, QCryptographicHash::Sha1).toHex();
    const QString fileName = QString::fromLatin1("%1.png").arg(QString::fromLatin1(pixelHash));

    if (error) {
        *error = QFile::NoError;
    }

    return avatarCacheDir.absoluteFilePath(fileName);
}

QString
qctWriteThumbnail(QImage &image)
{
    return qctWriteThumbnail(image, 0);
}

QString
qctWriteThumbnail(QImage &image, QFile::FileError *error)
{
    const QSize avatarSize = QctThreadLocalData::instance()->settings()->avatarSize();

    if (image.width() > avatarSize.width() ||
        image.height() > avatarSize.height()) {
        image = image.scaled(avatarSize, Qt::KeepAspectRatio);
    }

    // Save the avatar image when neeed. The cached file name is based on
    // a cryptographic hash of the thumbnail's pixels. So if there already
    // exists a image file with the calculated name, there is an incredibly
    // high probability the existing file is equal to this current avatar.
    // We prefer to not write the file in this case for better performance
    // and for better lifetime of the storage medium.
    const QString avatarFileName = makeAvatarCacheFileName(image, error);

    if (avatarFileName.isEmpty()) {
        // failed to build file name
        return QString();
    }

    QFile avatarFile(avatarFileName);

    if (not avatarFile.exists()) {
        QImageWriter writer;

        writer.setDevice(&avatarFile);

        if (not writer.write(image)) {
            qctWarn(QString::fromLatin1("Cannot save avatar thumbnail: %1").
                    arg(writer.errorString()));

            if (error) {
                if (QFile::NoError != avatarFile.error()) {
                    *error = avatarFile.error();
                } else {
                    *error = QFile::UnspecifiedError;
                }
            }

            if (not avatarFile.remove()) {
                qctWarn(QString::fromLatin1("Cannot remove invalid avatar file %1").
                        arg(avatarFileName));
            }

            return QString();
        }
    }

    if (error) {
        *error = QFile::NoError;
    }

    return avatarFileName;
}
