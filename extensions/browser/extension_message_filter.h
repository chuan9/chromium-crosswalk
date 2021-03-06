// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_MESSAGE_FILTER_H_
#define EXTENSIONS_BROWSER_EXTENSION_MESSAGE_FILTER_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "content/public/browser/browser_message_filter.h"

class GURL;

namespace content {
class BrowserContext;
}

namespace gfx {
class Size;
}

namespace extensions {

class EventRouter;
class ProcessManager;

// This class filters out incoming extension-specific IPC messages from the
// renderer process. It is created and destroyed on the UI thread and handles
// messages there.
class ExtensionMessageFilter : public content::BrowserMessageFilter {
 public:
  ExtensionMessageFilter(int render_process_id,
                         content::BrowserContext* context);

  int render_process_id() { return render_process_id_; }

  static void EnsureShutdownNotifierFactoryBuilt();

 private:
  friend class base::DeleteHelper<ExtensionMessageFilter>;
  friend class content::BrowserThread;

  ~ExtensionMessageFilter() override;

  void ShutdownOnUIThread();

  // content::BrowserMessageFilter implementation.
  void OverrideThreadForMessage(const IPC::Message& message,
                                content::BrowserThread::ID* thread) override;
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // Message handlers on the UI thread.
  void OnExtensionAddListener(const std::string& extension_id,
                              const GURL& listener_url,
                              const std::string& event_name);
  void OnExtensionRemoveListener(const std::string& extension_id,
                                 const GURL& listener_url,
                                 const std::string& event_name);
  void OnExtensionAddLazyListener(const std::string& extension_id,
                                  const std::string& event_name);
  void OnExtensionRemoveLazyListener(const std::string& extension_id,
                                     const std::string& event_name);
  void OnExtensionAddFilteredListener(const std::string& extension_id,
                                      const std::string& event_name,
                                      const base::DictionaryValue& filter,
                                      bool lazy);
  void OnExtensionRemoveFilteredListener(const std::string& extension_id,
                                         const std::string& event_name,
                                         const base::DictionaryValue& filter,
                                         bool lazy);
  void OnExtensionShouldSuspendAck(const std::string& extension_id,
                                   int sequence_id);
  void OnExtensionSuspendAck(const std::string& extension_id);
  void OnExtensionTransferBlobsAck(const std::vector<std::string>& blob_uuids);

  const int render_process_id_;

  scoped_ptr<KeyedServiceShutdownNotifier::Subscription> shutdown_notifier_;

  // Owned by the browser context; should only be accessed on the UI thread.
  EventRouter* event_router_;
  ProcessManager* process_manager_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageFilter);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_MESSAGE_FILTER_H_
