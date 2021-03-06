// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_TRACE_EVENT_ARGS_WHITELIST_H_
#define CHROME_COMMON_TRACE_EVENT_ARGS_WHITELIST_H_

// Used to filter trace event arguments against a whitelist of events that
// have been manually vetted to not include any PII.
bool IsTraceEventArgsWhitelisted(const char* category_group_name,
                                 const char* event_name);

#endif  // CHROME_COMMON_TRACE_EVENT_ARGS_WHITELIST_H_
