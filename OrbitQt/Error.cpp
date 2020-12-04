// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "Error.h"

#include <absl/strings/str_format.h>

#include <QCoreApplication>
#include <QString>

namespace OrbitQt {

static QString localized_message(int condition) {
  switch (static_cast<Error>(condition)) {
    case Error::kCouldNotConnectToServer:
      return QCoreApplication::translate("ErrorCategory", "Could not connect to remote server.");
    case Error::kCouldNotUploadPackage:
      return QCoreApplication::translate(
          "ErrorCategory",
          "Could not upload OrbitService package to remote. Please make "
          "sure the .deb package is located in the `collector` folder.");
    case Error::kCouldNotUploadSignature:
      return QCoreApplication::translate(
          "ErrorCategory",
          "Could not upload OrbitService signature to remote. Please make "
          "sure the .deb.asc signature is located in the `collector` "
          "folder.");
    case Error::kCouldNotInstallPackage:
      return QCoreApplication::translate("ErrorCategory",
                                         "Could not install OrbitService on remote.");
    case Error::kCouldNotStartTunnel:
      return QCoreApplication::translate("ErrorCategory", "Could not start tunnel to remote.");
    case Error::kUserCanceledServiceDeployment:
      return QCoreApplication::translate("ErrorCategory", "User canceled the deployment.");
    case Error::kUserClosedStartUpWindow:
      return QCoreApplication::translate("ErrorCategory", "User closed window.");
  }

  return QCoreApplication::translate("ErrorCategory", "Unknown error condition: %i.")
      .arg(condition);
}

std::string ErrorCategory::message(int condition) const {
  return localized_message(condition).toStdString();
}
}  // namespace OrbitQt