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
#include "Debugger.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>

static std::string toLower(const std::string &str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

Debugger::Debugger(VM *vm)
    : vm(vm), stepMode(StepMode::NONE), isRunning(false),
      stepFrameDepth(0), nextBreakpointId(1) {
}

void Debugger::start() {
    isRunning = true;
    stepMode = StepMode::STEP_INTO;
    std::cout << "Debugger started. Type 'help' for available commands.\n";
}

void Debugger::stepInto() {
    stepMode = StepMode::STEP_INTO;
    isRunning = true;
}

void Debugger::stepOver() {
    stepMode = StepMode::STEP_OVER;
    stepFrameDepth = static_cast<uint32_t>(vm->call_frames.size());
    isRunning = true;
}

void Debugger::stepOut() {
    stepMode = StepMode::STEP_OUT;
    stepFrameDepth = static_cast<uint32_t>(vm->call_frames.size()) - 1;
    isRunning = true;
}

void Debugger::continueExecution() {
    stepMode = StepMode::CONTINUE;
    isRunning = true;
}

void Debugger::pause() {
    isRunning = false;
}

void Debugger::stop() {
    isRunning = false;
    stepMode = StepMode::NONE;
}

void Debugger::stepNextLine() {
    // Get current location
    SourceLocation currentLoc = getCurrentLocation();
    if (currentLoc.file.empty()) {
        // If we can't determine location, fall back to step into
        stepInto();
        return;
    }

    // Remember current line
    currentStepLine = currentLoc.line;
    stepMode = StepMode::STEP_NEXT_LINE;
    isRunning = true;
}

uint32_t Debugger::addBreakpoint(const std::string &file, uint32_t line) {
    std::string normalizedFile = toLower(file); // normalize path
    uint32_t id = nextBreakpointId++;
    breakpoints[id] = std::make_unique<Breakpoint>(id, normalizedFile, line);
    std::cout << "Breakpoint " << id << " set at " << normalizedFile << ":" << line << "\n";
    return id;
}

void Debugger::removeBreakpoint(uint32_t id) {
    auto it = breakpoints.find(id);
    if (it != breakpoints.end()) {
        std::cout << "Breakpoint " << id << " removed.\n";
        breakpoints.erase(it);
    } else {
        std::cout << "Breakpoint " << id << " not found.\n";
    }
}

void Debugger::enableBreakpoint(uint32_t id) {
    auto it = breakpoints.find(id);
    if (it != breakpoints.end()) {
        it->second->enabled = true;
        std::cout << "Breakpoint " << id << " enabled.\n";
    }
}

void Debugger::disableBreakpoint(uint32_t id) {
    auto it = breakpoints.find(id);
    if (it != breakpoints.end()) {
        it->second->enabled = false;
        std::cout << "Breakpoint " << id << " disabled.\n";
    }
}

void Debugger::listBreakpoints() const {
    if (breakpoints.empty()) {
        std::cout << "No breakpoints set.\n";
        return;
    }

    std::cout << "Breakpoints:\n";
    for (const auto &[id, bp]: breakpoints) {
        std::cout << "  " << id << ": " << bp->file << ":" << bp->line
                << (bp->enabled ? "" : " (disabled)") << "\n";
    }
}

void Debugger::printStack() const {
    if (vm->stack_manager.sp == 0) {
        std::cout << "Stack is empty.\n";
        return;
    }

    std::cout << "Stack (top to bottom):\n";
    for (int32_t i = vm->stack_manager.sp - 1; i >= 0; i--) {
        std::cout << "  [" << i << "] = " << vm->stack_manager.stack[i].toString() << "\n";
    }
}

void Debugger::printLocals() const {
    if (vm->call_frames.empty()) {
        std::cout << "No active call frame.\n";
        return;
    }

    const CallFrame &frame = vm->call_frames.back();
    const Function *func = frame.function;

    // Find debug info for this function
    uint32_t funcIdx = UINT32_MAX;
    for (size_t i = 0; i < vm->functions.size(); i++) {
        if (vm->functions[i].get() == func) {
            funcIdx = static_cast<uint32_t>(i);
            break;
        }
    }

    if (funcIdx == UINT32_MAX) {
        std::cout << "Debug info not available for current function.\n";
        return;
    }

    auto it = functionDebugInfo.find(funcIdx);
    if (it == functionDebugInfo.end()) {
        std::cout << "Debug info not available for current function.\n";
        return;
    }

    const FunctionDebugInfo &debugInfo = it->second;
    std::cout << "Local variables in " << debugInfo.name << ":\n";

    for (const auto &[name, slot]: debugInfo.localVariables) {
        uint32_t abs = frame.localStartsAt + slot;
        if (abs < vm->stack_manager.sp) {
            std::cout << "  " << name << " = " << vm->stack_manager.stack[abs].toString() << "\n";
        } else {
            std::cout << "  " << name << " = <uninitialized>\n";
        }
    }
}

void Debugger::printGlobals() const {
    if (vm->globals.empty()) {
        std::cout << "No global variables.\n";
        return;
    }

    std::cout << "Global variables:\n";
    for (const auto &[name, value]: vm->globals) {
        std::cout << "  " << name << " = " << value.toString() << "\n";
    }
}

void Debugger::printVariable(const std::string &name) const {
    if (vm->call_frames.empty()) {
        std::cout << "No active call frame.\n";
        return;
    }

    const CallFrame &frame = vm->call_frames.back();
    const Function *func = frame.function;

    // Find function index
    uint32_t funcIdx = UINT32_MAX;
    for (size_t i = 0; i < vm->functions.size(); i++) {
        if (vm->functions[i].get() == func) {
            funcIdx = static_cast<uint32_t>(i);
            break;
        }
    }

    // Try to find as local variable
    if (funcIdx != UINT32_MAX) {
        auto it = functionDebugInfo.find(funcIdx);
        if (it != functionDebugInfo.end()) {
            const auto &locals = it->second.localVariables;
            auto localIt = locals.find(name);
            if (localIt != locals.end()) {
                uint32_t abs = frame.localStartsAt + localIt->second;
                if (abs < vm->stack_manager.sp) {
                    std::cout << name << " = " << vm->stack_manager.stack[abs].toString() << "\n";
                } else {
                    std::cout << name << " = <uninitialized>\n";
                }
                return;
            }
        }
    }

    // Try to find as global
    auto globalIt = vm->globals.find(name);
    if (globalIt != vm->globals.end()) {
        std::cout << name << " = " << globalIt->second.toString() << "\n";
        return;
    }

    std::cout << "Variable '" << name << "' not found.\n";
}

void Debugger::printBacktrace() const {
    if (vm->call_frames.empty()) {
        std::cout << "No active call frames.\n";
        return;
    }

    std::cout << "Call stack:\n";
    for (int i = static_cast<int>(vm->call_frames.size()) - 1; i >= 0; i--) {
        const CallFrame &frame = vm->call_frames[i];

        // Find function index and debug info
        uint32_t funcIdx = UINT32_MAX;
        for (size_t j = 0; j < vm->functions.size(); j++) {
            if (vm->functions[j].get() == frame.function) {
                funcIdx = static_cast<uint32_t>(j);
                break;
            }
        }

        std::cout << "#" << (vm->call_frames.size() - 1 - i) << " ";

        if (funcIdx != UINT32_MAX) {
            auto it = functionDebugInfo.find(funcIdx);
            if (it != functionDebugInfo.end()) {
                const FunctionDebugInfo &debugInfo = it->second;
                auto locIt = debugInfo.ipToLocation.find(frame.ip);
                if (locIt != debugInfo.ipToLocation.end()) {
                    std::cout << debugInfo.name << " at "
                            << locIt->second.file << ":" << locIt->second.line << "\n";
                } else {
                    std::cout << debugInfo.name << " at unknown location\n";
                }
            } else {
                std::cout << "function_" << funcIdx << " at unknown location\n";
            }
        } else {
            std::cout << "<unknown function>\n";
        }
    }
}

void Debugger::printCurrentLocation() const {
    SourceLocation loc = getCurrentLocation();
    if (loc.file.empty()) {
        std::cout << "Current location unknown.\n";
        return;
    }

    std::cout << "At " << loc.file << ":" << loc.line << "\n";
    std::string line = getSourceLine(loc.file, loc.line);
    if (!line.empty()) {
        std::cout << std::setw(4) << loc.line << " | " << line << "\n";
    }
}

void Debugger::listSource(int contextLines) const {
    SourceLocation loc = getCurrentLocation();
    if (loc.file.empty()) {
        std::cout << "Source location unknown.\n";
        return;
    }

    auto it = sourceFiles.find(loc.file);
    if (it == sourceFiles.end()) {
        std::cout << "Source file not available.\n";
        return;
    }

    const std::vector<std::string> &lines = it->second;
    uint32_t startLine = loc.line > static_cast<uint32_t>(contextLines)
                             ? loc.line - contextLines
                             : 1;
    uint32_t endLine = std::min(loc.line + contextLines, static_cast<uint32_t>(lines.size()));

    for (uint32_t i = startLine; i <= endLine; i++) {
        std::string marker = (i == loc.line) ? "=> " : "   ";
        std::cout << marker << std::setw(4) << i << " | " << lines[i - 1] << "\n";
    }
}

void Debugger::addFunctionDebugInfo(uint32_t funcIdx, const FunctionDebugInfo &info) {
    functionDebugInfo[funcIdx] = info;
}

void Debugger::setSourceFile(const std::string &file, const std::vector<std::string> &lines) {
    sourceFiles[file] = lines;
}

bool Debugger::shouldBreak() {
    if (!isRunning) {
        return true;
    }

    // Check step mode
    switch (stepMode) {
        case StepMode::STEP_INTO:
            return true;

        case StepMode::STEP_NEXT_LINE: {
            // Break if we're on a different line than when we started
            SourceLocation loc = getCurrentLocation();
            if (!loc.file.empty() && loc.line != currentStepLine) {
                return true;
            }
            return false;
        }

        case StepMode::STEP_OVER:
        case StepMode::STEP_OUT:
            if (vm->call_frames.size() <= stepFrameDepth) {
                return true;
            }
            break;

        case StepMode::CONTINUE:
        {
            SourceLocation loc = getCurrentLocation();
            if (!loc.file.empty() && hasBreakpointAt(loc.file, loc.line)) {
                // FIX: When we hit a breakpoint, switch to STEP_INTO mode
                // so we don't immediately continue again
                stepMode = StepMode::STEP_INTO;
                std::cout << "\nBreakpoint hit at " << loc.file << ":" << loc.line << "\n";
                return true;
            }
        }
        break;

        case StepMode::NONE:
            return false;
    }

    return false;
}

void Debugger::debugLoop() {
    // Only show the current line, not the full source context
    printCurrentLocation();

    while (true) {
        printPrompt();

        std::string input;
        if (!std::getline(std::cin, input)) {
            break;
        }

        if (input.empty()) {
            input = "next";
        }

        std::vector<std::string> tokens = tokenizeCommand(input);
        if (tokens.empty()) {
            continue;
        }

        executeCommand(tokens);

        if (isRunning) {
            break;
        }
    }
}

bool Debugger::hasBreakpointAt(const std::string &file, uint32_t line) const {
    std::string normalizedFile = toLower(file);
    for (const auto &[id, bp]: breakpoints) {
        if (bp->enabled && toLower(bp->file) == normalizedFile && bp->line == line) {
            return true;
        }
    }
    return false;
}

SourceLocation Debugger::getCurrentLocation() const {
    if (vm->call_frames.empty()) {
        return {};
    }

    const CallFrame &frame = vm->call_frames.back();
    const Function *func = frame.function;

    // Find function index
    uint32_t funcIdx = UINT32_MAX;
    for (size_t i = 0; i < vm->functions.size(); i++) {
        if (vm->functions[i].get() == func) {
            funcIdx = static_cast<uint32_t>(i);
            break;
        }
    }

    if (funcIdx == UINT32_MAX) {
        return {};
    }

    auto it = functionDebugInfo.find(funcIdx);
    if (it == functionDebugInfo.end()) {
        return {};
    }

    const FunctionDebugInfo &debugInfo = it->second;

    // First try exact match
    auto locIt = debugInfo.ipToLocation.find(frame.ip);
    if (locIt != debugInfo.ipToLocation.end()) {
        return locIt->second;
    }

    // If no exact match, find the closest previous IP with a location
    SourceLocation lastKnown;
    uint32_t closestIP = 0;

    for (const auto &[ip, loc]: debugInfo.ipToLocation) {
        if (ip <= frame.ip && ip >= closestIP) {
            closestIP = ip;
            lastKnown = loc;
        }
    }

    if (!lastKnown.file.empty()) {
        return lastKnown;
    }

    return {};
}

std::string Debugger::getSourceLine(const std::string &file, uint32_t line) const {
    auto it = sourceFiles.find(file);
    if (it == sourceFiles.end() || line == 0 || line > it->second.size()) {
        return "";
    }
    return it->second[line - 1];
}

void Debugger::printPrompt() {
    std::cout << "(droplet-db) ";
}

std::vector<std::string> Debugger::tokenizeCommand(const std::string &cmd) {
    std::vector<std::string> tokens;
    std::istringstream iss(cmd);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

void Debugger::executeCommand(const std::vector<std::string> &tokens) {
    const std::string &cmd = tokens[0];

    if (cmd == "step" || cmd == "s") {
        handleStep(tokens);
    } else if (cmd == "next" || cmd == "n") {
        handleNext(tokens);
    } else if (cmd == "stepi" || cmd == "si") {
        handleStepInstruction(tokens);
    } else if (cmd == "finish" || cmd == "fin") {
        handleFinish(tokens);
    } else if (cmd == "continue" || cmd == "c") {
        handleContinue(tokens);
    } else if (cmd == "break" || cmd == "b") {
        handleBreak(tokens);
    } else if (cmd == "info" || cmd == "i") {
        handleInfo(tokens);
    } else if (cmd == "print" || cmd == "p") {
        handlePrint(tokens);
    } else if (cmd == "list" || cmd == "l") {
        handleList(tokens);
    } else if (cmd == "backtrace" || cmd == "bt" || cmd == "where") {
        handleBacktrace(tokens);
    } else if (cmd == "quit" || cmd == "q") {
        handleQuit(tokens);
    } else if (cmd == "help" || cmd == "h") {
        handleHelp(tokens);
    } else if (cmd == "clear" || cmd == "cls") {
        handleClear(tokens);
    } else {
        std::cout << "Unknown command: " << cmd << ". Type 'help' for available commands.\n";
    }
}

void Debugger::handleClear(const std::vector<std::string> &args) {
#if defined(_WIN32)
    system("cls");
#else
    std::cout << "\033[2J\033[H"; // ANSI escape codes: clear screen + move cursor to home
#endif
}


void Debugger::handleStep(const std::vector<std::string> &args) {
    stepNextLine();
}

void Debugger::handleNext(const std::vector<std::string> &args) {
    stepNextLine();
}

void Debugger::handleStepInstruction(const std::vector<std::string> &args) {
    stepInto();
}

void Debugger::handleFinish(const std::vector<std::string> &args) {
    stepOut();
}

void Debugger::handleContinue(const std::vector<std::string> &args) {
    continueExecution();
}

void Debugger::handleBreak(const std::vector<std::string> &args) {
    if (args.size() < 2) {
        std::cout << "Usage: break <file>:<line> or break <line>\n";
        return;
    }

    const std::string &location = args[1];
    std::string file;
    uint32_t line;

    size_t colonPos = location.rfind(':'); // find last colon
    if (colonPos != std::string::npos) {
        file = location.substr(0, colonPos);
        line = std::stoul(location.substr(colonPos + 1));
    } else {
        // Use current file
        SourceLocation loc = getCurrentLocation();
        file = loc.file;
        line = std::stoul(location);
    }


    addBreakpoint(file, line);
}

void Debugger::handleInfo(const std::vector<std::string> &args) const {
    if (args.size() < 2) {
        std::cout << "Usage: info <breakpoints|locals|globals|stack>\n";
        return;
    }

    const std::string &what = args[1];
    if (what == "breakpoints" || what == "b") {
        listBreakpoints();
    } else if (what == "locals" || what == "l") {
        printLocals();
    } else if (what == "globals" || what == "g") {
        printGlobals();
    } else if (what == "stack" || what == "s") {
        printStack();
    } else {
        std::cout << "Unknown info command: " << what << "\n";
    }
}

void Debugger::handlePrint(const std::vector<std::string> &args) const {
    if (args.size() < 2) {
        std::cout << "Usage: print <variable>\n";
        return;
    }

    printVariable(args[1]);
}

void Debugger::handleList(const std::vector<std::string> &args) const {
    int contextLines = 5;
    if (args.size() >= 2) {
        contextLines = std::stoi(args[1]);
    }
    listSource(contextLines);
}

void Debugger::handleBacktrace(const std::vector<std::string> &args) const {
    printBacktrace();
}

void Debugger::handleQuit(const std::vector<std::string> &args) {
    stop();
    std::cout << "Quitting debugger.\n";
    exit(0);
}

void Debugger::handleHelp(const std::vector<std::string> &args) {
    std::cout << "Droplet Debugger Commands:\n\n";
    std::cout << "Execution Control:\n";
    std::cout << "  step, s, next, n    Step to next source line\n";
    std::cout << "  stepi, si           Step one instruction (low-level)\n";
    std::cout << "  finish, fin         Step out (continue until current function returns)\n";
    std::cout << "  continue, c         Continue execution until breakpoint\n";
    std::cout << "  quit, q             Exit debugger\n\n";

    std::cout << "Breakpoints:\n";
    std::cout << "  break <file>:<line> Set breakpoint at file:line\n";
    std::cout << "  break <line>        Set breakpoint at line in current file\n";
    std::cout << "  info breakpoints    List all breakpoints\n\n";

    std::cout << "Inspection:\n";
    std::cout << "  print <var>, p      Print variable value\n";
    std::cout << "  info locals         Show local variables\n";
    std::cout << "  info globals        Show global variables\n";
    std::cout << "  info stack          Show stack contents\n";
    std::cout << "  backtrace, bt       Show call stack\n";
    std::cout << "  list [n], l         List source code (n lines of context)\n\n";

    std::cout << "Utility:\n";
    std::cout << "  clear, cls          Clear the console screen\n\n";

    std::cout << "Notes:\n";
    std::cout << "  - Pressing Enter repeats the last step/next command\n";
    std::cout << "  - 'step' and 'next' move to the next source line\n";
    std::cout << "  - Use 'stepi' for instruction-level debugging\n";
    std::cout << "  - Use 'list' command to see more source context when needed\n\n";
}
