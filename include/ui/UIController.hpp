/**
 * =================================================================================================
 * Projeto: OmniStatement Studio
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#pragma once

#include "core/Types.hpp"
#include "converter/ConverterEngine.hpp"
#include <vector>
#include <string>
#include <memory>
#include <mutex>

namespace ofx::ui {

class UIController {
public:
    explicit UIController(converter::ConverterEngine& engine);

    void render();
    void addFiles(const std::vector<std::string>& paths);
    void addDirectory(const std::string& dirPath);

private:
    void renderHeader();
    void renderBatchQueuePanel();
    void renderTargetOptionsPanel();
    void renderTelemetryDashboard();
    void renderInspectionModal();
    void renderAboutModal();
    void startBatchConversion();

    converter::ConverterEngine& m_engine;

    std::mutex m_taskMutex;
    std::vector<core::BatchConversionTask> m_tasks;
    core::PipelineMetrics m_currentMetrics;

    // Target format checkboxes
    bool m_targetOfx = true;
    bool m_targetCsv = true;
    bool m_targetXlsx = true;
    bool m_targetQif = false;
    bool m_targetPdf = false;

    // Output directory
    char m_outputDirBuf[512] = "";
    char m_searchFilterBuf[128] = "";

    // Selected statement for detailed preview
    std::shared_ptr<core::Statement> m_inspectedStatement;
    bool m_showInspectModal = false;
    bool m_showAboutModal = false;

    // Historical throughput for plot
    std::vector<float> m_throughputHistory;
    bool m_isConverting = false;
};

} // namespace ofx::ui
