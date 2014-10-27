// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/handlers/audio/audio_directive_handler.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/timer/mock_timer.h"
#include "components/copresence/handlers/audio/tick_clock_ref_counted.h"
#include "components/copresence/mediums/audio/audio_manager.h"
#include "components/copresence/test/audio_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace copresence {

namespace {

// Callback stubs to pass into the directive handler.
void DecodeSamples(AudioType, const std::string&) {
}
void EncodeToken(const std::string&,
                 AudioType,
                 const AudioManager::SamplesCallback&) {
}

}  // namespace

class AudioManagerStub final : public AudioManager {
 public:
  AudioManagerStub() {}
  virtual ~AudioManagerStub() {}

  // AudioManager overrides:
  void Initialize(const DecodeSamplesCallback& decode_cb,
                  const EncodeTokenCallback& encode_cb) override {}
  void StartPlaying(AudioType type) override { playing_[type] = true; }
  void StopPlaying(AudioType type) override { playing_[type] = false; }
  void StartRecording(AudioType type) override { recording_[type] = true; }
  void StopRecording(AudioType type) override { recording_[type] = false; }
  void SetToken(AudioType type, const std::string& url_unsafe_token) override {}
  const std::string GetToken(AudioType type) override { return std::string(); }
  bool IsRecording(AudioType type) override { return recording_[type]; }
  bool IsPlaying(AudioType type) override { return playing_[type]; }

 private:
  // Indexed using enum AudioType.
  bool playing_[2];
  bool recording_[2];

  DISALLOW_COPY_AND_ASSIGN(AudioManagerStub);
};

class AudioDirectiveHandlerTest : public testing::Test {
 public:
  AudioDirectiveHandlerTest()
      : directive_handler_(new AudioDirectiveHandler()) {
    scoped_ptr<AudioManagerStub> manager(new AudioManagerStub);
    manager_ptr_ = manager.get();
    directive_handler_->set_audio_manager_for_testing(manager.Pass());
    directive_handler_->Initialize(base::Bind(&DecodeSamples),
                                   base::Bind(&EncodeToken));
  }
  virtual ~AudioDirectiveHandlerTest() {}

  void DirectiveAdded() {}

 protected:
  copresence::TokenInstruction CreateTransmitInstruction(
      const std::string& token,
      bool audible) {
    copresence::TokenInstruction instruction;
    instruction.set_token_instruction_type(copresence::TRANSMIT);
    instruction.set_token_id(token);
    instruction.set_medium(audible ? AUDIO_AUDIBLE_DTMF
                                   : AUDIO_ULTRASOUND_PASSBAND);
    return instruction;
  }

  copresence::TokenInstruction CreateReceiveInstruction(bool audible) {
    copresence::TokenInstruction instruction;
    instruction.set_token_instruction_type(copresence::RECEIVE);
    instruction.set_medium(audible ? AUDIO_AUDIBLE_DTMF
                                   : AUDIO_ULTRASOUND_PASSBAND);
    return instruction;
  }

  bool IsPlaying(AudioType type) { return manager_ptr_->IsPlaying(type); }

  bool IsRecording(AudioType type) { return manager_ptr_->IsRecording(type); }

  // This order is important. We want the message loop to get created before
  // our the audio directive handler since the directive list ctor (invoked
  // from the directive handler ctor) will post tasks.
  base::MessageLoop message_loop_;
  scoped_ptr<AudioDirectiveHandler> directive_handler_;
  // Unowned.
  AudioManagerStub* manager_ptr_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioDirectiveHandlerTest);
};

