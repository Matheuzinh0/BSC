/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#include "converter/ConverterEngine.hpp"
#include "core/Utils.hpp"
#include "core/Hasher.hpp"
#include "parsers/PdfParser.hpp"
#include "parsers/OfxParser.hpp"
#include "parsers/CsvParser.hpp"
#include "parsers/XlsxParser.hpp"
#include "parsers/QifParser.hpp"
#include "encoders/OfxEncoder.hpp"
#include "encoders/CsvEncoder.hpp"
#include "encoders/XlsxEncoder.hpp"
#include "encoders/QifEncoder.hpp"
#include "encoders/PdfEncoder.hpp"
#include <filesystem>
#include <chrono>

namespace ofx::converter {

ConverterEngine::ConverterEngine(size_t workerThreads)
    : m_threadPool(workerThreads) {
    registerComponents();
}

void ConverterEngine::registerComponents() {
    // Register Parsers
    m_parsers.push_back(std::make_unique<parsers::PdfParser>());
    m_parsers.push_back(std::make_unique<parsers::OfxParser>());
    m_parsers.push_back(std::make_unique<parsers::CsvParser>());
    m_parsers.push_back(std::make_unique<parsers::XlsxParser>());
    m_parsers.push_back(std::make_unique<parsers::QifParser>());

    // Register Encoders
    m_encoders[core::FileFormat::OFX] = std::make_unique<encoders::OfxEncoder>();
    m_encoders[core::FileFormat::CSV] = std::make_unique<encoders::CsvEncoder>(';');
    m_encoders[core::FileFormat::XLSX] = std::make_unique<encoders::XlsxEncoder>();
    m_encoders[core::FileFormat::QIF] = std::make_unique<encoders::QifEncoder>();
    m_encoders[core::FileFormat::PDF] = std::make_unique<encoders::PdfEncoder>();
}

core::FileFormat ConverterEngine::detectFormat(const std::string& filePath) const {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return core::FileFormat::UNKNOWN;

    uint8_t header[512] = {0};
    file.read(reinterpret_cast<char*>(header), sizeof(header));
    size_t bytesRead = static_cast<size_t>(file.gcount());

    for (const auto& parser : m_parsers) {
        if (parser->canParse(filePath, header, bytesRead)) {
            return parser->getFormat();
        }
    }

    // Fallback to extension check
    std::filesystem::path p(filePath);
    std::string ext = core::Utils::toLower(p.extension().string());
    if (ext == ".pdf") return core::FileFormat::PDF;
    if (ext == ".ofx") return core::FileFormat::OFX;
    if (ext == ".csv" || ext == ".txt") return core::FileFormat::CSV;
    if (ext == ".xlsx" || ext == ".xls") return core::FileFormat::XLSX;
    if (ext == ".qif") return core::FileFormat::QIF;

    return core::FileFormat::UNKNOWN;
}

std::shared_ptr<core::Statement> ConverterEngine::parseFile(const std::string& filePath) const {
    core::FileFormat fmt = detectFormat(filePath);
    for (const auto& parser : m_parsers) {
        if (parser->getFormat() == fmt) {
            return parser->parse(filePath);
        }
    }
    return nullptr;
}

bool ConverterEngine::convertStatement(const core::Statement& statement, core::FileFormat targetFormat, const std::string& outputPath) const {
    auto it = m_encoders.find(targetFormat);
    if (it != m_encoders.end()) {
        return it->second->encode(statement, outputPath);
    }
    return false;
}

void ConverterEngine::processBatchAsync(
    std::vector<core::BatchConversionTask> tasks,
    TaskProgressCallback onTaskUpdate,
    BatchCompleteCallback onBatchComplete
) {
    m_cancelRequested = false;

    // Run coordinator task on a dedicated worker
    m_threadPool.enqueue([this, tasks = std::move(tasks), onTaskUpdate, onBatchComplete]() mutable {
        auto batchStart = std::chrono::high_resolution_clock::now();
        size_t totalTasks = tasks.size();
        std::atomic<size_t> completedCount{0};

        std::vector<std::future<void>> futures;
        futures.reserve(totalTasks);

        for (auto& task : tasks) {
            if (m_cancelRequested) break;

            futures.push_back(m_threadPool.enqueue([this, taskPtr = &task, onTaskUpdate, &completedCount]() {
                if (m_cancelRequested) {
                    taskPtr->status = core::BatchConversionTask::Status::FAILED;
                    taskPtr->errorMessage = "Cancelled";
                    if (onTaskUpdate) onTaskUpdate(*taskPtr);
                    completedCount++;
                    return;
                }

                auto taskStart = std::chrono::high_resolution_clock::now();

                // 1. Calculate File Checksum & Size
                std::error_code ec;
                taskPtr->inputSizeBytes = std::filesystem::file_size(taskPtr->inputFilePath, ec);
                taskPtr->sha256Input = core::Sha256::hashFile(taskPtr->inputFilePath);

                // 2. Parse Input File
                taskPtr->status = core::BatchConversionTask::Status::PARSING;
                if (onTaskUpdate) onTaskUpdate(*taskPtr);

                auto parseStart = std::chrono::high_resolution_clock::now();
                taskPtr->parsedStatement = this->parseFile(taskPtr->inputFilePath);
                auto parseEnd = std::chrono::high_resolution_clock::now();
                taskPtr->parseTimeMs = std::chrono::duration<double, std::milli>(parseEnd - parseStart).count();

                if (!taskPtr->parsedStatement || taskPtr->parsedStatement->transactions.empty()) {
                    taskPtr->status = core::BatchConversionTask::Status::FAILED;
                    taskPtr->errorMessage = "Nenhuma transação identificada no arquivo.";
                    if (onTaskUpdate) onTaskUpdate(*taskPtr);
                    completedCount++;
                    return;
                }

                taskPtr->status = core::BatchConversionTask::Status::CONVERTING;
                if (onTaskUpdate) onTaskUpdate(*taskPtr);

                // 3. Encode to all requested formats
                auto encodeStart = std::chrono::high_resolution_clock::now();
                bool allSucceeded = true;
                std::filesystem::path inPath(taskPtr->inputFilePath);
                std::string stem = inPath.stem().string();

                std::filesystem::path outDir = taskPtr->outputDirectory.empty() ? 
                    inPath.parent_path() : std::filesystem::path(taskPtr->outputDirectory);

                for (core::FileFormat targetFmt : taskPtr->targetFormats) {
                    std::string fmtFolder = core::formatToString(targetFmt);
                    std::filesystem::path formatDir = outDir / fmtFolder;
                    std::error_code dirEc;
                    std::filesystem::create_directories(formatDir, dirEc);

                    std::string outExt = core::formatToExtension(targetFmt);
                    std::filesystem::path targetFile = formatDir / (stem + outExt);

                    bool ok = this->convertStatement(*(taskPtr->parsedStatement), targetFmt, targetFile.string());
                    if (ok) {
                        taskPtr->generatedFiles.push_back(targetFile.string());
                        std::error_code oec;
                        taskPtr->outputSizeBytes += std::filesystem::file_size(targetFile, oec);
                    } else {
                        allSucceeded = false;
                        taskPtr->errorMessage += "Falha ao exportar " + fmtFolder + "; ";
                    }
                }

                auto taskEnd = std::chrono::high_resolution_clock::now();
                taskPtr->encodeTimeMs = std::chrono::duration<double, std::milli>(taskEnd - encodeStart).count();
                taskPtr->totalTimeMs = std::chrono::duration<double, std::milli>(taskEnd - taskStart).count();

                // Telecom Throughput Telemetry
                double seconds = taskPtr->totalTimeMs / 1000.0;
                if (seconds > 0.0) {
                    taskPtr->throughputMBs = (taskPtr->inputSizeBytes / (1024.0 * 1024.0)) / seconds;
                    taskPtr->throughputTxSec = taskPtr->parsedStatement->transactions.size() / seconds;
                }

                taskPtr->status = allSucceeded ? core::BatchConversionTask::Status::COMPLETED : core::BatchConversionTask::Status::FAILED;
                if (onTaskUpdate) onTaskUpdate(*taskPtr);

                // Update cumulative metrics
                {
                    std::lock_guard<std::mutex> lock(m_metricsMutex);
                    m_metrics.totalFiles++;
                    if (allSucceeded) m_metrics.successfulFiles++;
                    else m_metrics.failedFiles++;
                    m_metrics.totalTransactions += taskPtr->parsedStatement->transactions.size();
                    m_metrics.totalBytesProcessed += taskPtr->inputSizeBytes;
                }

                completedCount++;
            }));
        }

        // Wait for all task futures
        for (auto& f : futures) {
            f.wait();
        }

        auto batchEnd = std::chrono::high_resolution_clock::now();
        double batchDurationMs = std::chrono::duration<double, std::milli>(batchEnd - batchStart).count();

        {
            std::lock_guard<std::mutex> lock(m_metricsMutex);
            m_metrics.totalDurationMs = batchDurationMs;
            double sec = batchDurationMs / 1000.0;
            if (sec > 0.0) {
                m_metrics.averageThroughputMBs = (m_metrics.totalBytesProcessed / (1024.0 * 1024.0)) / sec;
                m_metrics.averageTransactionsPerSec = m_metrics.totalTransactions / sec;
            }
        }

        if (onBatchComplete) {
            onBatchComplete(this->getCumulativeMetrics());
        }
    });
}

void ConverterEngine::cancelAll() {
    m_cancelRequested = true;
}

core::PipelineMetrics ConverterEngine::getCumulativeMetrics() const {
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    return m_metrics;
}

} // namespace ofx::converter
