#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include "alarm_server.hpp"  // We'll need to separate the classes into header files

// Mock Database for testing
class MockDatabase : public Database {
public:
    MOCK_METHOD(int, addAlarm, (const std::string&, const std::string&, RecurrenceType));
    MOCK_METHOD(std::vector<AlarmEvent>, loadAlarms, ());
    MOCK_METHOD(void, updateAlarmDateTime, (int, const std::string&));
};

// Test fixture
class AlarmSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up any necessary test state
    }

    void TearDown() override {
        // Clean up any test state
    }

    // Helper function to create a timepoint from string
    std::chrono::system_clock::time_point makeTimePoint(const std::string& datetime_str) {
        std::tm tm = {};
        std::istringstream ss(datetime_str);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M");
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }
};

// Database Tests
TEST_F(AlarmSystemTest, DatabaseCreation) {
    EXPECT_NO_THROW({
        Database db;
    });
}

TEST_F(AlarmSystemTest, DatabaseAddAlarm) {
    Database db;
    int id = db.addAlarm("Test Alarm", "2024-12-31 23:59", RecurrenceType::NONE);
    EXPECT_GT(id, 0);
}

TEST_F(AlarmSystemTest, DatabaseLoadAlarms) {
    Database db;
    // Add test alarm
    db.addAlarm("Test Alarm", "2024-12-31 23:59", RecurrenceType::NONE);
    
    auto alarms = db.loadAlarms();
    EXPECT_FALSE(alarms.empty());
    EXPECT_EQ(alarms[0].description, "Test Alarm");
}

// AlarmEvent Tests
TEST_F(AlarmSystemTest, AlarmEventComparison) {
    AlarmEvent event1{1, "First", makeTimePoint("2024-01-01 10:00"), RecurrenceType::NONE};
    AlarmEvent event2{2, "Second", makeTimePoint("2024-01-01 11:00"), RecurrenceType::NONE};
    
    EXPECT_FALSE(event1 > event2);
    EXPECT_TRUE(event2 > event1);
}

// AlarmSystem Tests
class AlarmSystemWithMockDB : public AlarmSystem {
public:
    explicit AlarmSystemWithMockDB(std::shared_ptr<MockDatabase> mock_db) 
        : mock_db_(mock_db) {}
    
    // Expose protected methods for testing
    using AlarmSystem::calculateNextOccurrence;
    
protected:
    std::shared_ptr<MockDatabase> mock_db_;
};

TEST_F(AlarmSystemTest, AddEvent) {
    auto mock_db = std::make_shared<MockDatabase>();
    AlarmSystemWithMockDB system(mock_db);
    
    EXPECT_CALL(*mock_db, addAlarm("Test Event", "2024-01-01 10:00", RecurrenceType::NONE))
        .WillOnce(testing::Return(1));
        
    system.addEvent("Test Event", "2024-01-01 10:00");
    
    auto events = system.getEvents();
    EXPECT_EQ(events.size(), 1);
    EXPECT_EQ(events[0].first, "Test Event");
}

TEST_F(AlarmSystemTest, RecurringEventCalculation) {
    auto mock_db = std::make_shared<MockDatabase>();
    AlarmSystemWithMockDB system(mock_db);
    
    AlarmEvent daily_event{
        1,
        "Daily Event",
        makeTimePoint("2024-01-01 10:00"),
        RecurrenceType::DAILY
    };
    
    auto next_time = system.calculateNextOccurrence(daily_event);
    EXPECT_GT(next_time, std::chrono::system_clock::now());
}

// Integration Tests
TEST_F(AlarmSystemTest, CompleteWorkflow) {
    AlarmSystem system;
    
    // Add various types of alarms
    system.addEvent("One-time Event", "2024-12-31 23:59", RecurrenceType::NONE);
    system.addEvent("Daily Event", "2024-01-01 10:00", RecurrenceType::DAILY);
    system.addEvent("Weekly Event", "2024-01-01 11:00", RecurrenceType::WEEKLY);
    
    auto events = system.getEvents();
    EXPECT_EQ(events.size(), 3);
    
    // Verify events are ordered correctly
    auto now = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point last_time;
    bool first = true;
    
    for (const auto& event : events) {
        auto time = makeTimePoint(event.second);
        if (!first) {
            EXPECT_GE(time, last_time);
        }
        first = false;
        last_time = time;
    }
}

// Performance Tests
TEST_F(AlarmSystemTest, LargeNumberOfEvents) {
    AlarmSystem system;
    const int NUM_EVENTS = 1000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_EVENTS; i++) {
        system.addEvent(
            "Event " + std::to_string(i),
            "2024-12-31 23:59",
            RecurrenceType::NONE
        );
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_LT(duration.count(), 1000);  // Should complete in less than 1 second
    
    auto events = system.getEvents();
    EXPECT_EQ(events.size(), NUM_EVENTS);
}

// Stress Tests
TEST_F(AlarmSystemTest, ConcurrentAccess) {
    AlarmSystem system;
    const int NUM_THREADS = 10;
    const int EVENTS_PER_THREAD = 100;
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([&system, i, EVENTS_PER_THREAD]() {
            for (int j = 0; j < EVENTS_PER_THREAD; j++) {
                system.addEvent(
                    "Thread " + std::to_string(i) + " Event " + std::to_string(j),
                    "2024-12-31 23:59",
                    RecurrenceType::NONE
                );
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto events = system.getEvents();
    EXPECT_EQ(events.size(), NUM_THREADS * EVENTS_PER_THREAD);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}