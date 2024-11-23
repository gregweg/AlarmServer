#include "alarm_server.hpp"
#include <iostream>

AlarmSystem::AlarmSystem() : running(true), db(std::make_unique<Database>()) {
    loadAlarms();
    checker_thread = std::thread(&AlarmSystem::checkAlarms, this);
}

AlarmSystem::~AlarmSystem() {
    running = false;
    cv.notify_all();
    if (checker_thread.joinable()) {
        checker_thread.join();
    }
}

void AlarmSystem::loadAlarms() {
    try {
        auto saved_alarms = db->loadAlarms();
        std::lock_guard<std::mutex> lock(mtx);
        
        for (const auto& alarm : saved_alarms) {
            if (alarm.recurrence != RecurrenceType::NONE) {
                auto next_occurrence = calculateNextOccurrence(alarm);
                events.push({alarm.id, alarm.description, next_occurrence, 
                           alarm.recurrence});
            } else if (alarm.datetime > std::chrono::system_clock::now()) {
                events.push(alarm);
            }
        }
    } catch (const DatabaseException& e) {
        throw AlarmSystemException("Failed to load alarms: " + 
            std::string(e.what()));
    }
}

void AlarmSystem::addEvent(const std::string& description,
                          const std::string& datetime_str,
                          RecurrenceType recurrence) {
    try {
        auto tp = datetime_utils::parseDateTime(datetime_str);
        int id = db->addAlarm(description, datetime_str, recurrence);
        
        {
            std::lock_guard<std::mutex> lock(mtx);
            events.push({id, description, tp, recurrence});
        }
        cv.notify_one();
    } catch (const DatabaseException& e) {
        throw AlarmSystemException("Failed to add event: " + 
            std::string(e.what()));
    }
}

std::vector<std::pair<std::string, std::string>> AlarmSystem::getEvents() {
    std::vector<std::pair<std::string, std::string>> result;
    std::lock_guard<std::mutex> lock(mtx);
    
    auto temp = events;
    while (!temp.empty()) {
        auto event = temp.top();
        std::string desc = event.description;
        
        if (event.recurrence != RecurrenceType::NONE) {
            desc += " (" + recurrenceTypeToString(event.recurrence) + ")";
        }
        
        result.push_back({
            desc,
            datetime_utils::formatDateTime(event.datetime)
        });
        temp.pop();
    }
    
    return result;
}

std::chrono::system_clock::time_point AlarmSystem::calculateNextOccurrence(
    const AlarmEvent& event) {
    auto time = event.datetime;
    auto now = std::chrono::system_clock::now();
    
    while (time <= now) {
        switch (event.recurrence) {
            case RecurrenceType::DAILY: {
                time += std::chrono::hours(24);
                break;
            }
            case RecurrenceType::WEEKLY: {
                time += std::chrono::hours(24 * 7);
                break;
            }
            case RecurrenceType::MONTHLY: {
                auto time_t = std::chrono::system_clock::to_time_t(time);
                std::tm* tm = std::localtime(&time_t);
                tm->tm_mon += 1;
                if (tm->tm_mon > 11) {
                    tm->tm_mon = 0;
                    tm->tm_year += 1;
                }
                time = std::chrono::system_clock::from_time_t(std::mktime(tm));
                break;
            }
            case RecurrenceType::YEARLY: {
                auto time_t = std::chrono::system_clock::to_time_t(time);
                std::tm* tm = std::localtime(&time_t);
                tm->tm_year += 1;
                time = std::chrono::system_clock::from_time_t(std::mktime(tm));
                break;
            }
            default:
                return time;
        }
    }
    return time;
}

void AlarmSystem::checkAlarms() {
    while (running) {
        std::unique_lock<std::mutex> lock(mtx);
        
        if (events.empty()) {
            cv.wait(lock);
            continue;
        }
        
        auto now = std::chrono::system_clock::now();
        auto next_event = events.top();
        
        if (next_event.datetime <= now) {
            std::cout << "ALARM: " << next_event.description << std::endl;
            // Here you could implement notifications (email, push, etc.)
            events.pop();
            
            if (next_event.recurrence != RecurrenceType::NONE) {
                auto next_occurrence = calculateNextOccurrence(next_event);
                next_event.datetime = next_occurrence;
                
                try {
                    db->updateAlarmDateTime(
                        next_event.id,
                        datetime_utils::formatDateTime(next_occurrence)
                    );
                    events.push(next_event);
                } catch (const DatabaseException& e) {
                    std::cerr << "Failed to update recurring alarm: " 
                              << e.what() << std::endl;
                }
            }
        } else {
            cv.wait_until(lock, next_event.datetime);
        }
    }
}