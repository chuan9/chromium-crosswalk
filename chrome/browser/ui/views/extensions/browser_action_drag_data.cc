// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"

#include "base/logging.h"
#include "base/pickle.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/base/clipboard/clipboard.h"

namespace {

// The MIME type for the clipboard format for BrowserActionDragData.
const char kClipboardFormatString[] = "chromium/x-browser-actions";

}

BrowserActionDragData::BrowserActionDragData()
    : profile_(NULL), index_(static_cast<size_t>(-1)) {
}

BrowserActionDragData::BrowserActionDragData(
    const std::string& id, int index)
    : profile_(NULL), id_(id), index_(index) {
}

bool BrowserActionDragData::GetDropFormats(
    std::set<ui::OSExchangeData::CustomFormat>* custom_formats) {
  custom_formats->insert(GetBrowserActionCustomFormat());
  return true;
}

bool BrowserActionDragData::AreDropTypesRequired() {
  return true;
}

bool BrowserActionDragData::CanDrop(const ui::OSExchangeData& data,
                                    const Profile* profile) {
  BrowserActionDragData drop_data;
  return drop_data.Read(data) && drop_data.IsFromProfile(profile);
}

bool BrowserActionDragData::IsFromProfile(const Profile* profile) const {
  return profile_ == profile;
}

#if defined(TOOLKIT_VIEWS)
void BrowserActionDragData::Write(
    Profile* profile, ui::OSExchangeData* data) const {
  DCHECK(data);
  Pickle data_pickle;
  WriteToPickle(profile, &data_pickle);
  data->SetPickledData(GetBrowserActionCustomFormat(), data_pickle);
}

bool BrowserActionDragData::Read(const ui::OSExchangeData& data) {
  if (!data.HasCustomFormat(GetBrowserActionCustomFormat()))
    return false;

  Pickle drag_data_pickle;
  if (!data.GetPickledData(GetBrowserActionCustomFormat(), &drag_data_pickle))
    return false;

  if (!ReadFromPickle(&drag_data_pickle))
    return false;

  return true;
}

// static
const ui::OSExchangeData::CustomFormat&
BrowserActionDragData::GetBrowserActionCustomFormat() {
  CR_DEFINE_STATIC_LOCAL(
      ui::OSExchangeData::CustomFormat,
      format,
      (ui::Clipboard::GetFormatType(kClipboardFormatString)));

  return format;
}
#endif

void BrowserActionDragData::WriteToPickle(
    Profile* profile, base::Pickle* pickle) const {
  pickle->WriteBytes(&profile, sizeof(profile));
  pickle->WriteString(id_);
  pickle->WriteUInt64(index_);
}

bool BrowserActionDragData::ReadFromPickle(Pickle* pickle) {
  PickleIterator data_iterator(*pickle);

  const char* tmp;
  if (!data_iterator.ReadBytes(&tmp, sizeof(profile_)))
    return false;
  memcpy(&profile_, tmp, sizeof(profile_));

  if (!data_iterator.ReadString(&id_))
    return false;

  uint64 index;
  if (!data_iterator.ReadUInt64(&index))
    return false;
  index_ = static_cast<size_t>(index);

  return true;
}
