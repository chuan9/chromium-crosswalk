// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/mailbox_manager_sync.h"

#include <algorithm>
#include <queue>

#include "base/memory/linked_ptr.h"
#include "base/synchronization/lock.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_implementation.h"

#if !defined(OS_MACOSX)
#include "ui/gl/gl_fence_egl.h"
#endif

namespace gpu {
namespace gles2 {

namespace {

bool SkipTextureWorkarounds(const Texture* texture) {
  // TODO(sievers): crbug.com/352274
  // Should probably only fail if it already *has* mipmaps, while allowing
  // incomplete textures here.
  bool needs_mips =
      texture->min_filter() != GL_NEAREST && texture->min_filter() != GL_LINEAR;
  if (texture->target() != GL_TEXTURE_2D || needs_mips || !texture->IsDefined())
    return true;

  // Skip compositor resources/tile textures.
  // TODO: Remove this, see crbug.com/399226.
  if (texture->pool() == GL_TEXTURE_POOL_MANAGED_CHROMIUM)
    return true;

  return false;
}

base::LazyInstance<base::Lock> g_lock = LAZY_INSTANCE_INITIALIZER;

typedef std::map<uint32, linked_ptr<gfx::GLFence>> SyncPointToFenceMap;
base::LazyInstance<SyncPointToFenceMap> g_sync_point_to_fence =
    LAZY_INSTANCE_INITIALIZER;
#if !defined(OS_MACOSX)
base::LazyInstance<std::queue<SyncPointToFenceMap::iterator>> g_sync_points =
    LAZY_INSTANCE_INITIALIZER;
#endif

void CreateFenceLocked(uint32 sync_point) {
  g_lock.Get().AssertAcquired();
  if (gfx::GetGLImplementation() == gfx::kGLImplementationMockGL)
    return;

#if !defined(OS_MACOSX)
  std::queue<SyncPointToFenceMap::iterator>& sync_points = g_sync_points.Get();
  SyncPointToFenceMap& sync_point_to_fence = g_sync_point_to_fence.Get();
  if (sync_point) {
    while (!sync_points.empty() &&
           sync_points.front()->second->HasCompleted()) {
      sync_point_to_fence.erase(sync_points.front());
      sync_points.pop();
    }
    // Need to use EGL fences since we are likely not in a single share group.
    linked_ptr<gfx::GLFence> fence(make_linked_ptr(new gfx::GLFenceEGL(true)));
    if (fence.get()) {
      std::pair<SyncPointToFenceMap::iterator, bool> result =
          sync_point_to_fence.insert(std::make_pair(sync_point, fence));
      DCHECK(result.second);
      sync_points.push(result.first);
    }
    DCHECK(sync_points.size() == sync_point_to_fence.size());
  }
#endif
}

void AcquireFenceLocked(uint32 sync_point) {
  g_lock.Get().AssertAcquired();
  SyncPointToFenceMap::iterator fence_it =
      g_sync_point_to_fence.Get().find(sync_point);
  if (fence_it != g_sync_point_to_fence.Get().end()) {
    fence_it->second->ServerWait();
  }
}

static const unsigned kNewTextureVersion = 1;

}  // anonymous namespace

base::LazyInstance<MailboxManagerSync::TextureGroup::MailboxToGroupMap>
    MailboxManagerSync::TextureGroup::mailbox_to_group_ =
        LAZY_INSTANCE_INITIALIZER;

// static
MailboxManagerSync::TextureGroup*
MailboxManagerSync::TextureGroup::CreateFromTexture(TargetName name,
                                                    MailboxManagerSync* manager,
                                                    Texture* texture) {
  TextureGroup* group = new TextureGroup();
  group->AddTexture(manager, texture);
  group->AddName(name);
  if (!SkipTextureWorkarounds(texture)) {
    group->definition_ =
        TextureDefinition(name.first, texture, kNewTextureVersion, NULL);
  }
  return group;
}

// static
MailboxManagerSync::TextureGroup* MailboxManagerSync::TextureGroup::FromName(
    TargetName name) {
  MailboxToGroupMap::iterator it = mailbox_to_group_.Get().find(name);
  if (it == mailbox_to_group_.Get().end())
    return NULL;

  return it->second.get();
}

MailboxManagerSync::TextureGroup::TextureGroup() {
}

MailboxManagerSync::TextureGroup::~TextureGroup() {
}

void MailboxManagerSync::TextureGroup::AddName(TargetName name) {
  g_lock.Get().AssertAcquired();
  DCHECK(std::find(names_.begin(), names_.end(), name) == names_.end());
  names_.push_back(name);
  DCHECK(mailbox_to_group_.Get().find(name) == mailbox_to_group_.Get().end());
  mailbox_to_group_.Get()[name] = this;
}

void MailboxManagerSync::TextureGroup::RemoveName(TargetName name) {
  g_lock.Get().AssertAcquired();
  std::vector<TargetName>::iterator names_it =
      std::find(names_.begin(), names_.end(), name);
  DCHECK(names_it != names_.end());
  names_.erase(names_it);
  MailboxToGroupMap::iterator it = mailbox_to_group_.Get().find(name);
  DCHECK(it != mailbox_to_group_.Get().end());
  mailbox_to_group_.Get().erase(it);
}

void MailboxManagerSync::TextureGroup::AddTexture(MailboxManagerSync* manager,
                                                  Texture* texture) {
  g_lock.Get().AssertAcquired();
  DCHECK(std::find(textures_.begin(), textures_.end(),
                   std::make_pair(manager, texture)) == textures_.end());
  textures_.push_back(std::make_pair(manager, texture));
}

bool MailboxManagerSync::TextureGroup::RemoveTexture(
    MailboxManagerSync* manager,
    Texture* texture) {
  g_lock.Get().AssertAcquired();
  TextureGroup::TextureList::iterator tex_list_it = std::find(
      textures_.begin(), textures_.end(), std::make_pair(manager, texture));
  DCHECK(tex_list_it != textures_.end());
  if (textures_.size() == 1) {
    // This is the last texture so the group is going away.
    for (size_t n = 0; n < names_.size(); n++) {
      const TargetName& target_name = names_[n];
      MailboxToGroupMap::iterator mbox_it =
          mailbox_to_group_.Get().find(target_name);
      DCHECK(mbox_it != mailbox_to_group_.Get().end());
      DCHECK(mbox_it->second.get() == this);
      mailbox_to_group_.Get().erase(mbox_it);
    }
    return false;
  } else {
    textures_.erase(tex_list_it);
    return true;
  }
}

Texture* MailboxManagerSync::TextureGroup::FindTexture(
    MailboxManagerSync* manager) {
  g_lock.Get().AssertAcquired();
  for (TextureGroup::TextureList::iterator it = textures_.begin();
       it != textures_.end(); it++) {
    if (it->first == manager)
      return it->second;
  }
  return NULL;
}

MailboxManagerSync::TextureGroupRef::TextureGroupRef(unsigned version,
                                                     TextureGroup* group)
    : version(version), group(group) {
}

MailboxManagerSync::TextureGroupRef::~TextureGroupRef() {
}

MailboxManagerSync::MailboxManagerSync() {
}

MailboxManagerSync::~MailboxManagerSync() {
  DCHECK_EQ(0U, texture_to_group_.size());
}

bool MailboxManagerSync::UsesSync() {
  return true;
}

Texture* MailboxManagerSync::ConsumeTexture(unsigned target,
                                            const Mailbox& mailbox) {
  base::AutoLock lock(g_lock.Get());
  TargetName target_name(target, mailbox);
  TextureGroup* group = TextureGroup::FromName(target_name);
  if (!group)
    return NULL;

  // Check if a texture already exists in this share group.
  Texture* texture = group->FindTexture(this);
  if (texture)
    return texture;

  // Otherwise create a new texture.
  texture = group->GetDefinition().CreateTexture();
  if (texture) {
    DCHECK(!SkipTextureWorkarounds(texture));
    texture->SetMailboxManager(this);
    group->AddTexture(this, texture);

    TextureGroupRef new_ref =
        TextureGroupRef(group->GetDefinition().version(), group);
    texture_to_group_.insert(std::make_pair(texture, new_ref));
  }

  return texture;
}

void MailboxManagerSync::ProduceTexture(unsigned target,
                                        const Mailbox& mailbox,
                                        Texture* texture) {
  base::AutoLock lock(g_lock.Get());
  TargetName target_name(target, mailbox);

  TextureToGroupMap::iterator tex_it = texture_to_group_.find(texture);
  TextureGroup* group_for_mailbox = TextureGroup::FromName(target_name);
  TextureGroup* group_for_texture = NULL;

  if (tex_it != texture_to_group_.end()) {
    group_for_texture = tex_it->second.group.get();
    DCHECK(group_for_texture);
    if (group_for_mailbox == group_for_texture) {
      // The texture is already known under this name.
      return;
    }
  }

  if (group_for_mailbox) {
    // Unlink the mailbox from its current group.
    group_for_mailbox->RemoveName(target_name);
  }

  if (group_for_texture) {
    group_for_texture->AddName(target_name);
  } else {
    // This is a new texture, so create a new group.
    texture->SetMailboxManager(this);
    group_for_texture =
        TextureGroup::CreateFromTexture(target_name, this, texture);
    texture_to_group_.insert(std::make_pair(
        texture, TextureGroupRef(kNewTextureVersion, group_for_texture)));
  }

  DCHECK(texture->mailbox_manager_ == this);
}

void MailboxManagerSync::TextureDeleted(Texture* texture) {
  base::AutoLock lock(g_lock.Get());
  TextureToGroupMap::iterator tex_it = texture_to_group_.find(texture);
  DCHECK(tex_it != texture_to_group_.end());
  TextureGroup* group_for_texture = tex_it->second.group.get();
  if (group_for_texture->RemoveTexture(this, texture))
    UpdateDefinitionLocked(texture, &tex_it->second);
  texture_to_group_.erase(tex_it);
}

void MailboxManagerSync::UpdateDefinitionLocked(
    Texture* texture,
    TextureGroupRef* group_ref) {
  g_lock.Get().AssertAcquired();

  if (SkipTextureWorkarounds(texture))
    return;

  gfx::GLImage* gl_image = texture->GetLevelImage(texture->target(), 0);
  TextureGroup* group = group_ref->group.get();
  const TextureDefinition& definition = group->GetDefinition();
  scoped_refptr<NativeImageBuffer> image_buffer = definition.image();

  // Make sure we don't clobber with an older version
  if (!definition.IsOlderThan(group_ref->version))
    return;

  // Also don't push redundant updates. Note that it would break the
  // versioning.
  if (definition.Matches(texture))
    return;

  if (gl_image && !image_buffer->IsClient(gl_image)) {
    LOG(ERROR) << "MailboxSync: Incompatible attachment";
    return;
  }

  group->SetDefinition(TextureDefinition(texture->target(), texture,
                                         ++group_ref->version,
                                         gl_image ? image_buffer : NULL));
}

void MailboxManagerSync::PushTextureUpdates(uint32 sync_point) {
  base::AutoLock lock(g_lock.Get());

  for (TextureToGroupMap::iterator it = texture_to_group_.begin();
       it != texture_to_group_.end(); it++) {
    UpdateDefinitionLocked(it->first, &it->second);
  }
  CreateFenceLocked(sync_point);
}

void MailboxManagerSync::PullTextureUpdates(uint32 sync_point) {
  base::AutoLock lock(g_lock.Get());
  AcquireFenceLocked(sync_point);

  for (TextureToGroupMap::iterator it = texture_to_group_.begin();
       it != texture_to_group_.end(); it++) {
    const TextureDefinition& definition = it->second.group->GetDefinition();
    Texture* texture = it->first;
    unsigned& texture_version = it->second.version;
    if (texture_version == definition.version() ||
        definition.IsOlderThan(texture_version))
      continue;
    texture_version = definition.version();
    definition.UpdateTexture(texture);
  }
}

}  // namespace gles2
}  // namespace gpu
