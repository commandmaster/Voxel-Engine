#pragma once

#include "Timer.h"
#include "DebugUtils.hpp"
#include <type_traits>
#include <string>
#include <unordered_map>
#include <memory>

#define ENABLE_TIMING
//#define ENABLE_PERF_LOGGING
#define PERF_TIME_UNIT std::chrono::microseconds


class PerformanceTimer
{
public:
    static PerformanceTimer& getInstance()
    {
        static PerformanceTimer instance;
        return instance;
    }

    void beginSection(const std::string& sectionName)
    {
        if (timers.find(sectionName) == timers.end())
        {
            timers[sectionName] = std::make_unique<Timer>();
        }
        timers[sectionName]->start();
    }

    double endSection(const std::string& sectionName)
    {
        auto it = timers.find(sectionName);
        if (it != timers.end())
        {
            it->second->stop();
            perfStats[sectionName] = it->second->elapsedTime<PERF_TIME_UNIT>();
            return perfStats[sectionName];
        }
        return 0.0;
    }

    void resetSection(const std::string& sectionName)
    {
        auto it = timers.find(sectionName);
        if (it != timers.end())
        {
            it->second->start();
        }
    }

    std::unordered_map<std::string, double> perfStats;

private:
    PerformanceTimer() = default;
    std::unordered_map<std::string, std::unique_ptr<Timer>> timers;
};

// Convenience macros for performance timing
#ifdef ENABLE_TIMING
    #define PERF_BEGIN(sectionName) PerformanceTimer::getInstance().beginSection(sectionName)
    #define PERF_END(sectionName) PerformanceTimer::getInstance().endSection(sectionName)
    #define PERF_RESET(sectionName) PerformanceTimer::getInstance().resetSection(sectionName)
#else
    #define PERF_BEGIN(sectionName)
    #define PERF_END(sectionName)
    #define PERF_RESET(sectionName)
#endif

// RAII-style performance timing
class ScopedTimer
{
public:
    ScopedTimer(const std::string& sectionName)
        : sectionName(sectionName)
    {
        PERF_BEGIN(sectionName);
    }

    ~ScopedTimer()
    {
        double time = PERF_END(sectionName);
    
        #ifdef ENABLE_PERF_LOGGING

        if constexpr (std::is_same_v<PERF_TIME_UNIT, std::chrono::microseconds>)
        {
            LOG_VERBOSE("Section '" << sectionName << "' took " << time << "us");
        }
        else if constexpr (std::is_same_v<PERF_TIME_UNIT, std::chrono::milliseconds>)
        {
            LOG_VERBOSE("Section '" << sectionName << "' took " << time << "ms");
        }
        else if constexpr (std::is_same_v<PERF_TIME_UNIT, std::chrono::seconds>)
        {
            LOG_VERBOSE("Section '" << sectionName << "' took " << time << "s");
        }
        else
        {
            LOG_ERROR("Unsupported time unit: " << typeid(PERF_TIME_UNIT).name());
        }

        #endif

    }

private:
    std::string sectionName;
};

#ifdef ENABLE_TIMING
    #define PERF_SCOPE(sectionName) ScopedTimer perfTimer##__LINE__(sectionName) 
#else
    #define PERF_SCOPE(sectionName)
#endif
