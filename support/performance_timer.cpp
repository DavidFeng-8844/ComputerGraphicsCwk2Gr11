#include "performance_timer.hpp"

#ifdef ENABLE_PERFORMANCE_MEASUREMENT

#include <print>
#include <algorithm>
#include <numeric>

// ============================================================================
// GPUTimer Implementation
// ============================================================================

GPUTimer::GPUTimer() = default;

GPUTimer::~GPUTimer()
{
	cleanup();
}

void GPUTimer::initialize()
{
	if (initialized_) return;
	initialized_ = true;
	currentFrame_ = 0;
	resultFrame_ = 0;
}

void GPUTimer::cleanup()
{
	if (!initialized_) return;
	
	for (auto& section : sections_)
	{
		for (int i = 0; i < QUERY_BUFFER_SIZE; ++i)
		{
			if (section.startQueries[i] != 0)
				glDeleteQueries(1, &section.startQueries[i]);
			if (section.endQueries[i] != 0)
				glDeleteQueries(1, &section.endQueries[i]);
		}
	}
	sections_.clear();
	initialized_ = false;
}

GPUTimer::TimerSection* GPUTimer::find_section(const std::string& name)
{
	for (auto& section : sections_)
	{
		if (section.name == name)
			return &section;
	}
	return nullptr;
}

const GPUTimer::TimerSection* GPUTimer::find_section(const std::string& name) const
{
	for (const auto& section : sections_)
	{
		if (section.name == name)
			return &section;
	}
	return nullptr;
}

GPUTimer::TimerSection& GPUTimer::get_or_create_section(const std::string& name)
{
	TimerSection* existing = find_section(name);
	if (existing) return *existing;
	
	TimerSection section;
	section.name = name;
	
	// Create query objects for all buffer slots
	for (int i = 0; i < QUERY_BUFFER_SIZE; ++i)
	{
		glGenQueries(1, &section.startQueries[i]);
		glGenQueries(1, &section.endQueries[i]);
		section.queryActive[i] = false;
	}
	
	sections_.push_back(std::move(section));
	return sections_.back();
}

void GPUTimer::start(const std::string& name)
{
	if (!initialized_) return;
	
	TimerSection& section = get_or_create_section(name);
	glQueryCounter(section.startQueries[currentFrame_], GL_TIMESTAMP);
}

void GPUTimer::end(const std::string& name)
{
	if (!initialized_) return;
	
	TimerSection* section = find_section(name);
	if (!section) return;
	
	glQueryCounter(section->endQueries[currentFrame_], GL_TIMESTAMP);
	section->queryActive[currentFrame_] = true;
}

void GPUTimer::end_frame()
{
	if (!initialized_) return;
	
	// Try to retrieve results from the oldest frame (non-blocking)
	int oldestFrame = (currentFrame_ + 1) % QUERY_BUFFER_SIZE;
	
	for (auto& section : sections_)
	{
		if (!section.queryActive[oldestFrame])
			continue;
		
		// Check if results are available (non-blocking)
		GLint startAvailable = 0, endAvailable = 0;
		glGetQueryObjectiv(section.startQueries[oldestFrame], GL_QUERY_RESULT_AVAILABLE, &startAvailable);
		glGetQueryObjectiv(section.endQueries[oldestFrame], GL_QUERY_RESULT_AVAILABLE, &endAvailable);
		
		if (startAvailable && endAvailable)
		{
			GLuint64 startTime, endTime;
			glGetQueryObjectui64v(section.startQueries[oldestFrame], GL_QUERY_RESULT, &startTime);
			glGetQueryObjectui64v(section.endQueries[oldestFrame], GL_QUERY_RESULT, &endTime);
			
			// Convert nanoseconds to milliseconds
			section.lastTimeMs = static_cast<double>(endTime - startTime) / 1000000.0;
			section.queryActive[oldestFrame] = false;
		}
	}
	
	// Advance to next frame
	resultFrame_ = oldestFrame;
	currentFrame_ = (currentFrame_ + 1) % QUERY_BUFFER_SIZE;
}

double GPUTimer::get_time_ms(const std::string& name) const
{
	const TimerSection* section = find_section(name);
	if (!section) return -1.0;
	return section->lastTimeMs;
}

bool GPUTimer::has_results() const
{
	for (const auto& section : sections_)
	{
		if (section.lastTimeMs >= 0.0)
			return true;
	}
	return false;
}

// ============================================================================
// CPUTimer Implementation
// ============================================================================

void CPUTimer::start()
{
	startTime_ = std::chrono::high_resolution_clock::now();
}

double CPUTimer::stop_ms()
{
	auto endTime = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime_);
	return static_cast<double>(duration.count()) / 1000000.0;
}

double CPUTimer::elapsed_ms() const
{
	auto now = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(now - startTime_);
	return static_cast<double>(duration.count()) / 1000000.0;
}

// ============================================================================
// PerformanceStats Implementation
// ============================================================================

