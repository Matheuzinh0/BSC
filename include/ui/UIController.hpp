/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
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
    void renderDirectoryPickerModal();
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
    bool m_targetJson = false;

    // Output directory
    char m_outputDirBuf[512] = "";
    char m_searchFilterBuf[128] = "";
    int m_filterTypeIndex = 0; // 0=All, 1=Credits, 2=Debits, 3=Pix

    // Selected statement for detailed preview
    std::shared_ptr<core::Statement> m_inspectedStatement;
    bool m_showInspectModal = false;
    bool m_showAboutModal = false;

    // Directory selection modal
    bool m_showDirPickerModal = false;
    bool m_isSelectingOutputDir = false;
    char m_dirPickerCurrentPath[512] = "";

    // Historical throughput for plot
    std::vector<float> m_throughputHistory;
    bool m_isConverting = false;
};

} // namespace ofx::ui
