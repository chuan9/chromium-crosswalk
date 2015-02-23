// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/power_policy_controller.h"

#include <utility>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace chromeos {

namespace {

PowerPolicyController* g_power_policy_controller = nullptr;

// Appends a description of |field|, a field within |delays|, a
// power_manager::PowerManagementPolicy::Delays object, to |str|, an
// std::string, if the field is set.  |name| is a char* describing the
// field.
#define APPEND_DELAY(str, delays, field, name)                                 \
    {                                                                          \
      if (delays.has_##field())                                                \
        str += base::StringPrintf(name "=%" PRId64 " ", delays.field());       \
    }

// Appends descriptions of all of the set delays in |delays|, a
// power_manager::PowerManagementPolicy::Delays object, to |str|, an
// std::string.  |prefix| should be a char* containing either "ac" or
// "battery".
#define APPEND_DELAYS(str, delays, prefix)                                     \
    {                                                                          \
      APPEND_DELAY(str, delays, screen_dim_ms, prefix "_screen_dim_ms");       \
      APPEND_DELAY(str, delays, screen_off_ms, prefix "_screen_off_ms");       \
      APPEND_DELAY(str, delays, screen_lock_ms, prefix "_screen_lock_ms");     \
      APPEND_DELAY(str, delays, idle_warning_ms, prefix "_idle_warning_ms");   \
      APPEND_DELAY(str, delays, idle_ms, prefix "_idle_ms");                   \
    }

// Returns the power_manager::PowerManagementPolicy_Action value
// corresponding to |action|.
power_manager::PowerManagementPolicy_Action GetProtoAction(
    PowerPolicyController::Action action) {
  switch (action) {
    case PowerPolicyController::ACTION_SUSPEND:
      return power_manager::PowerManagementPolicy_Action_SUSPEND;
    case PowerPolicyController::ACTION_STOP_SESSION:
      return power_manager::PowerManagementPolicy_Action_STOP_SESSION;
    case PowerPolicyController::ACTION_SHUT_DOWN:
      return power_manager::PowerManagementPolicy_Action_SHUT_DOWN;
    case PowerPolicyController::ACTION_DO_NOTHING:
      return power_manager::PowerManagementPolicy_Action_DO_NOTHING;
    default:
      NOTREACHED() << "Unhandled action " << action;
      return power_manager::PowerManagementPolicy_Action_DO_NOTHING;
  }
}

// Returns false if |use_audio_activity| and |use_audio_activity| prevent wake
// locks created for |reason| from being honored or true otherwise.
bool IsWakeLockReasonHonored(PowerPolicyController::WakeLockReason reason,
                             bool use_audio_activity,
                             bool use_video_activity) {
  if (reason == PowerPolicyController::REASON_AUDIO_PLAYBACK &&
      !use_audio_activity)
    return false;
  if (reason == PowerPolicyController::REASON_VIDEO_PLAYBACK &&
      !use_video_activity)
    return false;
  return true;
}

}  // namespace

const int PowerPolicyController::kScreenLockAfterOffDelayMs = 10000;  // 10 sec.
const char PowerPolicyController::kPrefsReason[] = "Prefs";

// -1 is interpreted as "unset" by powerd, resulting in powerd's default
// delays being used instead.  There are no similarly-interpreted values
// for the other fields, unfortunately (but the constructor-assigned values
// will only reach powerd if Chrome messes up and forgets to override them
// with the pref-assigned values).
PowerPolicyController::PrefValues::PrefValues()
    : ac_screen_dim_delay_ms(-1),
      ac_screen_off_delay_ms(-1),
      ac_screen_lock_delay_ms(-1),
      ac_idle_warning_delay_ms(-1),
      ac_idle_delay_ms(-1),
      battery_screen_dim_delay_ms(-1),
      battery_screen_off_delay_ms(-1),
      battery_screen_lock_delay_ms(-1),
      battery_idle_warning_delay_ms(-1),
      battery_idle_delay_ms(-1),
      ac_idle_action(ACTION_SUSPEND),
      battery_idle_action(ACTION_SUSPEND),
      lid_closed_action(ACTION_SUSPEND),
      use_audio_activity(true),
      use_video_activity(true),
      ac_brightness_percent(-1.0),
      battery_brightness_percent(-1.0),
      allow_screen_wake_locks(true),
      enable_auto_screen_lock(false),
      presentation_screen_dim_delay_factor(1.0),
      user_activity_screen_dim_delay_factor(1.0),
      wait_for_initial_user_activity(false),
      force_nonzero_brightness_for_user_activity(true) {}

// static
std::string PowerPolicyController::GetPolicyDebugString(
    const power_manager::PowerManagementPolicy& policy) {
  std::string str;
  if (policy.has_ac_delays())
    APPEND_DELAYS(str, policy.ac_delays(), "ac");
  if (policy.has_battery_delays())
    APPEND_DELAYS(str, policy.battery_delays(), "battery");
  if (policy.has_ac_idle_action())
    str += base::StringPrintf("ac_idle=%d ", policy.ac_idle_action());
  if (policy.has_battery_idle_action())
    str += base::StringPrintf("battery_idle=%d ", policy.battery_idle_action());
  if (policy.has_lid_closed_action())
    str += base::StringPrintf("lid_closed=%d ", policy.lid_closed_action());
  if (policy.has_use_audio_activity())
    str += base::StringPrintf("use_audio=%d ", policy.use_audio_activity());
  if (policy.has_use_video_activity())
    str += base::StringPrintf("use_video=%d ", policy.use_audio_activity());
  if (policy.has_ac_brightness_percent()) {
    str += base::StringPrintf("ac_brightness_percent=%f ",
        policy.ac_brightness_percent());
  }
  if (policy.has_battery_brightness_percent()) {
    str += base::StringPrintf("battery_brightness_percent=%f ",
        policy.battery_brightness_percent());
  }
  if (policy.has_presentation_screen_dim_delay_factor()) {
    str += base::StringPrintf("presentation_screen_dim_delay_factor=%f ",
        policy.presentation_screen_dim_delay_factor());
  }
  if (policy.has_user_activity_screen_dim_delay_factor()) {
    str += base::StringPrintf("user_activity_screen_dim_delay_factor=%f ",
        policy.user_activity_screen_dim_delay_factor());
  }
  if (policy.has_wait_for_initial_user_activity()) {
    str += base::StringPrintf("wait_for_initial_user_activity=%d ",
        policy.wait_for_initial_user_activity());
  }
  if (policy.has_force_nonzero_brightness_for_user_activity()) {
    str += base::StringPrintf("force_nonzero_brightness_for_user_activity=%d ",
        policy.force_nonzero_brightness_for_user_activity());
  }
  if (policy.has_reason())
    str += base::StringPrintf("reason=\"%s\" ", policy.reason().c_str());
  base::TrimWhitespace(str, base::TRIM_TRAILING, &str);
  return str;
}

// static
void PowerPolicyController::Initialize(PowerManagerClient* client) {
  DCHECK(!IsInitialized());
  g_power_policy_controller = new PowerPolicyController(client);
}

// static
bool PowerPolicyController::IsInitialized() {
  return g_power_policy_controller;
}

// static
void PowerPolicyController::Shutdown() {
  DCHECK(IsInitialized());
  delete g_power_policy_controller;
  g_power_policy_controller = nullptr;
}

// static
PowerPolicyController* PowerPolicyController::Get() {
  DCHECK(IsInitialized());
  return g_power_policy_controller;
}

void PowerPolicyController::ApplyPrefs(const PrefValues& values) {
  prefs_policy_.Clear();

  power_manager::PowerManagementPolicy::Delays* delays =
      prefs_policy_.mutable_ac_delays();
  delays->set_screen_dim_ms(values.ac_screen_dim_delay_ms);
  delays->set_screen_off_ms(values.ac_screen_off_delay_ms);
  delays->set_screen_lock_ms(values.ac_screen_lock_delay_ms);
  delays->set_idle_warning_ms(values.ac_idle_warning_delay_ms);
  delays->set_idle_ms(values.ac_idle_delay_ms);

  // If auto screen-locking is enabled, ensure that the screen is locked soon
  // after it's turned off due to user inactivity.
  int64 lock_ms = delays->screen_off_ms() + kScreenLockAfterOffDelayMs;
  if (values.enable_auto_screen_lock && delays->screen_off_ms() > 0 &&
      (delays->screen_lock_ms() <= 0 || lock_ms < delays->screen_lock_ms()) &&
      lock_ms < delays->idle_ms()) {
    delays->set_screen_lock_ms(lock_ms);
  }

  delays = prefs_policy_.mutable_battery_delays();
  delays->set_screen_dim_ms(values.battery_screen_dim_delay_ms);
  delays->set_screen_off_ms(values.battery_screen_off_delay_ms);
  delays->set_screen_lock_ms(values.battery_screen_lock_delay_ms);
  delays->set_idle_warning_ms(values.battery_idle_warning_delay_ms);
  delays->set_idle_ms(values.battery_idle_delay_ms);

  lock_ms = delays->screen_off_ms() + kScreenLockAfterOffDelayMs;
  if (values.enable_auto_screen_lock && delays->screen_off_ms() > 0 &&
      (delays->screen_lock_ms() <= 0 || lock_ms < delays->screen_lock_ms()) &&
      lock_ms < delays->idle_ms()) {
    delays->set_screen_lock_ms(lock_ms);
  }

  prefs_policy_.set_ac_idle_action(GetProtoAction(values.ac_idle_action));
  prefs_policy_.set_battery_idle_action(
      GetProtoAction(values.battery_idle_action));
  prefs_policy_.set_lid_closed_action(GetProtoAction(values.lid_closed_action));
  prefs_policy_.set_use_audio_activity(values.use_audio_activity);
  prefs_policy_.set_use_video_activity(values.use_video_activity);
  if (values.ac_brightness_percent >= 0.0)
    prefs_policy_.set_ac_brightness_percent(values.ac_brightness_percent);
  if (values.battery_brightness_percent >= 0.0) {
    prefs_policy_.set_battery_brightness_percent(
        values.battery_brightness_percent);
  }
  prefs_policy_.set_presentation_screen_dim_delay_factor(
      values.presentation_screen_dim_delay_factor);
  prefs_policy_.set_user_activity_screen_dim_delay_factor(
      values.user_activity_screen_dim_delay_factor);
  prefs_policy_.set_wait_for_initial_user_activity(
      values.wait_for_initial_user_activity);
  prefs_policy_.set_force_nonzero_brightness_for_user_activity(
      values.force_nonzero_brightness_for_user_activity);

  honor_screen_wake_locks_ = values.allow_screen_wake_locks;

  prefs_were_set_ = true;
  SendCurrentPolicy();
}

int PowerPolicyController::AddScreenWakeLock(WakeLockReason reason,
                                             const std::string& description) {
  return AddWakeLockInternal(WakeLock::TYPE_SCREEN, reason, description);
}

int PowerPolicyController::AddSystemWakeLock(WakeLockReason reason,
                                             const std::string& description) {
  return AddWakeLockInternal(WakeLock::TYPE_SYSTEM, reason, description);
}

void PowerPolicyController::RemoveWakeLock(int id) {
  if (!wake_locks_.erase(id))
    LOG(WARNING) << "Ignoring request to remove nonexistent wake lock " << id;
  else
    SendCurrentPolicy();
}

void PowerPolicyController::PowerManagerRestarted() {
  SendCurrentPolicy();
}

PowerPolicyController::PowerPolicyController(PowerManagerClient* client)
    : client_(client),
      prefs_were_set_(false),
      honor_screen_wake_locks_(true),
      next_wake_lock_id_(1) {
  DCHECK(client_);
  client_->AddObserver(this);
}

PowerPolicyController::~PowerPolicyController() {
  client_->RemoveObserver(this);
}

PowerPolicyController::WakeLock::WakeLock(Type type,
                                          WakeLockReason reason,
                                          const std::string& description)
    : type(type), reason(reason), description(description) {
}

PowerPolicyController::WakeLock::~WakeLock() {
}

int PowerPolicyController::AddWakeLockInternal(WakeLock::Type type,
                                               WakeLockReason reason,
                                               const std::string& description) {
  const int id = next_wake_lock_id_++;
  wake_locks_.insert(std::make_pair(id, WakeLock(type, reason, description)));
  SendCurrentPolicy();
  return id;
}

void PowerPolicyController::SendCurrentPolicy() {
  std::string causes;

  power_manager::PowerManagementPolicy policy = prefs_policy_;
  if (prefs_were_set_)
    causes = kPrefsReason;

  bool have_screen_wake_locks = false;
  bool have_system_wake_locks = false;
  for (const auto& it : wake_locks_) {
    // Skip audio and video locks that should be ignored due to policy.
    if (!IsWakeLockReasonHonored(it.second.reason, policy.use_audio_activity(),
                                 policy.use_video_activity()))
      continue;

    switch (it.second.type) {
      case WakeLock::TYPE_SCREEN:
        have_screen_wake_locks = true;
        break;
      case WakeLock::TYPE_SYSTEM:
        have_system_wake_locks = true;
        break;
    }
    causes += (causes.empty() ? "" : ", ") + it.second.description;
  }

  if (honor_screen_wake_locks_ && have_screen_wake_locks) {
    policy.mutable_ac_delays()->set_screen_dim_ms(0);
    policy.mutable_ac_delays()->set_screen_off_ms(0);
    policy.mutable_ac_delays()->set_screen_lock_ms(0);
    policy.mutable_battery_delays()->set_screen_dim_ms(0);
    policy.mutable_battery_delays()->set_screen_off_ms(0);
    policy.mutable_battery_delays()->set_screen_lock_ms(0);
  }

  if (have_screen_wake_locks || have_system_wake_locks) {
    if (!policy.has_ac_idle_action() || policy.ac_idle_action() ==
        power_manager::PowerManagementPolicy_Action_SUSPEND) {
      policy.set_ac_idle_action(
          power_manager::PowerManagementPolicy_Action_DO_NOTHING);
    }
    if (!policy.has_battery_idle_action() || policy.battery_idle_action() ==
        power_manager::PowerManagementPolicy_Action_SUSPEND) {
      policy.set_battery_idle_action(
          power_manager::PowerManagementPolicy_Action_DO_NOTHING);
    }
  }

  if (!causes.empty())
    policy.set_reason(causes);
  client_->SetPolicy(policy);
}

}  // namespace chromeos
