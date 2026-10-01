/********************************************************************************
 * Copyright (C) 2017-2026 LG Electronics Inc.
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ********************************************************************************/

#ifndef LG_SOMEIP_OSABSTRACTION_LOGGER_LOGGER_H
#define LG_SOMEIP_OSABSTRACTION_LOGGER_LOGGER_H

#include <atomic>
#include <functional>
#include <mutex>
#include <ostream>
#include <sstream>
#include <streambuf>
#include <string>
#include <thread>
#include <utility>

#if defined(ENABLE_DLT)
#include <dlt/dlt.h>
#else
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <cstdio>
#include <sys/time.h>
#include <cmath>
#include <iomanip>

#if defined(LINUX)
#include <sys/syscall.h>
#endif // LINUX
#endif // ENABLE_DLT

namespace lgsomeip {

using LogStream = std::ostream;

#if !defined(ENABLE_DLT)
inline std::size_t get_thread_id() {
#if defined(LINUX)
    return static_cast<std::size_t>(syscall(SYS_gettid));
#else
    return std::hash<std::thread::id>{}(std::this_thread::get_id());
#endif
}

#if defined(_GNU_SOURCE)
static const char* app_name = program_invocation_short_name;
#else
extern char* __progname;
static const char* app_name = __progname;
#endif // _GNU_SOURCE
#endif // ENABLE_DLT

class Logger {
public:
    enum class LogLevel : unsigned char { Off, Fatal, Error, Warn, Info, Debug, Verbose };

public:
    Logger(const Logger&) = delete;
    void operator=(const Logger&) = delete;

    // Sets the console log level (no-op when built with ENABLE_DLT;
    // the DLT daemon controls verbosity at runtime).
    void init(std::string log_level) {
#if !defined(ENABLE_DLT)
        init(std::move(log_level), true, false, "");
#else
        (void)log_level;
#endif
    }

    void init(std::string log_level, bool console_enabled, bool file_enabled, std::string file_path) {
#if !defined(ENABLE_DLT)
        const LogLevel level = parse_log_level(log_level);
        log_level_.store(level, std::memory_order_relaxed);

        {
            std::lock_guard<std::mutex> lock(output_mutex_);
            console_enabled_.store(console_enabled, std::memory_order_relaxed);
            file_enabled_.store(false, std::memory_order_relaxed);
            if (log_file_.is_open()) {
                log_file_.close();
            }
            if (file_enabled && !file_path.empty()) {
                log_file_.open(file_path.c_str(), std::ios::out | std::ios::app);
                file_enabled_.store(log_file_.is_open(), std::memory_order_relaxed);
            }
        }

        std::ostringstream output_stream;
        output_stream << "The level of log is set to " << log_level << " explicitly!"
                      << ", log_level_: " << level_name(level);
        write_record(LogLevel::Info, " [Info] ", output_stream.str());
#else
        (void)log_level;
        (void)console_enabled;
        (void)file_enabled;
        (void)file_path;
#endif
    }

    static Logger& instance() {
        static Logger instance;
        return instance;
    }

    bool is_enabled(LogLevel level) {
#if defined(ENABLE_DLT)
        const DltLogLevelType dlt_level = dlt_level_for(level);
        return dlt_level != DLT_LOG_OFF && DLT_IS_LOG_LEVEL_ENABLED(dlt_context_, dlt_level);
#else
        return should_log(level) &&
               (console_enabled_.load(std::memory_order_relaxed) || file_enabled_.load(std::memory_order_relaxed));
#endif
    }

    class LogMessage : public std::ostream {
    public:
        LogMessage(Logger& logger, LogLevel level, const char* level_name)
            : std::ostream(nullptr), logger_(logger), level_(level), level_name_(level_name),
              buffer_(logger.is_enabled(level)) {
            rdbuf(&buffer_);
        }

        template <typename Log> LogMessage& operator()(Log&& print_log) {
            if (buffer_.enabled()) {
                print_log(*this);
            }
            return *this;
        }

        ~LogMessage() noexcept override {
            try {
                if (buffer_.enabled()) {
                    logger_.dispatch(level_, level_name_, buffer_.take());
                }
            } catch (...) {
            }
        }

        LogMessage(const LogMessage&) = delete;
        LogMessage& operator=(const LogMessage&) = delete;