PerformanceStats::PerformanceStats(int sampleCount)
	: maxSamples_(sampleCount)
{
	samples_.resize(maxSamples_, 0.0);
}

void PerformanceStats::add_sample(double value)
{
	// Update running statistics
	if (sampleCount_ >= maxSamples_)
	{
		// Remove oldest sample from sum
		sum_ -= samples_[currentIndex_];
	}
	
	samples_[currentIndex_] = value;
	sum_ += value;
	
	if (value < min_) min_ = value;
	if (value > max_) max_ = value;
	
	currentIndex_ = (currentIndex_ + 1) % maxSamples_;
	if (sampleCount_ < maxSamples_)
		++sampleCount_;
}

double PerformanceStats::get_average() const
{
	if (sampleCount_ == 0) return 0.0;
	return sum_ / sampleCount_;
}

double PerformanceStats::get_min() const
{
	return (sampleCount_ > 0) ? min_ : 0.0;
}

double PerformanceStats::get_max() const
{
	return (sampleCount_ > 0) ? max_ : 0.0;
}

double PerformanceStats::get_last() const
{
	if (sampleCount_ == 0) return 0.0;
	int lastIndex = (currentIndex_ - 1 + maxSamples_) % maxSamples_;
	return samples_[lastIndex];
}

int PerformanceStats::get_sample_count() const
{
	return sampleCount_;
}

void PerformanceStats::reset()
{
	std::fill(samples_.begin(), samples_.end(), 0.0);
	currentIndex_ = 0;
	sampleCount_ = 0;
	sum_ = 0.0;
	min_ = 1e9;
	max_ = 0.0;
}

// ============================================================================
// PerformanceMeasurement Implementation
// ============================================================================

PerformanceMeasurement::PerformanceMeasurement()
	: frameStats_(100)
	, cpuStats_(100)
{
}

PerformanceMeasurement::~PerformanceMeasurement()
{
	cleanup();
}

void PerformanceMeasurement::initialize()
{
	if (initialized_) return;
	
	gpuTimer_.initialize();
	initialized_ = true;
	frameCount_ = 0;
	
	std::print("Performance measurement initialized\n");
}

void PerformanceMeasurement::cleanup()
{
	if (!initialized_) return;
	
	gpuTimer_.cleanup();
	sectionStats_.clear();
	initialized_ = false;
}

void PerformanceMeasurement::begin_frame()
{
	if (!initialized_) return;
	
	frameTimer_.start();
	gpuTimer_.start("TotalFrame");
}

void PerformanceMeasurement::end_frame()
{
	if (!initialized_) return;
	
	gpuTimer_.end("TotalFrame");
	gpuTimer_.end_frame();
	
	// Record frame time
	lastFrameTime_ = frameTimer_.stop_ms();
	frameStats_.add_sample(lastFrameTime_);
	
	// Update section statistics from GPU timer
	for (auto& [name, stats] : sectionStats_)
	{
		double time = gpuTimer_.get_time_ms(name);
		if (time >= 0.0)
		{
			stats.add_sample(time);
		}
	}
	
	// Also update TotalFrame stats
	double totalGpuTime = gpuTimer_.get_time_ms("TotalFrame");
	if (totalGpuTime >= 0.0)
	{
		get_or_create_stats("TotalFrame").add_sample(totalGpuTime);
	}
	
	++frameCount_;
}

void PerformanceMeasurement::begin_gpu_section(const std::string& name)
{
	if (!initialized_) return;
	
	gpuTimer_.start(name);
	get_or_create_stats(name);  // Ensure stats exist
}

void PerformanceMeasurement::end_gpu_section(const std::string& name)
{
	if (!initialized_) return;
	
	gpuTimer_.end(name);
}

void PerformanceMeasurement::begin_cpu_timing()
{
	if (!initialized_) return;
	
	cpuTimer_.start();
}

double PerformanceMeasurement::end_cpu_timing_ms()
{
	if (!initialized_) return 0.0;
	
	lastCpuTime_ = cpuTimer_.stop_ms();
	cpuStats_.add_sample(lastCpuTime_);
	return lastCpuTime_;
}

double PerformanceMeasurement::get_gpu_time_ms(const std::string& name) const
{
	return gpuTimer_.get_time_ms(name);
}

double PerformanceMeasurement::get_frame_time_ms() const
{
	return lastFrameTime_;
}

double PerformanceMeasurement::get_cpu_submission_time_ms() const
{
	return lastCpuTime_;
}

PerformanceStats& PerformanceMeasurement::get_or_create_stats(const std::string& name)
{
	for (auto& [n, stats] : sectionStats_)
	{
		if (n == name)
			return stats;
	}
	sectionStats_.emplace_back(name, PerformanceStats(100));
	return sectionStats_.back().second;
}

const PerformanceStats* PerformanceMeasurement::find_stats(const std::string& name) const
{
	for (const auto& [n, stats] : sectionStats_)
	{
		if (n == name)
			return &stats;
	}
	return nullptr;
}

