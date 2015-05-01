// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/mirror_window_controller.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>

// Xlib.h defines RootWindow.
#undef RootWindow
#endif

#include "ash/display/cursor_window_controller.h"
#include "ash/display/display_controller.h"
#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/display/root_window_transformers.h"
#include "ash/display/screen_position_controller.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/host/ash_window_tree_host_init_params.h"
#include "ash/host/root_window_transformer.h"
#include "ash/root_window_settings.h"
#include "ash/shell.h"
#include "base/strings/stringprintf.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/layout.h"
#include "ui/compositor/reflector.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/native_widget_types.h"

#if defined(USE_X11)
#include "ui/gfx/x/x11_types.h"
#endif

namespace ash {
namespace {

#if defined(USE_X11)
// Mirror window shouldn't handle input events.
void DisableInput(XID window) {
  long event_mask = ExposureMask | VisibilityChangeMask |
      StructureNotifyMask | PropertyChangeMask;
  XSelectInput(gfx::GetXDisplay(), window, event_mask);
  unsigned char mask[XIMaskLen(XI_LASTEVENT)] = {0};
  XIEventMask evmask;
  evmask.deviceid = XIAllDevices;
  evmask.mask_len = sizeof(mask);
  evmask.mask = mask;
  XISelectEvents(gfx::GetXDisplay(), window, &evmask, 1);
}
#endif

// ScreenPositionClient for mirroring windows.
class MirroringScreenPositionClient
    : public aura::client::ScreenPositionClient {
 public:
  explicit MirroringScreenPositionClient(MirrorWindowController* controller)
      : controller_(controller) {}

  void ConvertPointToScreen(const aura::Window* window,
                            gfx::Point* point) override {
    const aura::Window* root = window->GetRootWindow();
    aura::Window::ConvertPointToTarget(window, root, point);
    const gfx::Display& display = controller_->GetDisplayForRootWindow(root);
    const gfx::Point display_origin = display.bounds().origin();
    point->Offset(display_origin.x(), display_origin.y());
  }

  void ConvertPointFromScreen(const aura::Window* window,
                              gfx::Point* point) override {
    const aura::Window* root = window->GetRootWindow();
    const gfx::Display& display = controller_->GetDisplayForRootWindow(root);
    const gfx::Point display_origin = display.bounds().origin();
    point->Offset(-display_origin.x(), -display_origin.y());
    aura::Window::ConvertPointToTarget(root, window, point);
  }

  void ConvertHostPointToScreen(aura::Window* root_window,
                                gfx::Point* point) override {
    aura::Window* not_used;
    ScreenPositionController::ConvertHostPointToRelativeToRootWindow(
        root_window, controller_->GetAllRootWindows(), point, &not_used);
    ConvertPointToScreen(root_window, point);
  }

  void SetBounds(aura::Window* window,
                 const gfx::Rect& bounds,
                 const gfx::Display& display) override {
    NOTREACHED();
  }

 private:
  MirrorWindowController* controller_;  // not owned.

  DISALLOW_COPY_AND_ASSIGN(MirroringScreenPositionClient);
};

class NoneCaptureClient : public aura::client::CaptureClient {
 public:
  NoneCaptureClient() {}
  ~NoneCaptureClient() override {}

 private:
  // Does a capture on the |window|.
  void SetCapture(aura::Window* window) override {}

  // Releases a capture from the |window|.
  void ReleaseCapture(aura::Window* window) override {}

  // Returns the current capture window.
  aura::Window* GetCaptureWindow() override { return NULL; }
  aura::Window* GetGlobalCaptureWindow() override { return NULL; }

