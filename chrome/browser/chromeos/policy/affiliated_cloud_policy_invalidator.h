// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_AFFILIATED_CLOUD_POLICY_INVALIDATOR_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_AFFILIATED_CLOUD_POLICY_INVALIDATOR_H_

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/policy/affiliated_invalidation_service_provider.h"
#include "policy/proto/device_management_backend.pb.h"

namespace invalidation {
class InvalidationService;
}

namespace policy {

class CloudPolicyCore;
class CloudPolicyInvalidator;

// This class provides policy invalidations for a |CloudPolicyCore| that wishes
// to use a shared |InvalidationService| affiliated with the device's enrollment
// domain. This is the case e.g. for device policy and device-local account
// policy.
// It relies on an |AffiliatedInvalidationServiceProvider| to provide it with
// access to a shared |InvalidationService| to back the
// |CloudPolicyInvalidator|. Whenever the shared |InvalidationService| changes,
// the |CloudPolicyInvalidator| is destroyed and re-created.
class AffiliatedCloudPolicyInvalidator
    : public AffiliatedInvalidationServiceProvider::Consumer {
 public:
  AffiliatedCloudPolicyInvalidator(
      enterprise_management::DeviceRegisterRequest::Type type,
      CloudPolicyCore* core,
      AffiliatedInvalidationServiceProvider* invalidation_service_provider);
  ~AffiliatedCloudPolicyInvalidator() override;

  // AffiliatedInvalidationServiceProvider::Consumer:
  void OnInvalidationServiceSet(
      invalidation::InvalidationService* invalidation_service) override;

  CloudPolicyInvalidator* GetInvalidatorForTest() const;

 private:
  // Create a |CloudPolicyInvalidator| backed by the |invalidation_service|.
  void CreateInvalidator(
      invalidation::InvalidationService* invalidation_service);

  // Destroy the current |CloudPolicyInvalidator|, if any.
  void DestroyInvalidator();

  const enterprise_management::DeviceRegisterRequest::Type type_;
  CloudPolicyCore* const core_;

  AffiliatedInvalidationServiceProvider* const invalidation_service_provider_;

  // The highest invalidation version that was handled already.
  int64 highest_handled_invalidation_version_;

  // The current |CloudPolicyInvalidator|. nullptr if no connected invalidation
  // service is available.
  scoped_ptr<CloudPolicyInvalidator> invalidator_;

  DISALLOW_COPY_AND_ASSIGN(AffiliatedCloudPolicyInvalidator);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_AFFILIATED_CLOUD_POLICY_INVALIDATOR_H_