    private:
        class MessageBuffer : public std::streambuf {
        public:
            explicit MessageBuffer(bool enabled) : enabled_(enabled) {}

            bool enabled() const {
                return enabled_;
            }

            std::string take() {
                return std::move(message_);
            }

        protected:
            int_type overflow(int_type character) override {
                if (enabled_ && !traits_type::eq_int_type(character, traits_type::eof())) {
                    message_.push_back(static_cast<char>(character));
                }
                return traits_type::not_eof(character);
            }

            std::streamsize xsputn(const char* data, std::streamsize size) override {
                if (enabled_) {
                    message_.append(data, static_cast<std::size_t>(size));
                }
                return size;
            }

        private:
            bool enabled_;
            std::string message_;
        };

        Logger& logger_;
        LogLevel level_;
        const char* level_name_;
        MessageBuffer buffer_;
    };

    template <typename Log> void log_fatal(Log&& print_log) {
#if defined(ENABLE_DLT)
        if (!is_enabled(LogLevel::Fatal)) {
            return;
        }
        std::ostringstream output_stream;
        LogStream& log_stream = output_stream;
        print_log(log_stream);
        DLT_LOG_STRING(dlt_context_, DLT_LOG_FATAL, output_stream.str().c_str());
#else
        log(LogLevel::Fatal, " [Fatal] ", std::forward<Log>(print_log));
#endif
    }

    template <typename Log> void log_fatal(const char* file, int line, const char* function, Log&& print_log) {
#if defined(ENABLE_DLT)
        (void)file;
        (void)line;
        (void)function;
        log_fatal(std::forward<Log>(print_log));
#else
        log(LogLevel::Fatal, " [Fatal] ", file, line, function, std::forward<Log>(print_log));
#endif
    }

    template <typename Log> void log_error(Log&& print_log) {
#if defined(ENABLE_DLT)
        if (!is_enabled(LogLevel::Error)) {
            return;
        }
        std::ostringstream output_stream;
        LogStream& log_stream = output_stream;
        print_log(log_stream);
        DLT_LOG_STRING(dlt_context_, DLT_LOG_ERROR, output_stream.str().c_str());
#else
        log(LogLevel::Error, " [Error] ", std::forward<Log>(print_log));
#endif
    }

    template <typename Log> void log_error(const char* file, int line, const char* function, Log&& print_log) {
#if defined(ENABLE_DLT)
        (void)file;
        (void)line;
        (void)function;
        log_error(std::forward<Log>(print_log));
#else
        log(LogLevel::Error, " [Error] ", file, line, function, std::forward<Log>(print_log));
#endif
    }

    template <typename Log> void log_warn(Log&& print_log) {
#if defined(ENABLE_DLT)
        if (!is_enabled(LogLevel::Warn)) {
            return;
        }
        std::ostringstream output_stream;
        LogStream& log_stream = output_stream;
        print_log(log_stream);
        DLT_LOG_STRING(dlt_context_, DLT_LOG_WARN, output_stream.str().c_str());
#else
        log(LogLevel::Warn, " [Warn] ", std::forward<Log>(print_log));
#endif
    }

    template <typename Log> void log_warn(const char* file, int line, const char* function, Log&& print_log) {
#if defined(ENABLE_DLT)
        (void)file;
        (void)line;
        (void)function;
        log_warn(std::forward<Log>(print_log));
#else
        log(LogLevel::Warn, " [Warn] ", file, line, function, std::forward<Log>(print_log));
#endif
    }

    template <typename Log> void log_info(Log&& print_log) {
#if defined(ENABLE_DLT)
        if (!is_enabled(LogLevel::Info)) {
            return;
        }
        std::ostringstream output_stream;
        LogStream& log_stream = output_stream;
        print_log(log_stream);
        DLT_LOG_STRING(dlt_context_, DLT_LOG_INFO, output_stream.str().c_str());
#else
        log(LogLevel::Info, " [Info] ", std::forward<Log>(print_log));
#endif
    }

    template <typename Log> void log_info(const char* file, int line, const char* function, Log&& print_log) {
#if defined(ENABLE_DLT)
        (void)file;
        (void)line;
        (void)function;
        log_info(std::forward<Log>(print_log));
#else
        log(LogLevel::Info, " [Info] ", file, line, function, std::forward<Log>(print_log));
#endif
    }

