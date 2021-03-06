// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/fake_account_fetcher_service.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"

// static
scoped_ptr<KeyedService> FakeAccountFetcherService::BuildForTests(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  FakeAccountFetcherService* service = new FakeAccountFetcherService();
  service->Initialize(ChromeSigninClientFactory::GetForProfile(profile),
                      ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
                      AccountTrackerServiceFactory::GetForProfile(profile));
  return scoped_ptr<KeyedService>(service);
}

FakeAccountFetcherService::FakeAccountFetcherService() {}

FakeAccountFetcherService::~FakeAccountFetcherService() {}

void FakeAccountFetcherService::StartFetchingUserInfo(
    const std::string& account_id) {
  // In tests, don't do actual network fetch.
}

void FakeAccountFetcherService::FakeUserInfoFetchSuccess(
    const std::string& email,
    const std::string& gaia,
    const std::string& hosted_domain,
    const std::string& full_name,
    const std::string& given_name,
    const std::string& locale,
    const std::string& picture_url) {
  base::DictionaryValue user_info;
  user_info.SetString("id", gaia);
  user_info.SetString("email", email);
  user_info.SetString("hd", hosted_domain);
  user_info.SetString("name", full_name);
  user_info.SetString("given_name", given_name);
  user_info.SetString("locale", locale);
  user_info.SetString("picture", picture_url);
  std::vector<std::string> service_flags;
  account_tracker_service()->SetAccountStateFromUserInfo(
      account_tracker_service()->PickAccountIdForAccount(gaia, email),
      &user_info,
      &service_flags);
}

void FakeAccountFetcherService::SendRefreshTokenAnnotationRequest(
    const std::string& account_id) {
  // In tests, don't do actual network fetch.
}