TEST_F(AudioDirectiveHandlerTest, Basic) {
  const base::TimeDelta kTtl = base::TimeDelta::FromMilliseconds(9999);
  directive_handler_->AddInstruction(
      CreateTransmitInstruction("token", true), "op_id1", kTtl);
  directive_handler_->AddInstruction(
      CreateTransmitInstruction("token", false), "op_id1", kTtl);
  directive_handler_->AddInstruction(
      CreateTransmitInstruction("token", false), "op_id2", kTtl);
  directive_handler_->AddInstruction(
      CreateReceiveInstruction(false), "op_id1", kTtl);
  directive_handler_->AddInstruction(
      CreateReceiveInstruction(true), "op_id2", kTtl);
  directive_handler_->AddInstruction(
      CreateReceiveInstruction(false), "op_id3", kTtl);

  EXPECT_TRUE(IsPlaying(AUDIBLE));
  EXPECT_TRUE(IsPlaying(INAUDIBLE));
  EXPECT_TRUE(IsRecording(AUDIBLE));
  EXPECT_TRUE(IsRecording(INAUDIBLE));

  directive_handler_->RemoveInstructions("op_id1");
  EXPECT_FALSE(IsPlaying(AUDIBLE));
  EXPECT_TRUE(IsPlaying(INAUDIBLE));
  EXPECT_TRUE(IsRecording(AUDIBLE));
  EXPECT_TRUE(IsRecording(INAUDIBLE));

  directive_handler_->RemoveInstructions("op_id2");
  EXPECT_FALSE(IsPlaying(INAUDIBLE));
  EXPECT_FALSE(IsRecording(AUDIBLE));
  EXPECT_TRUE(IsRecording(INAUDIBLE));

  directive_handler_->RemoveInstructions("op_id3");
  EXPECT_FALSE(IsRecording(INAUDIBLE));
}

TEST_F(AudioDirectiveHandlerTest, Timed) {
  scoped_ptr<base::SimpleTestTickClock> clock(new base::SimpleTestTickClock());
  base::SimpleTestTickClock* clock_ptr = clock.get();

  scoped_refptr<TickClockRefCounted> clock_proxy =
      new TickClockRefCounted(clock.Pass());
  directive_handler_->set_clock_for_testing(clock_proxy);

  scoped_ptr<base::Timer> timer(new base::MockTimer(false, false));
  base::MockTimer* timer_ptr = static_cast<base::MockTimer*>(timer.get());
  directive_handler_->set_timer_for_testing(timer.Pass());

  const base::TimeDelta kTtl1 = base::TimeDelta::FromMilliseconds(1337);
  directive_handler_->AddInstruction(
      CreateTransmitInstruction("token", true), "op_id1", kTtl1);

  const base::TimeDelta kTtl2 = base::TimeDelta::FromMilliseconds(1338);
  directive_handler_->AddInstruction(
      CreateTransmitInstruction("token", false), "op_id1", kTtl2);

  const base::TimeDelta kTtl3 = base::TimeDelta::FromMilliseconds(1336);
  directive_handler_->AddInstruction(
      CreateReceiveInstruction(false), "op_id3", kTtl3);
  EXPECT_TRUE(IsPlaying(AUDIBLE));
  EXPECT_TRUE(IsPlaying(INAUDIBLE));
  EXPECT_FALSE(IsRecording(AUDIBLE));
  EXPECT_TRUE(IsRecording(INAUDIBLE));

  // We *have* to call an operation on the directive handler after we advance
  // time to trigger the next set of operations, so ensure that after calling
  // advance, we are also calling another operation.
  clock_ptr->Advance(kTtl3 + base::TimeDelta::FromMilliseconds(1));

  // We are now at base + 1337ms.
  // This instruction expires at base + (1337 + 1337 = 2674)
  directive_handler_->AddInstruction(
      CreateReceiveInstruction(true), "op_id4", kTtl1);
  EXPECT_TRUE(IsPlaying(AUDIBLE));
  EXPECT_TRUE(IsPlaying(INAUDIBLE));
  EXPECT_TRUE(IsRecording(AUDIBLE));
  EXPECT_FALSE(IsRecording(INAUDIBLE));

  clock_ptr->Advance(base::TimeDelta::FromMilliseconds(1));

  // We are now at base + 1338ms.
  timer_ptr->Fire();
  EXPECT_FALSE(IsPlaying(AUDIBLE));
  EXPECT_TRUE(IsPlaying(INAUDIBLE));
  EXPECT_TRUE(IsRecording(AUDIBLE));

  clock_ptr->Advance(base::TimeDelta::FromMilliseconds(1));

  // We are now at base + 1339ms.
  timer_ptr->Fire();
  EXPECT_FALSE(IsPlaying(INAUDIBLE));
  EXPECT_TRUE(IsRecording(AUDIBLE));

  clock_ptr->Advance(kTtl3);

  // We are now at base + 2676ms.
  timer_ptr->Fire();
  EXPECT_FALSE(IsRecording(AUDIBLE));
}

// TODO(rkc): Write more tests that check more convoluted sequences of
// transmits/receives.

}  // namespace copresence
