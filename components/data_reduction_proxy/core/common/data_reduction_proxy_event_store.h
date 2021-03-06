// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_EVENT_STORE_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_EVENT_STORE_H_

#include <deque>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_storage_delegate.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"

namespace base {
class DictionaryValue;
class TimeDelta;
class Value;
}

namespace data_reduction_proxy {

class DataReductionProxyEventStore
    : public DataReductionProxyEventStorageDelegate {
 public:
  // Adds data reduction proxy specific constants to the net_internals
  // constants dictionary.
  static void AddConstants(base::DictionaryValue* constants_dict);

  // Constructs a DataReductionProxyEventStore object
  explicit DataReductionProxyEventStore();

  virtual ~DataReductionProxyEventStore();

  // Creates a Value summary of Data Reduction Proxy related information:
  // - Whether the proxy is enabled
  // - The proxy configuration
  // - The state of the last secure proxy check response
  // - A stream of the last Data Reduction Proxy related events.
  // The caller is responsible for deleting the returned value.
  base::Value* GetSummaryValue() const;

  // Adds DATA_REDUCTION_PROXY event with no parameters to the event store.
  void AddEvent(scoped_ptr<base::Value> event) override;

  // Override of DataReductionProxyEventStorageDelegate.
  // Put |entry| on the deque of stored events and set |current_configuration_|.
  void AddEnabledEvent(scoped_ptr<base::Value> entry, bool enabled) override;

  // Override of DataReductionProxyEventStorageDelegate.
  // Put |entry| on a deque of events to store and set
  // |secure_proxy_check_state_|
  void AddEventAndSecureProxyCheckState(scoped_ptr<base::Value> entry,
                                        SecureProxyCheckState state) override;

  // Override of DataReductionProxyEventStorageDelegate.
  // Put |entry| on a deque of events to store and set |last_bypass_event_| and
  // |expiration_ticks_|
  void AddAndSetLastBypassEvent(scoped_ptr<base::Value> entry,
                                int64 expiration_ticks) override;

 private:
  friend class DataReductionProxyEventStoreTest;

  // A deque of data reduction proxy related events. It is used as a circular
  // buffer to prevent unbounded memory utilization.
  std::deque<base::Value*> stored_events_;
  // Whether the data reduction proxy is enabled or not.
  bool enabled_;
  // The current data reduction proxy configuration.
  scoped_ptr<base::Value> current_configuration_;
  // The state based on the last secure proxy check.
  SecureProxyCheckState secure_proxy_check_state_;
  // The last seen data reduction proxy bypass event.
  scoped_ptr<base::Value> last_bypass_event_;
  // The expiration time of the |last_bypass_event_|.
  int64 expiration_ticks_;

  // Enforce usage on the UI thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyEventStore);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_EVENT_STORE_H_
