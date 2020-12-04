// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#ifndef QT_TRANSLATE_NOOP
// Macro to mark text for localization by Qt's lupdate tool
#define QT_TRANSLATE_NOOP(_context, _string) (_string)
#endif
