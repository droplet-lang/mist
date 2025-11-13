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
 *  File: Lexer_test
 *  Created: 11/9/2025
 * ============================================================
 */
#ifndef DROPLET_LEXER_TEST_H
#define DROPLET_LEXER_TEST_H


#include <iostream>
#include "../compiler/Lexer.h"

inline void TEST_LEXER() {
    std::string source = R"(
        @ffi
        @deprecated(msg="This feature is depreciated")
        class Example {
            pub fn add(a: int, b: int) -> int {
                let sum = a + b;
                return sum;
            }

            fn greet() {
                print("Hello, Droplet!");
            }
        }

        fn main() {
            let ex = new Example();
            ex.greet();
            print(ex.add(5, 10));
        }
    )";

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();

        for (const auto& token : tokens) {
            std::cout << "Token("
                      << static_cast<int>(token.type) << ", "
                      << "\"" << token.lexeme << "\", "
                      << "line=" << token.line << ", "
                      << "col=" << token.column << ")\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Lexer error: " << e.what() << std::endl;
    }
}


#endif //DROPLET_LEXER_TEST_H