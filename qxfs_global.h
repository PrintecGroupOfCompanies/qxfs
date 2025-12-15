#ifndef QXFS_GLOBAL_H
#define QXFS_GLOBAL_H

#include <QtCore/qglobal.h>

#if !defined(QXFS_EXPORT)
#  if defined(QXFS_LIBRARY)
#    define QXFS_EXPORT Q_DECL_EXPORT
#  else
#    define QXFS_EXPORT Q_DECL_IMPORT
#  endif
#endif

#endif // QXFS_GLOBAL_H
