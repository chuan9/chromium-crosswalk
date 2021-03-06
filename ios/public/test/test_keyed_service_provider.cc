// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/test/test_keyed_service_provider.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "components/sync_driver/fake_sync_service.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/public/test/fake_sync_service_factory.h"

namespace ios {
namespace {

class MissingServiceKeyedServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static MissingServiceKeyedServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<MissingServiceKeyedServiceFactory>;

  MissingServiceKeyedServiceFactory();
  ~MissingServiceKeyedServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  scoped_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(MissingServiceKeyedServiceFactory);
};

// static
MissingServiceKeyedServiceFactory*
MissingServiceKeyedServiceFactory::GetInstance() {
  return Singleton<MissingServiceKeyedServiceFactory>::get();
}

MissingServiceKeyedServiceFactory::MissingServiceKeyedServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "MissingService",
          BrowserStateDependencyManager::GetInstance()) {
}

MissingServiceKeyedServiceFactory::~MissingServiceKeyedServiceFactory() {
}

scoped_ptr<KeyedService>
MissingServiceKeyedServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  NOTREACHED();
  return nullptr;
}

}  // namespace

TestKeyedServiceProvider::TestKeyedServiceProvider() {
}

TestKeyedServiceProvider::~TestKeyedServiceProvider() {
}

void TestKeyedServiceProvider::AssertKeyedFactoriesBuilt() {
  FakeSyncServiceFactory::GetInstance();
  MissingServiceKeyedServiceFactory::GetInstance();
}

KeyedServiceBaseFactory* TestKeyedServiceProvider::GetBookmarkModelFactory() {
  return MissingServiceKeyedServiceFactory::GetInstance();
}

bookmarks::BookmarkModel*
TestKeyedServiceProvider::GetBookmarkModelForBrowserState(
    ChromeBrowserState* browser_state) {
  return nullptr;
}

KeyedServiceBaseFactory*
TestKeyedServiceProvider::GetProfileOAuth2TokenServiceFactory() {
  return MissingServiceKeyedServiceFactory::GetInstance();
}

ProfileOAuth2TokenService*
TestKeyedServiceProvider::GetProfileOAuth2TokenServiceForBrowserState(
    ChromeBrowserState* browser_state) {
  return nullptr;
}

KeyedServiceBaseFactory* TestKeyedServiceProvider::GetSigninManagerFactory() {
  return MissingServiceKeyedServiceFactory::GetInstance();
}

SigninManager* TestKeyedServiceProvider::GetSigninManagerForBrowserState(
    ChromeBrowserState* browser_state) {
  return nullptr;
}

KeyedServiceBaseFactory*
TestKeyedServiceProvider::GetPersonalDataManagerFactory() {
  return MissingServiceKeyedServiceFactory::GetInstance();
}

autofill::PersonalDataManager*
TestKeyedServiceProvider::GetPersonalDataManagerForBrowserState(
    ChromeBrowserState* browser_state) {
  return nullptr;
}

KeyedServiceBaseFactory* TestKeyedServiceProvider::GetSyncServiceFactory() {
  return FakeSyncServiceFactory::GetInstance();
}

sync_driver::SyncService*
TestKeyedServiceProvider::GetSyncServiceForBrowserState(
    ChromeBrowserState* browser_state) {
  return FakeSyncServiceFactory::GetForBrowserState(browser_state);
}

}  // namespace ios
