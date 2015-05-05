// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TESTING_PROFILE_H_
#define CHROME_TEST_BASE_TESTING_PROFILE_H_

#include <string>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/domain_reliability/clear_mode.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class MockResourceContext;
class SSLHostStateDelegate;
class ZoomLevelDelegate;
}

namespace history {
class TopSites;
}

namespace net {
class CookieMonster;
class URLRequestContextGetter;
}

namespace policy {
class PolicyService;
class ProfilePolicyConnector;
class SchemaRegistryService;
}

namespace storage {
class SpecialStoragePolicy;
}

class BrowserContextDependencyManager;
class ExtensionSpecialStoragePolicy;
class HostContentSettingsMap;
class PrefServiceSyncable;
class TestingPrefServiceSyncable;

class TestingProfile : public Profile {
 public:
  // Profile directory name for the test user. This is "Default" on most
  // platforms but must be different on ChromeOS because a logged-in user cannot
  // use "Default" as profile directory.
  // Browser- and UI tests should always use this to get to the user's profile
  // directory. Unit-tests, though, should use |kInitialProfile|, which is
  // always "Default", because they are runnining without logged-in user.
  static const char kTestUserProfileDir[];

  // Default constructor that cannot be used with multi-profiles.
  TestingProfile();

  typedef std::vector<std::pair<
              BrowserContextKeyedServiceFactory*,
              BrowserContextKeyedServiceFactory::TestingFactoryFunction> >
      TestingFactories;

  // Helper class for building an instance of TestingProfile (allows injecting
  // mocks for various services prior to profile initialization).
  // TODO(atwilson): Remove non-default constructors and various setters in
  // favor of using the Builder API.
  class Builder {
   public:
    Builder();
    ~Builder();

    // Sets a Delegate to be called back during profile init. This causes the
    // final initialization to be performed via a task so the caller must run
    // a MessageLoop. Caller maintains ownership of the Delegate
    // and must manage its lifetime so it continues to exist until profile
    // initialization is complete.
    void SetDelegate(Delegate* delegate);

    // Adds a testing factory to the TestingProfile. These testing factories
    // are applied before the ProfileKeyedServices are created.
    void AddTestingFactory(
        BrowserContextKeyedServiceFactory* service_factory,
        BrowserContextKeyedServiceFactory::TestingFactoryFunction callback);

#if defined(ENABLE_EXTENSIONS)
    // Sets the ExtensionSpecialStoragePolicy to be returned by
    // GetExtensionSpecialStoragePolicy().
    void SetExtensionSpecialStoragePolicy(
        scoped_refptr<ExtensionSpecialStoragePolicy> policy);
#endif

    // Sets the path to the directory to be used to hold profile data.
    void SetPath(const base::FilePath& path);

    // Sets the PrefService to be used by this profile.
    void SetPrefService(scoped_ptr<PrefServiceSyncable> prefs);

    // Makes the Profile being built a guest profile.
    void SetGuestSession();

    // Sets the supervised user ID (which is empty by default). If it is set to
    // a non-empty string, the profile is supervised.
    void SetSupervisedUserId(const std::string& supervised_user_id);

    // Sets the PolicyService to be used by this profile.
    void SetPolicyService(scoped_ptr<policy::PolicyService> policy_service);

    // Creates the TestingProfile using previously-set settings.
    scoped_ptr<TestingProfile> Build();

    // Build an incognito profile, owned by |original_profile|. Note: unless you
    // need to customize the Builder, or access TestingProfile member functions,
    // you can use original_profile->GetOffTheRecordProfile().
    TestingProfile* BuildIncognito(TestingProfile* original_profile);

   private:
    // If true, Build() has already been called.
    bool build_called_;

    // Various staging variables where values are held until Build() is invoked.
    scoped_ptr<PrefServiceSyncable> pref_service_;
#if defined(ENABLE_EXTENSIONS)
    scoped_refptr<ExtensionSpecialStoragePolicy> extension_policy_;
#endif
    base::FilePath path_;
    Delegate* delegate_;
    bool guest_session_;
    std::string supervised_user_id_;
    scoped_ptr<policy::PolicyService> policy_service_;
    TestingFactories testing_factories_;

    DISALLOW_COPY_AND_ASSIGN(Builder);
  };

  // Multi-profile aware constructor that takes the path to a directory managed
  // for this profile. This constructor is meant to be used by
  // TestingProfileManager::CreateTestingProfile. If you need to create multi-
  // profile profiles, use that factory method instead of this directly.
  // Exception: if you need to create multi-profile profiles for testing the
  // ProfileManager, then use the constructor below instead.
  explicit TestingProfile(const base::FilePath& path);

  // Multi-profile aware constructor that takes the path to a directory managed
  // for this profile and a delegate. This constructor is meant to be used
  // for unittesting the ProfileManager.
  TestingProfile(const base::FilePath& path, Delegate* delegate);

