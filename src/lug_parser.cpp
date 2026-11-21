#include <cstdio>
#include <sstream>
#include <cstdarg>

#include "lug.hpp"
#include "lug_parser.h"

namespace Lug {

// Helper functions -----------------------------

static std::string FormatString(const char* format, va_list args) {
    va_list sizeArgs;
    va_copy(sizeArgs, args);
    const int length = std::vsnprintf(nullptr, 0, format, sizeArgs);
    va_end(sizeArgs);
    if (length < 0) {
        LUG_ASSERT(false, "error message formatting failed");
    }

    std::string message(static_cast<std::size_t>(length) + 1, '\0');
    va_list formatArgs;
    va_copy(formatArgs, args);
    const int formattedLength = std::vsnprintf(message.data(), message.size(), format, formatArgs);
    va_end(formatArgs);
    if (formattedLength < 0) {
        LUG_ASSERT(false, "error message formatting failed");
    }

    message.resize(static_cast<std::size_t>(formattedLength));
    return message;
}

// Parser public functions -----------------------------

void Parser::Reset(const char* source, std::filesystem::path scrPath) {
    scriptPath = scrPath;

    lexer.Reset(source);
    Advance();
}

// Parser private functions -----------------------------

void Parser::Advance() {
    previous = current;
    current = lexer.ScanToken();

    if (current.type == TokenType::ERROR) {
        ErrorAtCurrent(current.start);
    }
}


void Parser::Consume(TokenType type, const char* errorMessage) {
    if (current.type == type) {
        Advance();
        return;
    }

    ErrorAtCurrent(errorMessage);
}


bool Parser::Match(TokenType expected) {
    if (!Check(expected)) return false;

    Advance();
    return true;
}


bool Parser::IsAtEnd() {
    return Check(TokenType::END_OF_FILE);
}


// Error functions -----------------------------

void Parser::ErrorAt(const Token& at, const char* format, ...) {
    Lug::Error error;
    error.type = InterpretResult::COMPILE_ERROR;
    
    va_list args;
    va_start(args, format);
    
    std::string message = FormatString(format, args);
    
    va_end(args);

    error.message = message;

    error.sourceLine = lexer.LineContents(at.line);
    error.token = std::string(at.start, at.length);

    error.location.tokenLength = static_cast<int>(at.length);
    error.location.line = at.line;
    error.location.column = at.column;
    error.location.file = scriptPath.filename().string();

    throw error;
}


void Parser::ErrorAtCurrent(const char* format, ...) {
    va_list args;
    va_start(args, format);
    ErrorAt(current, format, args);
    va_end(args);
}


void Parser::Error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    ErrorAt(previous, format, args);
    va_end(args);
}

} // Lug