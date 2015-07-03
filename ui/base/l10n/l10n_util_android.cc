// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/l10n_util_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "jni/LocalizationUtils_jni.h"

#if defined(USE_ICU_ALTERNATIVES_ON_ANDROID)
#include "base/icu_alternatives_on_android/icu_utils.h"
#else
#include "third_party/icu/source/common/unicode/uloc.h"
#include "ui/base/l10n/time_format.h"
#endif

namespace l10n_util {

jint GetFirstStrongCharacterDirection(JNIEnv* env, jclass clazz,
                                      jstring string) {
  return base::i18n::GetFirstStrongCharacterDirection(
      base::android::ConvertJavaStringToUTF16(env, string));
}

bool IsLayoutRtl() {
  static bool is_layout_rtl_cached = false;
  static bool layout_rtl_cache;

  if (!is_layout_rtl_cached) {
    is_layout_rtl_cached = true;
    JNIEnv* env = base::android::AttachCurrentThread();
    layout_rtl_cache =
        static_cast<bool>(Java_LocalizationUtils_isLayoutRtl(env));
  }

  return layout_rtl_cache;
}

namespace {

#if !defined(USE_ICU_ALTERNATIVES_ON_ANDROID)
// Common prototype of ICU uloc_getXXX() functions.
typedef int32_t (*UlocGetComponentFunc)(const char*, char*, int32_t,
                                        UErrorCode*);

std::string GetLocaleComponent(const std::string& locale,
                               UlocGetComponentFunc uloc_func,
                               int32_t max_capacity) {
  std::string result;
  UErrorCode error = U_ZERO_ERROR;
  int32_t actual_length = uloc_func(locale.c_str(),
                                    WriteInto(&result, max_capacity),
                                    max_capacity,
                                    &error);
  DCHECK(U_SUCCESS(error));
  DCHECK(actual_length < max_capacity);
  result.resize(actual_length);
  return result;
}

ScopedJavaLocalRef<jobject> NewJavaLocale(
    JNIEnv* env,
    const std::string& locale) {
  // TODO(wangxianzhu): Use new Locale API once Android supports scripts.
  std::string language = GetLocaleComponent(
      locale, uloc_getLanguage, ULOC_LANG_CAPACITY);
  std::string country = GetLocaleComponent(
      locale, uloc_getCountry, ULOC_COUNTRY_CAPACITY);
  std::string variant = GetLocaleComponent(
      locale, uloc_getVariant, ULOC_FULLNAME_CAPACITY);
  return Java_LocalizationUtils_getJavaLocale(env,
          base::android::ConvertUTF8ToJavaString(env, language).obj(),
          base::android::ConvertUTF8ToJavaString(env, country).obj(),
          base::android::ConvertUTF8ToJavaString(env, variant).obj());
}
#endif  // !defined(USE_ICU_ALTERNATIVES_ON_ANDROID)

}  // namespace

#if defined(USE_ICU_ALTERNATIVES_ON_ANDROID)
//Note:Compiler complains Java_Locali_getJavaLocale defined but not used.
void fake_call_getJavaLocale(JNIEnv* env,jstring str) {
  Java_LocalizationUtils_getJavaLocale(env,str,str,str);
}
#endif

base::string16 GetDisplayNameForLocale(const std::string& locale,
                                       const std::string& display_locale) {
  JNIEnv* env = base::android::AttachCurrentThread();
#if defined(USE_ICU_ALTERNATIVES_ON_ANDROID)
  ScopedJavaLocalRef<jobject> java_locale =
      base::icu_utils::getJavaLocale(locale);
  ScopedJavaLocalRef<jobject> java_display_locale =
      base::icu_utils::getJavaLocale(display_locale);
#else
  ScopedJavaLocalRef<jobject> java_locale =
      NewJavaLocale(env, locale);
  ScopedJavaLocalRef<jobject> java_display_locale =
      NewJavaLocale(env, display_locale);
#endif

  ScopedJavaLocalRef<jstring> java_result(
      Java_LocalizationUtils_getDisplayNameForLocale(
          env,
          java_locale.obj(),
          java_display_locale.obj()));
  return ConvertJavaStringToUTF16(java_result);
}

jstring GetDurationString(JNIEnv* env, jclass clazz, jlong timeInMillis) {
#if defined(USE_ICU_ALTERNATIVES_ON_ANDROID)
  // return NULL to use java implementation.
  return NULL;
#else
  ScopedJavaLocalRef<jstring> jtime_remaining =
      base::android::ConvertUTF16ToJavaString(
          env,
          ui::TimeFormat::Simple(
              ui::TimeFormat::FORMAT_REMAINING, ui::TimeFormat::LENGTH_SHORT,
              base::TimeDelta::FromMilliseconds(timeInMillis)));
  return jtime_remaining.Release();
#endif
}

bool RegisterLocalizationUtil(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace l10n_util
