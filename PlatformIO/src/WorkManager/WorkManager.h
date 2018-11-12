// RBotFirmware
// Rob Dobson 2016-2018

#pragma once

#include <Arduino.h>
#include "WorkItemQueue.h"
#include "EvaluatorPatterns.h"
#include "EvaluatorSequences.h"

class ConfigBase;
class RobotController;
class RestAPISystem;
class FileManager;

// Work Manager - handles all workflow for the robot
class WorkManager
{
private:
    ConfigBase& _systemConfig;
    ConfigBase& _robotConfig;
    ConfigBase& _ledStripConfig;
    RobotController& _robotController;
    WorkItemQueue _workItemQueue;
    RestAPISystem& _restAPISystem;
    FileManager& _fileManager;

    // Evaluators
    EvaluatorPatterns _evaluatorPatterns;
    EvaluatorSequences _evaluatorSequences;

public:
    WorkManager(ConfigBase& mainConfig,
                ConfigBase &robotConfig, 
                ConfigBase &ledStripConfig, 
                RobotController &robotController,
                RestAPISystem &restAPISystem,
                FileManager& fileManager) :
                _systemConfig(mainConfig),
                _robotConfig(robotConfig),
                _ledStripConfig(ledStripConfig),
                _robotController(robotController),
                _restAPISystem(restAPISystem),
                _fileManager(fileManager)
    {
    }

    // Check if queue can accept a work item
    bool canAcceptWorkItem();

    // Queue info
    bool queueIsEmpty();

    // Call frequently to pump the queue
    void service();

    // Configuration of the robot
    void getRobotConfig(String& respStr);
    bool setRobotConfig(const uint8_t* pData, int len);
    bool setLedStripConfig(const uint8_t* pData, int len);

    // Apply configuration
    void reconfigure();

    // Process startup actions
    void handleStartupCommands();

    // Get status report
    void queryStatus(String &respStr);

    // Add a work item to the queue
    void addWorkItem(WorkItem& workItem, String &retStr, int cmdIdx = -1);

private:
    // Execute an item of work
    bool execWorkItem(WorkItem& workItem);

    // Process a single 
    void processSingle(const char *pCmdStr, String &retStr);

};