    template <typename Log> void log_debug(Log&& print_log) {
#if defined(ENABLE_DLT)
        if (!is_enabled(LogLevel::Debug)) {
            return;
        }
        std::ostringstream output_stream;
        LogStream& log_stream = output_stream;
        print_log(log_stream);
        DLT_LOG_STRING(dlt_context_, DLT_LOG_DEBUG, output_stream.str().c_str());
#else
        log(LogLevel::Debug, " [Debug] ", std::forward<Log>(print_log));
#endif
    }

    template <typename Log> void log_debug(const char* file, int line, const char* function, Log&& print_log) {
#if defined(ENABLE_DLT)
        (void)file;
        (void)line;
        (void)function;
        log_debug(std::forward<Log>(print_log));
#else
        log(LogLevel::Debug, " [Debug] ", file, line, function, std::forward<Log>(print_log));
#endif
    }

    template <typename Log> void log_verbose(Log&& print_log) {
#if defined(ENABLE_DLT)
        if (!is_enabled(LogLevel::Verbose)) {
            return;
        }
        std::ostringstream output_stream;
        LogStream& log_stream = output_stream;
        print_log(log_stream);
        DLT_LOG_STRING(dlt_context_, DLT_LOG_VERBOSE, output_stream.str().c_str());
#else
        log(LogLevel::Verbose, " [Verbose] ", std::forward<Log>(print_log));
#endif
    }

    template <typename Log> void log_verbose(const char* file, int line, const char* function, Log&& print_log) {
#if defined(ENABLE_DLT)
        (void)file;
        (void)line;
        (void)function;
        log_verbose(std::forward<Log>(print_log));
#else
        log(LogLevel::Verbose, " [Verbose] ", file, line, function, std::forward<Log>(print_log));
#endif
    }

private:
#if defined(ENABLE_DLT)
    static DltLogLevelType dlt_level_for(LogLevel level) {
        switch (level) {
        case LogLevel::Fatal:
            return DLT_LOG_FATAL;
        case LogLevel::Error:
            return DLT_LOG_ERROR;
        case LogLevel::Warn:
            return DLT_LOG_WARN;
        case LogLevel::Info:
            return DLT_LOG_INFO;
        case LogLevel::Debug:
            return DLT_LOG_DEBUG;
        case LogLevel::Verbose:
            return DLT_LOG_VERBOSE;
        case LogLevel::Off:
            return DLT_LOG_OFF;
        }
        return DLT_LOG_OFF;
    }
#endif

    void dispatch(LogLevel level, const char* level_name, std::string message) {
#if defined(ENABLE_DLT)
        const DltLogLevelType dlt_level = dlt_level_for(level);
        if (dlt_level == DLT_LOG_OFF) {
            return;
        }
        (void)level_name;
        DLT_LOG_STRING(dlt_context_, dlt_level, message.c_str());
#else
        write_record(level, level_name, std::move(message));
#endif
    }

#if defined(ENABLE_DLT)
    DltContext dlt_context_;
    Logger() {
        dlt_enable_local_print();
        DLT_REGISTER_APP("LSIP", "LG SOME/IP");
        DLT_REGISTER_CONTEXT(dlt_context_, "LSIP", "LG SOME/IP logging context");
    }
    ~Logger() {
        DLT_UNREGISTER_CONTEXT(dlt_context_);
        DLT_UNREGISTER_APP();
    }
#else
    struct LogRecord {
        LogLevel level;
        const char* level_name;
        std::string message;
        std::chrono::system_clock::time_point timestamp;
        std::size_t thread_id;
    };

    static LogLevel parse_log_level(std::string log_level) {
        for (char& character : log_level) {
            if (character >= 'A' && character <= 'Z') {
                character = static_cast<char>(character - 'A' + 'a');
            }
        }

        if (!log_level.empty() && log_level.front() == 'k') {
            log_level.erase(log_level.begin());
        }

        if (log_level == "off")
            return LogLevel::Off;
        if (log_level == "fatal")
            return LogLevel::Fatal;
        if (log_level == "error")
            return LogLevel::Error;
        if (log_level == "warn" || log_level == "warning")
            return LogLevel::Warn;
        if (log_level == "debug")
            return LogLevel::Debug;
        if (log_level == "verbose" || log_level == "trace")
            return LogLevel::Verbose;
        return LogLevel::Info;
    }