  // Full constructor allowing the setting of all possible instance data.
  // Callers should use Builder::Build() instead of invoking this constructor.
  TestingProfile(const base::FilePath& path,
                 Delegate* delegate,
#if defined(ENABLE_EXTENSIONS)
                 scoped_refptr<ExtensionSpecialStoragePolicy> extension_policy,
#endif
                 scoped_ptr<PrefServiceSyncable> prefs,
                 TestingProfile* parent,
                 bool guest_session,
                 const std::string& supervised_user_id,
                 scoped_ptr<policy::PolicyService> policy_service,
                 const TestingFactories& factories);

  ~TestingProfile() override;

  // Creates the favicon service. Consequent calls would recreate the service.
  void CreateFaviconService();

  // Creates the history service. If |delete_file| is true, the history file is
  // deleted first, then the HistoryService is created. As TestingProfile
  // deletes the directory containing the files used by HistoryService, this
  // only matters if you're recreating the HistoryService.  If |no_db| is true,
  // the history backend will fail to initialize its database; this is useful
  // for testing error conditions. Returns true on success.
  bool CreateHistoryService(bool delete_file, bool no_db) WARN_UNUSED_RESULT;

  // Shuts down and nulls out the reference to HistoryService.
  void DestroyHistoryService();

  // Creates TopSites. This returns immediately, and top sites may not be
  // loaded. Use BlockUntilTopSitesLoaded to ensure TopSites has finished
  // loading.
  void CreateTopSites();

  void DestroyTopSites();

  // Creates the BookmarkBarModel. If not invoked the bookmark bar model is
  // NULL. If |delete_file| is true, the bookmarks file is deleted first, then
  // the model is created. As TestingProfile deletes the directory containing
  // the files used by HistoryService, the boolean only matters if you're
  // recreating the BookmarkModel.
  //
  // NOTE: this does not block until the bookmarks are loaded. For that use
  // WaitForBookmarkModelToLoad().
  void CreateBookmarkModel(bool delete_file);

  // Creates a WebDataService. If not invoked, the web data service is NULL.
  void CreateWebDataService();

  // Blocks until the HistoryService finishes restoring its in-memory cache.
  // This is NOT invoked from CreateHistoryService.
  void BlockUntilHistoryIndexIsRefreshed();

  // Blocks until TopSites finishes loading.
  void BlockUntilTopSitesLoaded();

  // Allow setting a profile as Guest after-the-fact to simplify some tests.
  void SetGuestSession(bool guest);

  TestingPrefServiceSyncable* GetTestingPrefService();

  // Called on the parent of an incognito |profile|. Usually called from the
  // constructor of an incognito TestingProfile, but can also be used by tests
  // to provide an OffTheRecordProfileImpl instance.
  void SetOffTheRecordProfile(scoped_ptr<Profile> profile);

  void SetSupervisedUserId(const std::string& id);

  // content::BrowserContext
  base::FilePath GetPath() const override;
  scoped_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
  scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner() override;
  bool IsOffTheRecord() const override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  net::URLRequestContextGetter* GetRequestContext() override;
  net::URLRequestContextGetter* CreateRequestContext(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  net::URLRequestContextGetter* GetRequestContextForRenderProcess(
      int renderer_child_id) override;
  content::ResourceContext* GetResourceContext() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  content::PermissionManager* GetPermissionManager() override;

  TestingProfile* AsTestingProfile() override;

  // Profile
  std::string GetProfileUserName() const override;
  ProfileType GetProfileType() const override;

  // DEPRECATED, because it's fragile to change a profile from non-incognito
  // to incognito after the ProfileKeyedServices have been created (some
  // ProfileKeyedServices either should not exist in incognito mode, or will
  // crash when they try to get references to other services they depend on,
  // but do not exist in incognito mode).
  // TODO(atwilson): Remove this API (http://crbug.com/277296).
  //
  // Changes a profile's to/from incognito mode temporarily - profile will be
  // returned to non-incognito before destruction to allow services to
  // properly shutdown. This is only supported for legacy tests - new tests
  // should create a true incognito profile using Builder::SetIncognito() or
  // by using the TestingProfile constructor that allows setting the incognito
  // flag.
  void ForceIncognito(bool force_incognito) {
    force_incognito_ = force_incognito;
  }

  Profile* GetOffTheRecordProfile() override;
  void DestroyOffTheRecordProfile() override {}
  bool HasOffTheRecordProfile() override;
  Profile* GetOriginalProfile() override;
  bool IsSupervised() override;
  bool IsChild() override;
  bool IsLegacySupervised() override;
#if defined(ENABLE_EXTENSIONS)
  void SetExtensionSpecialStoragePolicy(
      ExtensionSpecialStoragePolicy* extension_special_storage_policy);
#endif
  ExtensionSpecialStoragePolicy* GetExtensionSpecialStoragePolicy() override;
  // TODO(ajwong): Remove this API in favor of directly retrieving the
  // CookieStore from the StoragePartition after ExtensionURLRequestContext
  // has been removed.
  net::CookieMonster* GetCookieMonster();

  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  chrome::ChromeZoomLevelPrefs* GetZoomLevelPrefs() override;

  net::URLRequestContextGetter* GetMediaRequestContext() override;
  net::URLRequestContextGetter* GetMediaRequestContextForRenderProcess(
      int renderer_child_id) override;
  net::URLRequestContextGetter* GetRequestContextForExtensions() override;
  net::URLRequestContextGetter* GetMediaRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory) override;
  net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  net::SSLConfigService* GetSSLConfigService() override;
  HostContentSettingsMap* GetHostContentSettingsMap() override;
  void set_last_session_exited_cleanly(bool value) {
    last_session_exited_cleanly_ = value;
  }
  bool IsSameProfile(Profile* p) override;
  base::Time GetStartTime() const override;
  base::FilePath last_selected_directory() override;
  void set_last_selected_directory(const base::FilePath& path) override;
  bool WasCreatedByVersionOrLater(const std::string& version) override;
  bool IsGuestSession() const override;
  void SetExitType(ExitType exit_type) override {}
  ExitType GetLastSessionExitType() override;
#if defined(OS_CHROMEOS)
  void ChangeAppLocale(const std::string&, AppLocaleChangedVia) override {}
  void OnLogin() override {}
  void InitChromeOSPreferences() override {}
#endif  // defined(OS_CHROMEOS)

