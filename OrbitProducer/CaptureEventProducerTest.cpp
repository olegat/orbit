// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread>

#include "FakeProducerSideService.h"
#include "OrbitProducer/CaptureEventProducer.h"
#include "absl/strings/str_format.h"
#include "grpcpp/grpcpp.h"
#include "producer_side_services.grpc.pb.h"

namespace orbit_producer {

namespace {

class CaptureEventProducerImpl : public CaptureEventProducer {
 public:
  // Override and forward these methods to make them public.
  void BuildAndStart(const std::shared_ptr<grpc::Channel>& channel) override {
    CaptureEventProducer::BuildAndStart(channel);
  }

  void ShutdownAndWait() override { CaptureEventProducer::ShutdownAndWait(); }

  // Hide and forward these methods to make them public.
  [[nodiscard]] bool SendCaptureEvents(
      const orbit_grpc_protos::ReceiveCommandsAndSendEventsRequest& send_events_request) {
    return CaptureEventProducer::SendCaptureEvents(send_events_request);
  }

  [[nodiscard]] bool NotifyAllEventsSent() { return CaptureEventProducer::NotifyAllEventsSent(); }

  MOCK_METHOD(void, OnCaptureStart, (), (override));
  MOCK_METHOD(void, OnCaptureStop, (), (override));
};

class CaptureEventProducerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    fake_service_.emplace();

    grpc::ServerBuilder builder;
    builder.RegisterService(&*fake_service_);
    fake_server_ = builder.BuildAndStart();
    ASSERT_NE(fake_server_, nullptr);

    std::shared_ptr<grpc::Channel> channel =
        fake_server_->InProcessChannel(grpc::ChannelArguments{});

    producer_.emplace();
    producer_->BuildAndStart(channel);

    // Leave some time for the ReceiveCommandsAndSendEvents RPC to actually happen.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  void TearDown() override {
    // Leave some time for all pending communication to finish.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    producer_->ShutdownAndWait();
    producer_.reset();

    fake_service_->FinishAndDisallowRpc();
    fake_server_->Shutdown();
    fake_server_->Wait();

    fake_service_.reset();
    fake_server_.reset();
  }

  std::optional<FakeProducerSideService> fake_service_;
  std::unique_ptr<grpc::Server> fake_server_;
  std::optional<CaptureEventProducerImpl> producer_;
};

constexpr std::chrono::duration kWaitMessagesSentDuration = std::chrono::milliseconds(25);

}  // namespace

TEST_F(CaptureEventProducerTest, OnCaptureStartStopAndIsCapturing) {
  EXPECT_FALSE(producer_->IsCapturing());

  EXPECT_CALL(*producer_, OnCaptureStart).Times(1);
  fake_service_->SendStartCaptureCommand();
  std::this_thread::sleep_for(kWaitMessagesSentDuration);
  EXPECT_TRUE(producer_->IsCapturing());

  ::testing::Mock::VerifyAndClearExpectations(&*producer_);

  EXPECT_CALL(*producer_, OnCaptureStop).Times(1);
  fake_service_->SendStopCaptureCommand();
  std::this_thread::sleep_for(kWaitMessagesSentDuration);
  EXPECT_FALSE(producer_->IsCapturing());

  ::testing::Mock::VerifyAndClearExpectations(&*producer_);

  EXPECT_CALL(*producer_, OnCaptureStart).Times(1);
  fake_service_->SendStartCaptureCommand();
  std::this_thread::sleep_for(kWaitMessagesSentDuration);
  EXPECT_TRUE(producer_->IsCapturing());

  ::testing::Mock::VerifyAndClearExpectations(&*producer_);

  EXPECT_CALL(*producer_, OnCaptureStop).Times(1);
  fake_service_->SendStopCaptureCommand();
  std::this_thread::sleep_for(kWaitMessagesSentDuration);
  EXPECT_FALSE(producer_->IsCapturing());
}

TEST_F(CaptureEventProducerTest, SendCaptureEventsAndAllEventsSent) {
  {
    ::testing::InSequence in_sequence;
    EXPECT_CALL(*fake_service_, OnCaptureEventsReceived).Times(2);
    EXPECT_CALL(*fake_service_, OnAllEventsSentReceived).Times(1);
  }

  orbit_grpc_protos::ReceiveCommandsAndSendEventsRequest send_events_request;
  send_events_request.mutable_buffered_capture_events()->mutable_capture_events()->Add();
  EXPECT_TRUE(producer_->SendCaptureEvents(send_events_request));
  EXPECT_TRUE(producer_->SendCaptureEvents(send_events_request));
  EXPECT_TRUE(producer_->NotifyAllEventsSent());
}