    static const char* level_name(LogLevel level) {
        switch (level) {
        case LogLevel::Off:
            return "off";
        case LogLevel::Fatal:
            return "fatal";
        case LogLevel::Error:
            return "error";
        case LogLevel::Warn:
            return "warn";
        case LogLevel::Info:
            return "info";
        case LogLevel::Debug:
            return "debug";
        case LogLevel::Verbose:
            return "verbose";
        }
        return "info";
    }

    bool should_log(LogLevel level) const {
        return static_cast<unsigned int>(log_level_.load(std::memory_order_relaxed)) >=
               static_cast<unsigned int>(level);
    }

    template <typename Log> void log(LogLevel level, const char* level_name_text, Log&& print_log) {
        log(level, level_name_text, nullptr, 0, nullptr, std::forward<Log>(print_log));
    }

    template <typename Log>
    void log(LogLevel level, const char* level_name_text, const char* file, int line, const char* function,
             Log&& print_log) {
        if (!should_log(level) ||
            (!console_enabled_.load(std::memory_order_relaxed) && !file_enabled_.load(std::memory_order_relaxed))) {
            return;
        }

        std::ostringstream output_stream;
        print_log(output_stream);
        (void)file;
        (void)line;
        (void)function;
        write_record(level, level_name_text, output_stream.str());
    }

    void write_record(LogLevel level, const char* level_name_text, std::string message) {
        const LogRecord record{level, level_name_text, std::move(message), std::chrono::system_clock::now(),
                               get_thread_id()};
        const auto duration = record.timestamp.time_since_epoch();
        const auto seconds_duration = std::chrono::duration_cast<std::chrono::seconds>(duration);
        const std::time_t seconds = static_cast<std::time_t>(seconds_duration.count());
        const auto microseconds =
            std::chrono::duration_cast<std::chrono::microseconds>(duration - seconds_duration).count();
        char time_buffer[26] = {};
        tm time_info{};
        localtime_r(&seconds, &time_info);
        strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", &time_info);

        std::ostringstream output_stream;
        output_stream << "[" << app_name << ":" << record.thread_id << "] " << time_buffer << "." << std::setw(6)
                      << std::setfill('0') << microseconds << record.level_name;
        output_stream << record.message << '\n';
        const std::string output = output_stream.str();

        std::lock_guard<std::mutex> lock(output_mutex_);
        if (console_enabled_.load(std::memory_order_relaxed)) {
            std::cout << output;
            std::cout.flush();
        }
        if (file_enabled_.load(std::memory_order_relaxed) && log_file_.is_open()) {
            log_file_ << output;
            log_file_.flush();
        }
    }

    std::atomic<LogLevel> log_level_{LogLevel::Info};
    std::atomic<bool> console_enabled_{true};
    std::atomic<bool> file_enabled_{false};
    std::mutex output_mutex_;
    std::ofstream log_file_;
    Logger() {}
#endif
};

} // namespace lgsomeip

// Common expansion shared by all LGSOMEIP_LOG_* macros below; LEVEL and LEVEL_NAME are substituted per level.
#define LGSOMEIP_LOG_IMPL(LEVEL, LEVEL_NAME)                                                                           \
    for (bool lgsomeip_log_enabled = lgsomeip::Logger::instance().is_enabled(lgsomeip::Logger::LogLevel::LEVEL);       \
         lgsomeip_log_enabled; lgsomeip_log_enabled = false)                                                           \
    lgsomeip::Logger::LogMessage(lgsomeip::Logger::instance(), lgsomeip::Logger::LogLevel::LEVEL, LEVEL_NAME)

#define LGSOMEIP_LOG_FATAL LGSOMEIP_LOG_IMPL(Fatal, " [Fatal] ")
#define LGSOMEIP_LOG_ERROR LGSOMEIP_LOG_IMPL(Error, " [Error] ")
#define LGSOMEIP_LOG_WARN LGSOMEIP_LOG_IMPL(Warn, " [Warn] ")
#define LGSOMEIP_LOG_INFO LGSOMEIP_LOG_IMPL(Info, " [Info] ")
#define LGSOMEIP_LOG_DEBUG LGSOMEIP_LOG_IMPL(Debug, " [Debug] ")
#define LGSOMEIP_LOG_VERBOSE LGSOMEIP_LOG_IMPL(Verbose, " [Verbose] ")

#endif // LG_SOMEIP_OSABSTRACTION_LOGGER_LOGGER_H
