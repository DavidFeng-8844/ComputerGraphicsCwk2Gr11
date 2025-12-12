#ifndef PERFORMANCE_TIMER_HPP_
#define PERFORMANCE_TIMER_HPP_

#include <glad/glad.h>
#include <chrono>
#include <array>
#include <string>
#include <vector>

// Enable/disable performance measurement with this macro
// Define ENABLE_PERFORMANCE_MEASUREMENT to enable timing code
// #define ENABLE_PERFORMANCE_MEASUREMENT

#ifdef ENABLE_PERFORMANCE_MEASUREMENT

// Number of frames to buffer for async query results (avoid stalling)
constexpr int QUERY_BUFFER_SIZE = 3;

// GPU Timer using GL_TIMESTAMP queries
// Uses double-buffering to avoid stalling when retrieving results
class GPUTimer
{
public:
	GPUTimer();
	~GPUTimer();

	// Initialize the timer (call after OpenGL context is created)
	void initialize();

	// Cleanup resources
	void cleanup();

	// Start timing a section (inserts a timestamp query)
	void start(const std::string &name);

	// End timing a section (inserts another timestamp query)
	void end(const std::string &name);

	// Call at the end of each frame to advance the query buffer
	void end_frame();

	// Get the last available result for a section (in milliseconds)
	// Returns -1.0 if no result is available yet
	double get_time_ms(const std::string &name) const;

	// Check if results are available (non-blocking)
	bool has_results() const;

private:
	struct TimerSection
	{
		std::string name;
		std::array<GLuint, QUERY_BUFFER_SIZE> startQueries;
		std::array<GLuint, QUERY_BUFFER_SIZE> endQueries;
		std::array<bool, QUERY_BUFFER_SIZE> queryActive;
		double lastTimeMs = -1.0;
	};

	std::vector<TimerSection> sections_;
	int currentFrame_ = 0;
	int resultFrame_ = 0;
	bool initialized_ = false;

	TimerSection *find_section(const std::string &name);
	const TimerSection *find_section(const std::string &name) const;
	TimerSection &get_or_create_section(const std::string &name);
};

// CPU Timer using std::chrono
class CPUTimer
{
public:
	// Start timing
	void start();

	// Stop timing and return elapsed time in milliseconds
	double stop_ms();

	// Get elapsed time without stopping (in milliseconds)
	double elapsed_ms() const;

private:
	std::chrono::high_resolution_clock::time_point startTime_;
};

// Performance statistics accumulator
class PerformanceStats
{
public:
	PerformanceStats(int sampleCount = 100);

	// Add a sample
	void add_sample(double value);

	// Get statistics
	double get_average() const;
	double get_min() const;
	double get_max() const;
	double get_last() const;
	int get_sample_count() const;

	// Reset statistics
	void reset();

private:
	std::vector<double> samples_;
	int maxSamples_;
	int currentIndex_ = 0;
	int sampleCount_ = 0;
	double sum_ = 0.0;
	double min_ = 1e9;
	double max_ = 0.0;
};

// Main performance measurement class
class PerformanceMeasurement
{
public:
	PerformanceMeasurement();
	~PerformanceMeasurement();

	// Initialize (call after OpenGL context is created)
	void initialize();

	// Cleanup
	void cleanup();

	// Frame timing
	void begin_frame();
	void end_frame();

	// Section timing (GPU)
	void begin_gpu_section(const std::string &name);
	void end_gpu_section(const std::string &name);

	// CPU timing for command submission
	void begin_cpu_timing();
	double end_cpu_timing_ms();

	// Get results
	double get_gpu_time_ms(const std::string &name) const;
	double get_frame_time_ms() const;
	double get_cpu_submission_time_ms() const;

	// Get statistics
	const PerformanceStats &get_stats(const std::string &name) const;
	const PerformanceStats &get_frame_stats() const;
	const PerformanceStats &get_cpu_stats() const;

	// Print summary to console
	void print_summary() const;

	// Check if ready to report
	bool has_results() const;

private:
	GPUTimer gpuTimer_;
	CPUTimer cpuTimer_;
	CPUTimer frameTimer_;

	std::vector<std::pair<std::string, PerformanceStats>> sectionStats_;
	PerformanceStats frameStats_;
	PerformanceStats cpuStats_;

	double lastCpuTime_ = 0.0;
	double lastFrameTime_ = 0.0;

	bool initialized_ = false;
	int frameCount_ = 0;

	PerformanceStats &get_or_create_stats(const std::string &name);
	const PerformanceStats *find_stats(const std::string &name) const;
};

#else // ENABLE_PERFORMANCE_MEASUREMENT not defined

// Stub implementations when performance measurement is disabled
class GPUTimer
{
public:
	void initialize() {}
	void cleanup() {}
	void start(const std::string &) {}
	void end(const std::string &) {}
	void end_frame() {}
	double get_time_ms(const std::string &) const { return 0.0; }
	bool has_results() const { return false; }
};

class CPUTimer
{
public:
	void start() {}
	double stop_ms() { return 0.0; }
	double elapsed_ms() const { return 0.0; }
};

class PerformanceStats
{
public:
	PerformanceStats(int = 100) {}
	void add_sample(double) {}
	double get_average() const { return 0.0; }
	double get_min() const { return 0.0; }
	double get_max() const { return 0.0; }
	double get_last() const { return 0.0; }
	int get_sample_count() const { return 0; }
	void reset() {}
};

class PerformanceMeasurement
{
public:
	void initialize() {}
	void cleanup() {}
	void begin_frame() {}
	void end_frame() {}
	void begin_gpu_section(const std::string &) {}
	void end_gpu_section(const std::string &) {}
	void begin_cpu_timing() {}
	double end_cpu_timing_ms() { return 0.0; }
	double get_gpu_time_ms(const std::string &) const { return 0.0; }
	double get_frame_time_ms() const { return 0.0; }
	double get_cpu_submission_time_ms() const { return 0.0; }
	const PerformanceStats &get_stats(const std::string &) const
	{
		static PerformanceStats s;
		return s;
	}
	const PerformanceStats &get_frame_stats() const
	{
		static PerformanceStats s;
		return s;
	}
	const PerformanceStats &get_cpu_stats() const
	{
		static PerformanceStats s;
		return s;
	}
	void print_summary() const {}
	bool has_results() const { return false; }
};

#endif // ENABLE_PERFORMANCE_MEASUREMENT

#endif // PERFORMANCE_TIMER_HPP_