const PerformanceStats& PerformanceMeasurement::get_stats(const std::string& name) const
{
	const PerformanceStats* stats = find_stats(name);
	if (stats) return *stats;
	static PerformanceStats empty;
	return empty;
}

const PerformanceStats& PerformanceMeasurement::get_frame_stats() const
{
	return frameStats_;
}

const PerformanceStats& PerformanceMeasurement::get_cpu_stats() const
{
	return cpuStats_;
}

bool PerformanceMeasurement::has_results() const
{
	return gpuTimer_.has_results();
}

void PerformanceMeasurement::print_summary() const
{
	if (!initialized_) return;
	
	std::print("\n");
	std::print("================================================================================\n");
	std::print("                    PERFORMANCE MEASUREMENT RESULTS\n");
	std::print("                         ({} frames sampled)\n", frameCount_);
	std::print("================================================================================\n");
	
	// Table header
	std::print("\n");
	std::print("+----------------------+------------+------------+------------+------------+\n");
	std::print("| Metric               | Average    | Min        | Max        | Std Dev    |\n");
	std::print("|                      | (ms)       | (ms)       | (ms)       | (ms)       |\n");
	std::print("+----------------------+------------+------------+------------+------------+\n");
	
	// Frame time (CPU measured)
	double frameAvg = frameStats_.get_average();
	double fps = (frameAvg > 0) ? 1000.0 / frameAvg : 0.0;
	std::print("| Frame Time (CPU)     | {:>10.3f} | {:>10.3f} | {:>10.3f} | {:>10.3f} |\n",
		frameAvg, frameStats_.get_min(), frameStats_.get_max(),
		frameStats_.get_max() - frameStats_.get_min());
	
	// CPU command submission time
	std::print("| CPU Submission       | {:>10.3f} | {:>10.3f} | {:>10.3f} | {:>10.3f} |\n",
		cpuStats_.get_average(), cpuStats_.get_min(), cpuStats_.get_max(),
		cpuStats_.get_max() - cpuStats_.get_min());
	
	std::print("+----------------------+------------+------------+------------+------------+\n");
	
	// GPU section times
	std::print("| GPU Total Frame      |");
	const PerformanceStats* totalStats = find_stats("TotalFrame");
	if (totalStats && totalStats->get_sample_count() > 0)
	{
		std::print(" {:>10.3f} | {:>10.3f} | {:>10.3f} | {:>10.3f} |\n",
			totalStats->get_average(), totalStats->get_min(), totalStats->get_max(),
			totalStats->get_max() - totalStats->get_min());
	}
	else
	{
		std::print(" {:>10} | {:>10} | {:>10} | {:>10} |\n", "N/A", "N/A", "N/A", "N/A");
	}
	
	// Individual sections
	const char* sectionNames[] = {"Terrain", "Launchpad", "Vehicle"};
	const char* sectionLabels[] = {"GPU Terrain (1.2)", "GPU Launchpad (1.4)", "GPU Vehicle (1.5)"};
	
	for (int i = 0; i < 3; ++i)
	{
		const PerformanceStats* stats = find_stats(sectionNames[i]);
		std::print("| {:<20} |", sectionLabels[i]);
		if (stats && stats->get_sample_count() > 0)
		{
			std::print(" {:>10.3f} | {:>10.3f} | {:>10.3f} | {:>10.3f} |\n",
				stats->get_average(), stats->get_min(), stats->get_max(),
				stats->get_max() - stats->get_min());
		}
		else
		{
			std::print(" {:>10} | {:>10} | {:>10} | {:>10} |\n", "N/A", "N/A", "N/A", "N/A");
		}
	}
	
	std::print("+----------------------+------------+------------+------------+------------+\n");
	
	// Summary statistics
	std::print("\n");
	std::print("Summary:\n");
	std::print("  - Average FPS: {:.1f}\n", fps);
	std::print("  - GPU utilization: {:.1f}%% of frame time\n", 
		(totalStats && frameAvg > 0) ? (totalStats->get_average() / frameAvg * 100.0) : 0.0);
	
	// Breakdown
	const PerformanceStats* terrainStats = find_stats("Terrain");
	const PerformanceStats* launchpadStats = find_stats("Launchpad");
	const PerformanceStats* vehicleStats = find_stats("Vehicle");
	
	if (totalStats && totalStats->get_average() > 0)
	{
		double total = totalStats->get_average();
		std::print("\n");
		std::print("GPU Time Breakdown:\n");
		if (terrainStats)
			std::print("  - Terrain:   {:.1f}%%\n", terrainStats->get_average() / total * 100.0);
		if (launchpadStats)
			std::print("  - Launchpad: {:.1f}%%\n", launchpadStats->get_average() / total * 100.0);
		if (vehicleStats)
			std::print("  - Vehicle:   {:.1f}%%\n", vehicleStats->get_average() / total * 100.0);
	}
	
	std::print("\n================================================================================\n");
}

#endif // ENABLE_PERFORMANCE_MEASUREMENT
