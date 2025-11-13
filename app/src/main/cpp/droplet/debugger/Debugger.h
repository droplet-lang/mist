/*
* ============================================================
 *  Droplet
 * ============================================================
 *  Copyright (c) 2025 Droplet Contributors
 *  All rights reserved.
 *
 *  Licensed under the MIT License.
 *  See LICENSE file in the project root for full license.
 *
 *  File: Debugger
 *  Created: 11/12/2025
 * ============================================================
 */

#ifndef DROPLET_DEBUGGER_H
#define DROPLET_DEBUGGER_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include "../vm/VM.h"

struct SourceLocation {
    std::string file;
    uint32_t line;
    uint32_t column;

    SourceLocation() : line(0), column(0) {}
    SourceLocation(const std::string& f, uint32_t l, uint32_t c) : file(f), line(l), column(c) {}
};

struct FunctionDebugInfo {
    std::string name;
    std::string file;
    uint32_t startLine;
    uint32_t endLine;
    std::map<uint32_t, SourceLocation> ipToLocation;  // IP -> source location
    std::map<std::string, uint8_t> localVariables;    // var name -> local slot
};

struct Breakpoint {
    uint32_t id;
    std::string file;
    uint32_t line;
    bool enabled;
    std::string condition;

    Breakpoint(uint32_t i, const std::string& f, uint32_t l) : id(i), file(f), line(l), enabled(true) {}
};

class Debugger {
public:
    bool isRunning;
    uint32_t currentStepLine = 0;

    enum class StepMode {
        NONE,
        STEP_INTO,   // Step into function calls
        STEP_OVER,   // Step over function calls
        STEP_OUT,    // Step out of current function
        STEP_NEXT_LINE,   // Step to next SOURCE LINE (skip multiple instructions on same line)
        CONTINUE     // Continue until breakpoint
    };

    Debugger(VM* vm);

    // Control flow
    void start();
    void stepInto();
    void stepOver();
    void stepOut();
    void continueExecution();
    void pause();
    void stop();
    void stepNextLine();

    // Breakpoints
    uint32_t addBreakpoint(const std::string& file, uint32_t line);
    void removeBreakpoint(uint32_t id);
    void enableBreakpoint(uint32_t id);
    void disableBreakpoint(uint32_t id);
    void listBreakpoints() const;

    // Inspection
    void printStack() const;
    void printLocals() const;
    void printGlobals() const;
    void printVariable(const std::string& name) const;
    void printBacktrace() const;
    void printCurrentLocation() const;

    // Source code display
    void listSource(int contextLines = 5) const;

    // Debug info management
    void addFunctionDebugInfo(uint32_t funcIdx, const FunctionDebugInfo& info);
    void setSourceFile(const std::string& file, const std::vector<std::string>& lines);

    // Check if should break at current location
    bool shouldBreak();

    // Interactive debug loop
    void debugLoop();

private:
    VM* vm;
    StepMode stepMode;
    uint32_t stepFrameDepth;  // For step over/out

    // Debug information
    std::map<uint32_t, FunctionDebugInfo> functionDebugInfo;
    std::map<std::string, std::vector<std::string>> sourceFiles;
    std::map<uint32_t, std::unique_ptr<Breakpoint>> breakpoints;
    uint32_t nextBreakpointId;

    // Helpers
    [[nodiscard]] bool hasBreakpointAt(const std::string& file, uint32_t line) const;
    [[nodiscard]] SourceLocation getCurrentLocation() const;
    [[nodiscard]] std::string getSourceLine(const std::string& file, uint32_t line) const;

    static void printPrompt();
    [[nodiscard]] static std::vector<std::string> tokenizeCommand(const std::string& cmd);
    void executeCommand(const std::vector<std::string>& tokens);

    static void handleClear(const std::vector<std::string> &args);

    // Command handlers
    void handleStep(const std::vector<std::string>& args);
    void handleNext(const std::vector<std::string>& args);

    void handleStepInstruction(const std::vector<std::string> &args);

    void handleFinish(const std::vector<std::string>& args);
    void handleContinue(const std::vector<std::string>& args);
    void handleBreak(const std::vector<std::string>& args);
    void handleInfo(const std::vector<std::string>& args) const;
    void handlePrint(const std::vector<std::string>& args) const;
    void handleList(const std::vector<std::string>& args) const;
    void handleBacktrace(const std::vector<std::string>& args) const;
    void handleQuit(const std::vector<std::string>& args);

    static void handleHelp(const std::vector<std::string>& args);
};

#endif //DROPLET_DEBUGGER_H