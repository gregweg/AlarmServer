#include <crow.h>
#include <queue>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <sstream>
#include <iomanip>
#include <sqlite3.h>
#include <optional>
#include <vector>
#include <stdexcept>
#include <memory>

// Forward declarations
class Database;
class AlarmSystem;

enum class RecurrenceType {
    NONE,
    DAILY,
    WEEKLY,
    MONTHLY,
    YEARLY
};

struct AlarmEvent {
    int id;
    std::string description;
    std::chrono::system_clock::time_point datetime;
    RecurrenceType recurrence;
    
    bool operator>(const AlarmEvent& other) const {
        return datetime > other.datetime;
    }
};

class Database {
public:
    Database();
    virtual ~Database();

    // Make these virtual for mocking in tests
    virtual void createTables();
    virtual int addAlarm(const std::string& description,
                        const std::string& datetime_str,
                        RecurrenceType recurrence);
    virtual std::vector<AlarmEvent> loadAlarms();
    virtual void updateAlarmDateTime(int id, const std::string& new_datetime);

protected:
    sqlite3* db;
};

// AlarmSystem interface
class AlarmSystem {
public:
    AlarmSystem();
    virtual ~AlarmSystem();

    // Public interface
    virtual void loadAlarms();
    virtual void addEvent(const std::string& description, 
                         const std::string& datetime_str,
                         RecurrenceType recurrence = RecurrenceType::NONE);
    virtual std::vector<std::pair<std::string, std::string>> getEvents();

protected:
    // Protected interface for testing and derived classes
    virtual std::chrono::system_clock::time_point calculateNextOccurrence(
        const AlarmEvent& event);
    virtual void checkAlarms();
    
    // Member variables
    std::priority_queue<AlarmEvent, 
                       std::vector<AlarmEvent>, 
                       std::greater<AlarmEvent>> events;
    std::mutex mtx;
    std::condition_variable cv;
    bool running;
    std::thread checker_thread;
    std::unique_ptr<Database> db;

private:
    // Utility functions
    static std::chrono::system_clock::time_point parseDateTime(
        const std::string& datetime_str);
    static std::string formatDateTime(
        const std::chrono::system_clock::time_point& tp);
};

namespace datetime_utils {
    inline std::chrono::system_clock::time_point parseDateTime(
        const std::string& datetime_str) {
            std::tm tm = {};
            std::istringstream ss(datetime_str);
            ss >> std::get_time(&tm, "%Y-%m-%d %H:%M");
            return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }

    inline std::string formatDateTime(
        const std::chrono::system_clock::time_point& tp) {
            auto time_t = std::chrono::system_clock::to_time_t(tp);
            std::stringstream ss;
            ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M");
            return ss.str();
    }
}


// Exception classes
class DatabaseException : public std::runtime_error {
public:
    explicit DatabaseException(const std::string& message) 
        : std::runtime_error(message) {}
};

class AlarmSystemException : public std::runtime_error {
public:
    explicit AlarmSystemException(const std::string& message) 
        : std::runtime_error(message) {}
};

// Factory function for creating AlarmSystem instances
inline std::unique_ptr<AlarmSystem> createAlarmSystem() {
    return std::make_unique<AlarmSystem>();
}

// Inline helper functions for type conversions
inline std::string recurrenceTypeToString(RecurrenceType type) {
    switch (type) {
        case RecurrenceType::DAILY: return "Daily";
        case RecurrenceType::WEEKLY: return "Weekly";
        case RecurrenceType::MONTHLY: return "Monthly";
        case RecurrenceType::YEARLY: return "Yearly";
        default: return "None";
    }
}

inline RecurrenceType stringToRecurrenceType(const std::string& str) {
    if (str == "Daily") return RecurrenceType::DAILY;
    if (str == "Weekly") return RecurrenceType::WEEKLY;
    if (str == "Monthly") return RecurrenceType::MONTHLY;
    if (str == "Yearly") return RecurrenceType::YEARLY;
    return RecurrenceType::NONE;
}