  DISALLOW_COPY_AND_ASSIGN(NoneCaptureClient);
};

}  // namespace

struct MirrorWindowController::MirroringHostInfo {
  MirroringHostInfo();
  ~MirroringHostInfo();
  scoped_ptr<AshWindowTreeHost> ash_host;
  gfx::Size mirror_window_host_size;
  aura::Window* mirror_window = nullptr;
};

MirrorWindowController::MirroringHostInfo::MirroringHostInfo() {
}
MirrorWindowController::MirroringHostInfo::~MirroringHostInfo() {
}

MirrorWindowController::MirrorWindowController()
    : screen_position_client_(new MirroringScreenPositionClient(this)) {
}

MirrorWindowController::~MirrorWindowController() {
  // Make sure the root window gets deleted before cursor_window_delegate.
  Close();
}

void MirrorWindowController::UpdateWindow(
    const std::vector<DisplayInfo>& display_info_list) {
  static int mirror_host_count = 0;
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  const gfx::Display& primary = Shell::GetScreen()->GetPrimaryDisplay();
  const DisplayInfo& source_display_info =
      display_manager->GetDisplayInfo(primary.id());

  gfx::Point mirroring_origin;
  for (const DisplayInfo& display_info : display_info_list) {
    if (mirroring_host_info_map_.find(display_info.id()) ==
        mirroring_host_info_map_.end()) {
      AshWindowTreeHostInitParams init_params;
      init_params.initial_bounds = display_info.bounds_in_native();
      MirroringHostInfo* host_info = new MirroringHostInfo;
      host_info->ash_host.reset(AshWindowTreeHost::Create(init_params));
      mirroring_host_info_map_[display_info.id()] = host_info;

      aura::WindowTreeHost* host = host_info->ash_host->AsWindowTreeHost();
      host->window()->SetName(
          base::StringPrintf("MirrorRootWindow-%d", mirror_host_count++));
      host->compositor()->SetBackgroundColor(SK_ColorBLACK);
      // No need to remove the observer because the DisplayController outlives
      // the
      // host.
      host->AddObserver(Shell::GetInstance()->display_controller());
      host->AddObserver(this);
      // TODO(oshima): TouchHUD is using idkey.
      InitRootWindowSettings(host->window())->display_id = display_info.id();
      host->InitHost();
#if defined(USE_X11)
      if (!display_manager->IsInUnifiedMode())
        DisableInput(host->GetAcceleratedWidget());
#endif

#if defined(OS_CHROMEOS)
      if (display_manager->IsInUnifiedMode()) {
        host_info->ash_host->ConfineCursorToRootWindow();
        AshWindowTreeHost* unified_ash_host =
            Shell::GetInstance()
                ->display_controller()
                ->GetAshWindowTreeHostForDisplayId(
                    Shell::GetScreen()->GetPrimaryDisplay().id());
        unified_ash_host->RegisterMirroringHost(host_info->ash_host.get());
        aura::client::SetScreenPositionClient(host->window(),
                                              screen_position_client_.get());
      }
#endif

      aura::client::SetCaptureClient(host->window(), new NoneCaptureClient());
      host->Show();

      aura::Window* mirror_window = host_info->mirror_window =
          new aura::Window(nullptr);
      mirror_window->Init(ui::LAYER_SOLID_COLOR);
      host->window()->AddChild(mirror_window);
      mirror_window->SetBounds(host->window()->bounds());
      mirror_window->Show();
      if (reflector_) {
        reflector_->AddMirroringLayer(mirror_window->layer());
      } else {
        reflector_ =
            aura::Env::GetInstance()->context_factory()->CreateReflector(
                Shell::GetPrimaryRootWindow()->GetHost()->compositor(),
                mirror_window->layer());
      }
    } else {
      aura::WindowTreeHost* host = mirroring_host_info_map_[display_info.id()]
                                       ->ash_host->AsWindowTreeHost();
      GetRootWindowSettings(host->window())->display_id = display_info.id();
      host->SetBounds(display_info.bounds_in_native());
    }

    AshWindowTreeHost* mirroring_ash_host =
        mirroring_host_info_map_[display_info.id()]->ash_host.get();
    if (display_manager->IsInMirrorMode()) {
      scoped_ptr<RootWindowTransformer> transformer(
          CreateRootWindowTransformerForMirroredDisplay(source_display_info,
                                                        display_info));
      mirroring_ash_host->SetRootWindowTransformer(transformer.Pass());
    } else if (display_manager->IsInUnifiedMode()) {
      gfx::Display display;
      display.SetScaleAndBounds(
          1.0f,
          gfx::Rect(mirroring_origin, display_info.bounds_in_native().size()));
      mirroring_origin.SetPoint(display.bounds().right(), 0);
      scoped_ptr<RootWindowTransformer> transformer(
          CreateRootWindowTransformerForUnifiedDesktop(primary.bounds(),
                                                       display));
      mirroring_ash_host->SetRootWindowTransformer(transformer.Pass());
    } else {
      NOTREACHED();
    }
  }

  // Deleting WTHs for disconnected displays.
  if (mirroring_host_info_map_.size() > display_info_list.size()) {
    for (MirroringHostInfoMap::iterator iter = mirroring_host_info_map_.begin();
         iter != mirroring_host_info_map_.end();) {
      if (std::find_if(display_info_list.begin(), display_info_list.end(),
                       [iter](const DisplayInfo& info) {
                         return info.id() == iter->first;
                       }) == display_info_list.end()) {
        CloseAndDeleteHost(iter->second);
        iter = mirroring_host_info_map_.erase(iter);
      } else {
        ++iter;
      }
    }
  }
}

void MirrorWindowController::UpdateWindow() {
  if (!mirroring_host_info_map_.size())
    return;
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  std::vector<DisplayInfo> display_info_list;
  for (auto& pair : mirroring_host_info_map_)
    display_info_list.push_back(display_manager->GetDisplayInfo(pair.first));
  UpdateWindow(display_info_list);
}

void MirrorWindowController::Close() {
  for (auto& info : mirroring_host_info_map_) {
    CloseAndDeleteHost(info.second);
  }
  mirroring_host_info_map_.clear();
  if (reflector_) {
    aura::Env::GetInstance()->context_factory()->RemoveReflector(
        reflector_.get());
    reflector_.reset();
  }
}

void MirrorWindowController::OnHostResized(const aura::WindowTreeHost* host) {
  for (auto& pair : mirroring_host_info_map_) {
    MirroringHostInfo* info = pair.second;
    if (info->ash_host->AsWindowTreeHost() == host) {
      if (info->mirror_window_host_size == host->GetBounds().size())
        return;
      info->mirror_window_host_size = host->GetBounds().size();
      reflector_->OnMirroringCompositorResized();
      info->ash_host->SetRootWindowTransformer(
          CreateRootWindowTransformer().Pass());
      Shell::GetInstance()
          ->display_controller()
          ->cursor_window_controller()
          ->UpdateLocation();
      return;
    }
  }
}

aura::Window* MirrorWindowController::GetWindow() {
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  // TODO(oshima): Support software cursor in Unified Mode.
  if (!display_manager->IsInMirrorMode() || mirroring_host_info_map_.empty())
    return nullptr;
  DCHECK_EQ(1U, mirroring_host_info_map_.size());
  return mirroring_host_info_map_.begin()
      ->second->ash_host->AsWindowTreeHost()
      ->window();
}

gfx::Display MirrorWindowController::GetDisplayForRootWindow(
    const aura::Window* root) const {
  for (const auto& pair : mirroring_host_info_map_) {
    if (pair.second->ash_host->AsWindowTreeHost()->window() == root) {
      // Sanity check to catch an error early.
      int64 id = pair.first;
      const DisplayManager::DisplayList& list =
          Shell::GetInstance()
              ->display_manager()
              ->software_mirroring_display_list();
      auto iter = std::find_if(
          list.begin(), list.end(),
          [id](const gfx::Display& display) { return display.id() == id; });
      DCHECK(iter != list.end());
      if (iter != list.end())
        return *iter;
    }
  }
  return gfx::Display();
}

AshWindowTreeHost* MirrorWindowController::GetAshWindowTreeHostForDisplayId(
    int64 id) {
  CHECK_EQ(1u, mirroring_host_info_map_.count(id));
  return mirroring_host_info_map_[id]->ash_host.get();
}

aura::Window::Windows MirrorWindowController::GetAllRootWindows() const {
  aura::Window::Windows root_windows;
  for (const auto& pair : mirroring_host_info_map_)
    root_windows.push_back(pair.second->ash_host->AsWindowTreeHost()->window());
  return root_windows;
}

void MirrorWindowController::CloseAndDeleteHost(MirroringHostInfo* host_info) {
  aura::WindowTreeHost* host = host_info->ash_host->AsWindowTreeHost();

  aura::client::SetScreenPositionClient(host->window(), nullptr);

  NoneCaptureClient* capture_client = static_cast<NoneCaptureClient*>(
      aura::client::GetCaptureClient(host->window()));
  aura::client::SetCaptureClient(host->window(), nullptr);
  delete capture_client;

  host->RemoveObserver(Shell::GetInstance()->display_controller());
  host->RemoveObserver(this);
  host_info->ash_host->PrepareForShutdown();
  reflector_->RemoveMirroringLayer(host_info->mirror_window->layer());

  delete host_info;
}

scoped_ptr<RootWindowTransformer>
MirrorWindowController::CreateRootWindowTransformer() const {
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  const DisplayInfo& mirror_display_info =
      display_manager->GetDisplayInfo(display_manager->mirroring_display_id());
  const DisplayInfo& source_display_info = display_manager->GetDisplayInfo(
      Shell::GetScreen()->GetPrimaryDisplay().id());
  DCHECK(display_manager->IsInMirrorMode());
  return scoped_ptr<RootWindowTransformer>(
      CreateRootWindowTransformerForMirroredDisplay(source_display_info,
                                                    mirror_display_info));
}

}  // namespace ash
