#include "alarm_server.hpp"
#include <iostream>

// HTML template for the web interface
const char* HTML_TEMPLATE = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Alarm System</title>
    <style>
        body { 
            font-family: Arial, sans-serif; 
            max-width: 800px; 
            margin: 0 auto; 
            padding: 20px;
        }
        .form-group { 
            margin-bottom: 15px; 
        }
        input, select, button { 
            padding: 8px; 
            margin: 5px 0; 
            width: 100%; 
        }
        button { 
            background-color: #4CAF50; 
            color: white; 
            border: none; 
            cursor: pointer; 
        }
        button:hover { 
            background-color: #45a049; 
        }
        #alarms { 
            margin-top: 20px; 
        }
        .alarm-item { 
            padding: 10px; 
            border-bottom: 1px solid #ddd; 
        }
    </style>
</head>
<body>
    <h1>Alarm System</h1>
    <form id='alarmForm'>
        <div class="form-group">
            <input type='text' id='description' placeholder='Event description' required>
        </div>
        <div class="form-group">
            <input type='datetime-local' id='datetime' required>
        </div>
        <div class="form-group">
            <select id='recurrence'>
                <option value='0'>No recurrence</option>
                <option value='1'>Daily</option>
                <option value='2'>Weekly</option>
                <option value='3'>Monthly</option>
                <option value='4'>Yearly</option>
            </select>
        </div>
        <button type='submit'>Add Alarm</button>
    </form>
    <h2>Current Alarms</h2>
    <div id='alarms'></div>

    <script>
        document.getElementById('alarmForm').onsubmit = function(e) {
            e.preventDefault();
            fetch('/add_alarm', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({
                    description: document.getElementById('description').value,
                    datetime: document.getElementById('datetime').value.replace('T', ' '),
                    recurrence: parseInt(document.getElementById('recurrence').value)
                })
            })
            .then(response => {
                if (!response.ok) {
                    throw new Error('Failed to add alarm');
                }
                return response.json();
            })
            .then(() => {
                updateAlarms();
                document.getElementById('alarmForm').reset();
            })
            .catch(error => {
                console.error('Error:', error);
                alert('Failed to add alarm: ' + error.message);
            });
        };
        
        function updateAlarms() {
            fetch('/get_alarms')
            .then(response => response.json())
            .then(alarms => {
                const alarmsDiv = document.getElementById('alarms');
                alarmsDiv.innerHTML = '';
                alarms.forEach(alarm => {
                    const div = document.createElement('div');
                    div.className = 'alarm-item';
                    div.textContent = `${alarm.description} - ${alarm.datetime}`;
                    alarmsDiv.appendChild(div);
                });
            })
            .catch(error => {
                console.error('Error:', error);
                alert('Failed to fetch alarms: ' + error.message);
            });
        }
        
        setInterval(updateAlarms, 5000);
        updateAlarms();
    </script>
</body>
</html>
)";

int main() {
    try {
        crow::SimpleApp app;
        auto alarm_system = createAlarmSystem();

        CROW_ROUTE(app, "/")([]() {
            return crow::response(HTML_TEMPLATE);
        });

        CROW_ROUTE(app, "/add_alarm")
        .methods("POST"_method)
        ([&alarm_system](const crow::request& req) {
            try {
                auto json = crow::json::load(req.body);
                if (!json) {
                    return crow::response(400, "Invalid JSON");
                }
                
                alarm_system->addEvent(
                    json["description"].s(),
                    json["datetime"].s(),
                    static_cast<RecurrenceType>(json["recurrence"].i())
                );
                
                return crow::response(200);
            } catch (const std::exception& e) {
                return crow::response(500, e.what());
            }
        });

        CROW_ROUTE(app, "/get_alarms")
        ([&alarm_system]() {
            try {
                auto events = alarm_system->getEvents();
                crow::json::wvalue response;
                std::vector<crow::json::wvalue> json_events;
        
                for (const auto& event : events) {
                    crow::json::wvalue json_event;
                    json_event["description"] = event.first;
                    json_event["datetime"] = event.second;
                    json_events.push_back(std::move(json_event));
                }
        
                response = std::move(json_events);
                return crow::response(response);
            } catch (const std::exception& e) {
                return crow::response(500, e.what());
            }
        });

        app.port(8080).multithreaded().run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}