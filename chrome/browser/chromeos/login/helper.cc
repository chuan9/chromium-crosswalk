// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/helper.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/guest_view_manager.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/screen.h"

namespace chromeos {

namespace {

// Gets the WebContents instance of current login display. If there is none,
// returns nullptr.
content::WebContents* GetLoginWebContents() {
  LoginDisplayHost* host = LoginDisplayHostImpl::default_host();
  if (!host || !host->GetWebUILoginView())
    return nullptr;

  return host->GetWebUILoginView()->GetWebContents();
}

// Callback used by GetPartition below to return the first guest contents with a
// matching partition name.
bool FindGuestByPartitionName(const std::string& partition_name,
                              content::WebContents** out_guest_contents,
                              content::WebContents* guest_contents) {
  std::string domain;
  std::string name;
  bool in_memory;
  extensions::WebViewGuest::GetGuestPartitionConfigForSite(
      guest_contents->GetSiteInstance()->GetSiteURL(), &domain, &name,
      &in_memory);
  if (partition_name != name)
    return false;

  *out_guest_contents = guest_contents;
  return true;
}

// Gets the storage partition of guest contents of a given embedder.
// If a name is given, returns the partition associated with the name.
// Otherwise, returns the default shared in-memory partition. Returns nullptr if
// a matching partition could not be found.
content::StoragePartition* GetPartition(content::WebContents* embedder,
                                        const std::string& partition_name) {
  extensions::GuestViewManager* manager =
      extensions::GuestViewManager::FromBrowserContext(
          embedder->GetBrowserContext());
  content::WebContents* guest_contents = nullptr;
  manager->ForEachGuest(embedder, base::Bind(&FindGuestByPartitionName,
                                             partition_name, &guest_contents));

  return guest_contents ? content::BrowserContext::GetStoragePartition(
                              guest_contents->GetBrowserContext(),
                              guest_contents->GetSiteInstance())
                        : nullptr;
}

}  // namespace

gfx::Rect CalculateScreenBounds(const gfx::Size& size) {
  gfx::Rect bounds =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().bounds();
  if (!size.IsEmpty()) {
    int horizontal_diff = bounds.width() - size.width();
    int vertical_diff = bounds.height() - size.height();
    bounds.Inset(horizontal_diff / 2, vertical_diff / 2);
  }
  return bounds;
}

int GetCurrentUserImageSize() {
  // The biggest size that the profile picture is displayed at is currently
  // 220px, used for the big preview on OOBE and Change Picture options page.
  static const int kBaseUserImageSize = 220;
  float scale_factor = gfx::Display::GetForcedDeviceScaleFactor();
  if (scale_factor > 1.0f)
    return static_cast<int>(scale_factor * kBaseUserImageSize);
  return kBaseUserImageSize * gfx::ImageSkia::GetMaxSupportedScale();
}

namespace login {

bool LoginScrollIntoViewEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kDisableLoginScrollIntoView);
}

NetworkStateHelper::NetworkStateHelper() {}
NetworkStateHelper::~NetworkStateHelper() {}

base::string16 NetworkStateHelper::GetCurrentNetworkName() const {
  NetworkStateHandler* nsh = NetworkHandler::Get()->network_state_handler();
  const NetworkState* network =
      nsh->ConnectedNetworkByType(NetworkTypePattern::NonVirtual());
  if (network) {
    if (network->Matches(NetworkTypePattern::Ethernet()))
      return l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
    return base::UTF8ToUTF16(network->name());
  }

  network = nsh->ConnectingNetworkByType(NetworkTypePattern::NonVirtual());
  if (network) {
    if (network->Matches(NetworkTypePattern::Ethernet()))
      return l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
    return base::UTF8ToUTF16(network->name());
  }
  return base::string16();
}

bool NetworkStateHelper::IsConnected() const {
  chromeos::NetworkStateHandler* nsh =
      chromeos::NetworkHandler::Get()->network_state_handler();
  return nsh->ConnectedNetworkByType(chromeos::NetworkTypePattern::Default()) !=
         NULL;
}

bool NetworkStateHelper::IsConnecting() const {
  chromeos::NetworkStateHandler* nsh =
      chromeos::NetworkHandler::Get()->network_state_handler();
  return nsh->ConnectingNetworkByType(
      chromeos::NetworkTypePattern::Default()) != NULL;
}

content::StoragePartition* GetSigninPartition() {
  // Note the partition name must match the sign-in webview used. For now,
  // this is the default unnamed, shared, in-memory partition.
  return GetPartition(GetLoginWebContents(), std::string());
}

net::URLRequestContextGetter* GetSigninContext() {
  if (StartupUtils::IsWebviewSigninEnabled())
    return GetSigninPartition()->GetURLRequestContext();

  return ProfileHelper::GetSigninProfile()->GetRequestContext();
}

}  // namespace login

}  // namespace chromeos
