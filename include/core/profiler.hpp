/*
 * profiler.h
 *
 *  Created on: Nov 26, 2016
 *      Author: nullifiedcat
 */

#pragma once

#include <chrono>
#include <string>
#include "config.h"

class ProfilerNode;

class ProfilerSection
{
public:
    ProfilerSection(std::string name, ProfilerSection *parent = nullptr);

    void OnNodeDeath(ProfilerNode &node);

    std::chrono::nanoseconds m_min;
    std::chrono::nanoseconds m_max;
    std::chrono::nanoseconds m_sum;
    unsigned m_spewcount;
    unsigned m_calls;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_log;
    std::string m_name;
    ProfilerSection *m_parent;
};

class ProfilerNode
{
public:
    ProfilerNode(ProfilerSection &section);
    ~ProfilerNode();

    std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
    ProfilerSection &m_section;
};

#if ENABLE_PROFILER
#define PROF_SECTION(id)                          \
    static ProfilerSection __PROFILER__##id(#id); \
    volatile ProfilerNode __PROFILER_NODE__##id(__PROFILER__##id);
#else
#define PROF_SECTION(id)
#endif