TEST_F(CaptureEventProducerTest, UnexpectedStartStopCaptureCommands) {
  EXPECT_FALSE(producer_->IsCapturing());

  EXPECT_CALL(*producer_, OnCaptureStart).Times(1);
  fake_service_->SendStartCaptureCommand();
  std::this_thread::sleep_for(kWaitMessagesSentDuration);
  EXPECT_TRUE(producer_->IsCapturing());

  ::testing::Mock::VerifyAndClearExpectations(&*producer_);

  EXPECT_CALL(*producer_, OnCaptureStart).Times(0);
  // This should have no effect.
  fake_service_->SendStartCaptureCommand();
  std::this_thread::sleep_for(kWaitMessagesSentDuration);
  EXPECT_TRUE(producer_->IsCapturing());

  ::testing::Mock::VerifyAndClearExpectations(&*producer_);

  EXPECT_CALL(*producer_, OnCaptureStop).Times(1);
  fake_service_->SendStopCaptureCommand();
  std::this_thread::sleep_for(kWaitMessagesSentDuration);
  EXPECT_FALSE(producer_->IsCapturing());

  ::testing::Mock::VerifyAndClearExpectations(&*producer_);

  EXPECT_CALL(*producer_, OnCaptureStop).Times(0);
  // This should have no effect.
  fake_service_->SendStopCaptureCommand();
  std::this_thread::sleep_for(kWaitMessagesSentDuration);
  EXPECT_FALSE(producer_->IsCapturing());
}

TEST_F(CaptureEventProducerTest, ServiceDisconnectCausesOnCaptureStop) {
  EXPECT_FALSE(producer_->IsCapturing());

  EXPECT_CALL(*producer_, OnCaptureStart).Times(1);
  fake_service_->SendStartCaptureCommand();
  std::this_thread::sleep_for(kWaitMessagesSentDuration);
  EXPECT_TRUE(producer_->IsCapturing());

  ::testing::Mock::VerifyAndClearExpectations(&*producer_);

  EXPECT_CALL(*producer_, OnCaptureStop).Times(1);
  // Disconnect.
  fake_service_->FinishAndDisallowRpc();
  std::this_thread::sleep_for(kWaitMessagesSentDuration);
  EXPECT_FALSE(producer_->IsCapturing());
}

TEST_F(CaptureEventProducerTest, SendingMessagesFailsWhenDisconnected) {
  EXPECT_FALSE(producer_->IsCapturing());

  EXPECT_CALL(*producer_, OnCaptureStart).Times(1);
  fake_service_->SendStartCaptureCommand();
  std::this_thread::sleep_for(kWaitMessagesSentDuration);
  EXPECT_TRUE(producer_->IsCapturing());

  ::testing::Mock::VerifyAndClearExpectations(&*producer_);

  {
    ::testing::InSequence in_sequence;
    EXPECT_CALL(*fake_service_, OnCaptureEventsReceived).Times(2);
    EXPECT_CALL(*fake_service_, OnAllEventsSentReceived).Times(1);
  }
  orbit_grpc_protos::ReceiveCommandsAndSendEventsRequest send_events_request;
  send_events_request.mutable_buffered_capture_events()->mutable_capture_events()->Add();
  EXPECT_TRUE(producer_->SendCaptureEvents(send_events_request));
  EXPECT_TRUE(producer_->SendCaptureEvents(send_events_request));
  EXPECT_TRUE(producer_->NotifyAllEventsSent());
  std::this_thread::sleep_for(kWaitMessagesSentDuration);

  ::testing::Mock::VerifyAndClearExpectations(&*fake_service_);

  EXPECT_CALL(*producer_, OnCaptureStop).Times(1);
  // Disconnect.
  fake_service_->FinishAndDisallowRpc();
  std::this_thread::sleep_for(kWaitMessagesSentDuration);
  EXPECT_FALSE(producer_->IsCapturing());

  ::testing::Mock::VerifyAndClearExpectations(&*producer_);

  EXPECT_CALL(*fake_service_, OnCaptureEventsReceived).Times(0);
  EXPECT_CALL(*fake_service_, OnAllEventsSentReceived).Times(0);
  EXPECT_FALSE(producer_->SendCaptureEvents(send_events_request));
  EXPECT_FALSE(producer_->SendCaptureEvents(send_events_request));
  EXPECT_FALSE(producer_->NotifyAllEventsSent());
}

