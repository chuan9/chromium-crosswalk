// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_NET_CONNECTIVITY_CHECKER_IMPL_H_
#define CHROMECAST_NET_CONNECTIVITY_CHECKER_IMPL_H_

#include "chromecast/net/connectivity_checker.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_request.h"

class GURL;

namespace base {
class SingleThreadTaskRunner;
}

namespace net {
class SSLInfo;
class URLRequest;
class URLRequestContext;
}

namespace chromecast {

// Simple class to check network connectivity by sending a HEAD http request
// to given url.
class ConnectivityCheckerImpl
    : public ConnectivityChecker,
      public net::URLRequest::Delegate,
      public net::NetworkChangeNotifier::ConnectionTypeObserver,
      public net::NetworkChangeNotifier::IPAddressObserver {
 public:
  explicit ConnectivityCheckerImpl(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  // ConnectivityChecker implementation:
  bool Connected() const override;
  void Check() override;

 protected:
  ~ConnectivityCheckerImpl() override;

 private:
  // UrlRequest::Delegate implementation:
  void OnResponseStarted(net::URLRequest* request) override;
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override;
  void OnSSLCertificateError(net::URLRequest* request,
                             const net::SSLInfo& ssl_info,
                             bool fatal) override;

  // Initializes ConnectivityChecker
  void Initialize();

  // NetworkChangeNotifier::ConnectionTypeObserver implementation:
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // net::NetworkChangeNotifier::IPAddressObserver implementation:
  void OnIPAddressChanged() override;

  // Cancels current connectivity checking in progress.
  void Cancel();

  // Sets connectivity and alerts observers if it has changed
  void SetConnected(bool connected);

  // Called when URL request failed.
  void OnUrlRequestError();

  scoped_ptr<GURL> connectivity_check_url_;
  scoped_ptr<net::URLRequestContext> url_request_context_;
  scoped_ptr<net::URLRequest> url_request_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  bool connected_;
  // Number of connectivity check errors.
  unsigned int check_errors_;

  DISALLOW_COPY_AND_ASSIGN(ConnectivityCheckerImpl);
};

}  // namespace chromecast

#endif  // CHROMECAST_NET_CONNECTIVITY_CHECKER_IMPL_H_