  PrefProxyConfigTracker* GetProxyConfigTracker() override;

  // Schedules a task on the history backend and runs a nested loop until the
  // task is processed.  This has the effect of blocking the caller until the
  // history service processes all pending requests.
  void BlockUntilHistoryProcessesPendingRequests();

  chrome_browser_net::Predictor* GetNetworkPredictor() override;
  DevToolsNetworkController* GetDevToolsNetworkController() override;
  void ClearNetworkingHistorySince(base::Time time,
                                   const base::Closure& completion) override;
  GURL GetHomePage() override;

  PrefService* GetOffTheRecordPrefs() override;

  void set_profile_name(const std::string& profile_name) {
    profile_name_ = profile_name;
  }

 protected:
  base::Time start_time_;
  scoped_ptr<PrefServiceSyncable> prefs_;
  // ref only for right type, lifecycle is managed by prefs_
  TestingPrefServiceSyncable* testing_prefs_;

 private:
  // Creates a temporary directory for use by this profile.
  void CreateTempProfileDir();

  // Common initialization between the two constructors.
  void Init();

  // Finishes initialization when a profile is created asynchronously.
  void FinishInit();

  // Creates a TestingPrefService and associates it with the TestingProfile.
  void CreateTestingPrefService();

  // Initializes |prefs_| for an incognito profile, derived from
  // |original_profile_|.
  void CreateIncognitoPrefService();

  // Creates a ProfilePolicyConnector that the ProfilePolicyConnectorFactory
  // maps to this profile.
  void CreateProfilePolicyConnector();

  // Internally, this is a TestURLRequestContextGetter that creates a dummy
  // request context. Currently, only the CookieMonster is hooked up.
  scoped_refptr<net::URLRequestContextGetter> extensions_request_context_;

  bool force_incognito_;
  scoped_ptr<Profile> incognito_profile_;
  TestingProfile* original_profile_;

  bool guest_session_;

  std::string supervised_user_id_;

  // Did the last session exit cleanly? Default is true.
  bool last_session_exited_cleanly_;

  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

  base::FilePath last_selected_directory_;

#if defined(ENABLE_EXTENSIONS)
  scoped_refptr<ExtensionSpecialStoragePolicy>
      extension_special_storage_policy_;
#endif

  // The proxy prefs tracker.
  scoped_ptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;

  // We use a temporary directory to store testing profile data. In a multi-
  // profile environment, this is invalid and the directory is managed by the
  // TestingProfileManager.
  base::ScopedTempDir temp_dir_;
  // The path to this profile. This will be valid in either of the two above
  // cases.
  base::FilePath profile_path_;

  // We keep a weak pointer to the dependency manager we want to notify on our
  // death. Defaults to the Singleton implementation but overridable for
  // testing.
  BrowserContextDependencyManager* browser_context_dependency_manager_;

  // Owned, but must be deleted on the IO thread so not placing in a
  // scoped_ptr<>.
  content::MockResourceContext* resource_context_;

#if defined(ENABLE_CONFIGURATION_POLICY)
  scoped_ptr<policy::SchemaRegistryService> schema_registry_service_;
#endif
  scoped_ptr<policy::ProfilePolicyConnector> profile_policy_connector_;

  // Weak pointer to a delegate for indicating that a profile was created.
  Delegate* delegate_;

  std::string profile_name_;

  scoped_ptr<policy::PolicyService> policy_service_;
};

#endif  // CHROME_TEST_BASE_TESTING_PROFILE_H_
