// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_APP_REMOTING_CONNECTION_HELPER_H_
#define REMOTING_TEST_APP_REMOTING_CONNECTION_HELPER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/test/remote_connection_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class RunLoop;
class Timer;
class Lock;
}

namespace remoting {
namespace test {

struct RemoteApplicationDetails;
class TestChromotingClient;

// Allows for custom handling of ExtensionMessage messages.
typedef base::Callback<void(const protocol::ExtensionMessage& message)>
    HostMessageReceivedCallback;

// Creates a connection to a remote host which is available for tests to use.
// A TestChromotingClient is required from caller.
class AppRemotingConnectionHelper
    : public RemoteConnectionObserver {
 public:
  explicit AppRemotingConnectionHelper(
      const RemoteApplicationDetails& application_details);
  ~AppRemotingConnectionHelper() override;

  // Initialize internal structures.
  void Initialize(scoped_ptr<TestChromotingClient> test_chromoting_client);

  // Starts a connection with the remote host.
  // NOTE: Initialize() must be called before calling this method.
  bool StartConnection();

  // Stubs used to send messages to the remote host. Caller should not release
  // the objects.
  protocol::ClipboardStub* clipboard_forwarder();
  protocol::HostStub* host_stub();
  protocol::InputStub* input_stub();

  // Setter for |host_message_received_callback_|.
  void SetHostMessageReceivedCallback(
      HostMessageReceivedCallback host_message_received_callback);

  // Reset |host_message_received_callback_ to null|.
  void ResetHostMessageReceivedCallback();

 private:
  // RemoteConnectionObserver interface.
  void ConnectionStateChanged(protocol::ConnectionToHost::State state,
                              protocol::ErrorCode error_code) override;
  void ConnectionReady(bool ready) override;
  void HostMessageReceived(const protocol::ExtensionMessage& message) override;

  // Sends client details to the host in order to complete the connection.
  void SendClientConnectionDetailsToHost();

  // Handles onWindowAdded messages from the host.
  void HandleOnWindowAddedMessage(
      const remoting::protocol::ExtensionMessage& message);

  // Contains the details for the application being tested.
  const RemoteApplicationDetails& application_details_;

  // Called when an ExtensionMessage is received from the host.  Used to
  // override default message handling.
  HostMessageReceivedCallback host_message_received_callback_;

  // Indicates whether the remote connection is ready to be used for testing.
  // True when a chromoting connection to the remote host has been established
  // and the main application window is visible.
  bool connection_is_ready_for_tests_;

  // Used to run the thread's message loop.
  scoped_ptr<base::RunLoop> run_loop_;

  // Used for setting timeouts and delays.
  scoped_ptr<base::Timer> timer_;

  // Used to ensure RemoteConnectionObserver methods are called on the same
  // thread.
  base::ThreadChecker thread_checker_;

  // Creates and manages the connection to the remote host.
  scoped_ptr<TestChromotingClient> client_;

  DISALLOW_COPY_AND_ASSIGN(AppRemotingConnectionHelper);
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_APP_REMOTING_CONNECTION_HELPER_H_
