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
 *  File: Object
 *  Created: 11/7/2025
 * ============================================================
 */
#ifndef DROPLET_OBJECT_H
#define DROPLET_OBJECT_H

#include <ranges>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "defines.h"


// Object will be fully manager by the Garbage Collector
struct Object {
    virtual ~Object() = default;

    [[nodiscard]] virtual std::string get_representor() const = 0;

    // for mark and sweep GC
    bool marked = false;

    virtual void markChildren(MarkerFunction fn) = 0;
};

/*
 * There will be primarily support for
 * Array, String, Map and Object Instance
 * I think this should handle all the type of values that we can handle
 *
 * Normal primitive value will be handled as normal values (see Value) and other will be handled as Object
*/
struct ObjString final : Object {
    std::string value;

    explicit ObjString(std::string v) : value(std::move(v)) {
    }

    [[nodiscard]] std::string get_representor() const override {
        // "hello"
        return "\"" + value + "\"";
    }

    void markChildren(const MarkerFunction fn) override {
        // to shut up compiler
        // warning: unused parameter fn...
        (void) fn;
    }
};

struct ObjArray final : Object {
    // will be easy to manage, can even get len, cap later lol
    std::vector<Value> value;

    [[nodiscard]] std::string get_representor() const override {
        // <array>
        return "<array>";
    }

    void markChildren(const MarkerFunction fn) override {
        for (const auto &v: value) fn(v);
    }
};

struct ObjMap final : Object {
    // Todo: (@svpz) change to hash based implementation
    // for initial implementation, lets treat key as string
    // this will be easy for us now
    // later we can rely on hash based implementation
    std::unordered_map<std::string, Value> value;

    std::string get_representor() const override {
        return "<map>";
    }

    void markChildren(const MarkerFunction fn) override {
        for (const auto &val: value) fn(val.second);
    }
};

struct ObjInstance final : Object {
    std::string className;
    std::unordered_map<std::string, Value> fields;

    explicit ObjInstance(std::string cn = "Object") : className(std::move(cn)) {
    }

    std::string get_representor() const override {
        // <object:HelloWorld>
        return "<object:" + className + ">";
    }

    void markChildren(const MarkerFunction fn) override {
        for (const auto &val: fields) fn(val.second);
    }
};

#endif //DROPLET_OBJECT_H