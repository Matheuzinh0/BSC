/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#pragma once

#include "core/Types.hpp"
#include "core/Pipeline.hpp"
#include "parsers/IParser.hpp"
#include "encoders/IEncoder.hpp"
#include <memory>
#include <vector>
#include <map>
#include <functional>
#include <mutex>

namespace ofx::converter {

class ConverterEngine {
public:
    ConverterEngine(size_t workerThreads = 0);
    ~ConverterEngine() = default;

    // Format detection
    core::FileFormat detectFormat(const std::string& filePath) const;

    // Single file parse
    std::shared_ptr<core::Statement> parseFile(const std::string& filePath) const;

    // Single format conversion
    bool convertStatement(const core::Statement& statement, core::FileFormat targetFormat, const std::string& outputPath) const;

    // Batch asynchronous processing
    using TaskProgressCallback = std::function<void(const core::BatchConversionTask&)>;
    using BatchCompleteCallback = std::function<void(const core::PipelineMetrics&)>;

    void processBatchAsync(
        std::vector<core::BatchConversionTask> tasks,
        TaskProgressCallback onTaskUpdate,
        BatchCompleteCallback onBatchComplete
    );

    // Cancel active tasks if needed
    void cancelAll();

    // Telemetry & metrics
    core::PipelineMetrics getCumulativeMetrics() const;
    size_t getWorkerCount() const { return m_threadPool.getThreadCount(); }

private:
    void registerComponents();

    std::vector<std::unique_ptr<parsers::IParser>> m_parsers;
    std::map<core::FileFormat, std::unique_ptr<encoders::IEncoder>> m_encoders;

    core::ThreadPool m_threadPool;
    mutable std::mutex m_metricsMutex;
    core::PipelineMetrics m_metrics;
    std::atomic<bool> m_cancelRequested{false};
};

} // namespace ofx::converter
