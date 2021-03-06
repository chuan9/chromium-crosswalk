// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAD_TAB_TYPES_H_
#define CHROME_BROWSER_UI_SAD_TAB_TYPES_H_

namespace chrome {

enum SadTabKind {
  SAD_TAB_KIND_CRASHED,  // Tab crashed.
#if defined(OS_CHROMEOS)
  SAD_TAB_KIND_KILLED_BY_OOM,  // Tab killed by oom killer.
#endif
  SAD_TAB_KIND_KILLED  // Tab killed.
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SAD_TAB_TYPES_H_