TEST_F(CaptureEventProducerTest, DisconnectAndReconnect) {
  EXPECT_FALSE(producer_->IsCapturing());

  EXPECT_CALL(*producer_, OnCaptureStart).Times(1);
  fake_service_->SendStartCaptureCommand();
  std::this_thread::sleep_for(kWaitMessagesSentDuration);
  EXPECT_TRUE(producer_->IsCapturing());

  ::testing::Mock::VerifyAndClearExpectations(&*producer_);

  {
    ::testing::InSequence in_sequence;
    EXPECT_CALL(*fake_service_, OnCaptureEventsReceived).Times(2);
    EXPECT_CALL(*fake_service_, OnAllEventsSentReceived).Times(1);
  }
  orbit_grpc_protos::ReceiveCommandsAndSendEventsRequest send_events_request;
  send_events_request.mutable_buffered_capture_events()->mutable_capture_events()->Add();
  EXPECT_TRUE(producer_->SendCaptureEvents(send_events_request));
  EXPECT_TRUE(producer_->SendCaptureEvents(send_events_request));
  EXPECT_TRUE(producer_->NotifyAllEventsSent());
  std::this_thread::sleep_for(kWaitMessagesSentDuration);

  ::testing::Mock::VerifyAndClearExpectations(&*fake_service_);

  // Reduce reconnection delay before disconnecting.
  static constexpr uint64_t kReconnectionDelayMs = 50;
  producer_->SetReconnectionDelayMs(kReconnectionDelayMs);

  EXPECT_CALL(*producer_, OnCaptureStop).Times(1);
  // Disconnect.
  fake_service_->FinishAndDisallowRpc();
  std::this_thread::sleep_for(kWaitMessagesSentDuration);
  EXPECT_FALSE(producer_->IsCapturing());

  ::testing::Mock::VerifyAndClearExpectations(&*producer_);

  EXPECT_CALL(*fake_service_, OnCaptureEventsReceived).Times(0);
  EXPECT_CALL(*fake_service_, OnAllEventsSentReceived).Times(0);
  EXPECT_FALSE(producer_->SendCaptureEvents(send_events_request));
  EXPECT_FALSE(producer_->SendCaptureEvents(send_events_request));
  EXPECT_FALSE(producer_->NotifyAllEventsSent());
  std::this_thread::sleep_for(kWaitMessagesSentDuration);

  ::testing::Mock::VerifyAndClearExpectations(&*fake_service_);

  // Wait for reconnection.
  fake_service_->ReAllowRpc();
  std::this_thread::sleep_for(std::chrono::milliseconds{2 * kReconnectionDelayMs});

  EXPECT_CALL(*producer_, OnCaptureStart).Times(1);
  fake_service_->SendStartCaptureCommand();
  std::this_thread::sleep_for(kWaitMessagesSentDuration);
  EXPECT_TRUE(producer_->IsCapturing());

  ::testing::Mock::VerifyAndClearExpectations(&*producer_);

  {
    ::testing::InSequence in_sequence;
    EXPECT_CALL(*fake_service_, OnCaptureEventsReceived).Times(2);
    EXPECT_CALL(*fake_service_, OnAllEventsSentReceived).Times(1);
  }
  EXPECT_TRUE(producer_->SendCaptureEvents(send_events_request));
  EXPECT_TRUE(producer_->SendCaptureEvents(send_events_request));
  EXPECT_TRUE(producer_->NotifyAllEventsSent());
  std::this_thread::sleep_for(kWaitMessagesSentDuration);

  ::testing::Mock::VerifyAndClearExpectations(&*fake_service_);

  EXPECT_CALL(*producer_, OnCaptureStop).Times(1);
  fake_service_->SendStopCaptureCommand();
  std::this_thread::sleep_for(kWaitMessagesSentDuration);
  EXPECT_FALSE(producer_->IsCapturing());
}

}  // namespace orbit_producer
