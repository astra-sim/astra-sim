#include "astra-sim/system/BasicEventHandlerData.hh"
#include "astra-sim/system/Common.hh"
#include "gtest/gtest.h"

// Make sure initialization does what it should
TEST(BasicEventHandlerDataTest, Init) {
  int nodeId = 5;
  AstraSim::EventType eventType = AstraSim::EventType::Read_Port_Free;
  AstraSim::BasicEventHandlerData data(nodeId, eventType);
  EXPECT_EQ(data.nodeId, nodeId);
  EXPECT_EQ(data.event, eventType);
}
