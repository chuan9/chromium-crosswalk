// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_FAKE_ACCOUNT_FETCHER_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_FAKE_ACCOUNT_FETCHER_SERVICE_H_

#include "base/memory/scoped_ptr.h"
#include "components/signin/core/browser/account_fetcher_service.h"

class KeyedService;

namespace content {
class BrowserContext;
}

// AccountTrackerService is a KeyedService that retrieves and caches GAIA
// information about Google Accounts.  This fake class can be used in tests
// to prevent AccountTrackerService from sending network requests.
class FakeAccountFetcherService : public AccountFetcherService {
 public:
  void FakeUserInfoFetchSuccess(const std::string& email,
                                const std::string& gaia,
                                const std::string& hosted_domain,
                                const std::string& full_name,
                                const std::string& given_name,
                                const std::string& locale,
                                const std::string& picture_url);

  static scoped_ptr<KeyedService> BuildForTests(
      content::BrowserContext* context);

  FakeAccountFetcherService();
  ~FakeAccountFetcherService() override;

 private:
  void StartFetchingUserInfo(const std::string& account_id) override;
  void StartFetchingChildInfo(const std::string& account_id) override;
  void SendRefreshTokenAnnotationRequest(
      const std::string& account_id) override;

  DISALLOW_COPY_AND_ASSIGN(FakeAccountFetcherService);
};

#endif  // CHROME_BROWSER_SIGNIN_FAKE_ACCOUNT_FETCHER_SERVICE_H_
