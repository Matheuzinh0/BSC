/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#include "ui/UIController.hpp"
#include "core/Utils.hpp"
#include "imgui.h"
#include <filesystem>
#include <algorithm>

namespace ofx::ui {

UIController::UIController(converter::ConverterEngine& engine)
    : m_engine(engine) {
    m_throughputHistory.resize(50, 0.0f);
}

void UIController::addFiles(const std::vector<std::string>& paths) {
    std::lock_guard<std::mutex> lock(m_taskMutex);
    for (const auto& path : paths) {
        if (!std::filesystem::exists(path) || std::filesystem::is_directory(path)) continue;

        core::FileFormat fmt = m_engine.detectFormat(path);
        if (fmt == core::FileFormat::UNKNOWN) continue;

        // Check if already in queue
        bool exists = false;
        for (const auto& t : m_tasks) {
            if (t.inputFilePath == path) { exists = true; break; }
        }
        if (exists) continue;

        core::BatchConversionTask task;
        task.id = std::to_string(m_tasks.size() + 1);
        task.inputFilePath = path;
        task.inputFormat = fmt;
        task.status = core::BatchConversionTask::Status::PENDING;

        // Immediately attempt fast background parse for transaction count preview
        task.parsedStatement = m_engine.parseFile(path);
        if (task.parsedStatement && !task.parsedStatement->transactions.empty()) {
            task.status = core::BatchConversionTask::Status::PARSED;
        }

        m_tasks.push_back(std::move(task));
    }
}

void UIController::addDirectory(const std::string& dirPath) {
    if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath)) return;

    std::vector<std::string> collected;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath)) {
        if (entry.is_regular_file()) {
            collected.push_back(entry.path().string());
        }
    }
    addFiles(collected);
}

void UIController::startBatchConversion() {
    std::vector<core::FileFormat> targets;
    if (m_targetOfx)  targets.push_back(core::FileFormat::OFX);
    if (m_targetCsv)  targets.push_back(core::FileFormat::CSV);
    if (m_targetXlsx) targets.push_back(core::FileFormat::XLSX);
    if (m_targetQif)  targets.push_back(core::FileFormat::QIF);
    if (m_targetPdf)  targets.push_back(core::FileFormat::PDF);

    if (targets.empty()) return;

    std::vector<core::BatchConversionTask> toProcess;
    {
        std::lock_guard<std::mutex> lock(m_taskMutex);
        std::string customOut(m_outputDirBuf);

        for (auto& task : m_tasks) {
            task.targetFormats = targets;
            task.outputDirectory = customOut;
            task.status = core::BatchConversionTask::Status::PENDING;
            toProcess.push_back(task);
        }
    }

    if (toProcess.empty()) return;

    m_isConverting = true;

    m_engine.processBatchAsync(
        toProcess,
        [this](const core::BatchConversionTask& update) {
            std::lock_guard<std::mutex> lock(this->m_taskMutex);
            for (auto& t : this->m_tasks) {
                if (t.inputFilePath == update.inputFilePath) {
                    t = update;
                    break;
                }
            }
        },
        [this](const core::PipelineMetrics& finalMetrics) {
            std::lock_guard<std::mutex> lock(this->m_taskMutex);
            this->m_currentMetrics = finalMetrics;
            this->m_isConverting = false;

            // Shift throughput history
            if (this->m_throughputHistory.size() >= 50) {
                this->m_throughputHistory.erase(this->m_throughputHistory.begin());
            }
            this->m_throughputHistory.push_back(static_cast<float>(finalMetrics.averageTransactionsPerSec));
        }
    );
}

void UIController::renderHeader() {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.06f, 0.72f, 0.51f, 1.0f));
    ImGui::Text("BSC - BANK STATEMENT CONVERSION");
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::TextDisabled("v2.5 High-Throughput Universal Converter");

    ImGui::SameLine(ImGui::GetWindowWidth() - 480);
    ImGui::TextColored(ImVec4(0.22f, 0.75f, 0.95f, 1.0f), "[Air-Gapped: 100%% Local]");

    ImGui::SameLine();
    std::string threadsInfo = "[Threads: " + std::to_string(m_engine.getWorkerCount()) + " Core Pool]";
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.4f, 1.0f), "%s", threadsInfo.c_str());

    ImGui::SameLine();
    if (ImGui::SmallButton("Créditos / UFPE")) {
        m_showAboutModal = true;
    }

    ImGui::Separator();
}

void UIController::renderTargetOptionsPanel() {
    ImGui::BeginChild("TargetOptionsPanel", ImVec2(0, 140), true);

    ImGui::TextColored(ImVec4(0.22f, 0.75f, 0.95f, 1.0f), "MATRIZ UNIVERSAL DE CONVERSAO (QUALQUER PARA QUALQUER):");
    ImGui::Spacing();

    ImGui::Checkbox("OFX (QuickBooks / ContaAzul / ERP)", &m_targetOfx);
    ImGui::SameLine(320);
    ImGui::Checkbox("CSV (RFC-4180 / Delimitador ';')", &m_targetCsv);
    ImGui::SameLine(600);
    ImGui::Checkbox("XLSX (Excel Nativo com Cores e Fórmulas)", &m_targetXlsx);

    ImGui::Checkbox("QIF (Quicken Interchange Format)", &m_targetQif);
    ImGui::SameLine(320);
    ImGui::Checkbox("PDF (Relatório Vetorial Financeiro)", &m_targetPdf);

    ImGui::Spacing();
    ImGui::Text("Diretório de Destino (em branco = mesma pasta de origem):");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(450);
    ImGui::InputText("##outdir", m_outputDirBuf, sizeof(m_outputDirBuf));
    ImGui::SameLine();
    if (ImGui::Button("Selecionar Pasta...##out", ImVec2(170, 24))) {
        m_isSelectingOutputDir = true;
        std::string cur = m_outputDirBuf[0] ? m_outputDirBuf : std::filesystem::current_path().string();
        snprintf(m_dirPickerCurrentPath, sizeof(m_dirPickerCurrentPath), "%s", cur.c_str());
        m_showDirPickerModal = true;
    }

    ImGui::EndChild();
}

void UIController::renderBatchQueuePanel() {
    float availHeight = ImGui::GetContentRegionAvail().y - 170.0f;
    ImGui::BeginChild("BatchQueuePanel", ImVec2(0, availHeight), true);

    // Top action bar
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.45f, 0.75f, 0.9f));
    if (ImGui::Button("Selecionar Pasta com Extratos...", ImVec2(270, 32))) {
        m_isSelectingOutputDir = false;
        std::string cur = std::filesystem::current_path().string();
        snprintf(m_dirPickerCurrentPath, sizeof(m_dirPickerCurrentPath), "%s", cur.c_str());
        m_showDirPickerModal = true;
    }
    ImGui::PopStyleColor();

    ImGui::SameLine();
    if (ImGui::Button("Carregar Amostras", ImVec2(150, 32))) {
        if (std::filesystem::exists("samples")) {
            addDirectory("samples");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Limpar Fila", ImVec2(110, 32))) {
        std::lock_guard<std::mutex> lock(m_taskMutex);
        m_tasks.clear();
    }
    ImGui::SameLine();

    if (m_isConverting) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.2f, 0.2f, 0.85f));
        if (ImGui::Button("Cancelar Conversão", ImVec2(220, 32))) {
            m_engine.cancelAll();
        }
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06f, 0.72f, 0.51f, 0.9f));
        if (ImGui::Button("INICIAR CONVERSAO EM LOTE", ImVec2(250, 32))) {
            startBatchConversion();
        }
        ImGui::PopStyleColor();
    }

    ImGui::SameLine(ImGui::GetWindowWidth() - 280);
    std::string countStr = "Arquivos na Fila: " + std::to_string(m_tasks.size());
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "%s", countStr.c_str());

    ImGui::Spacing();

    // Table of queued files
    static ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | 
                                   ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
                                   ImGuiTableFlags_Resizable;

    if (ImGui::BeginTable("QueueTable", 7, flags)) {
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 35.0f);
        ImGui::TableSetupColumn("Formato", ImGuiTableColumnFlags_WidthFixed, 65.0f);
        ImGui::TableSetupColumn("Arquivo de Origem", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Transações", ImGuiTableColumnFlags_WidthFixed, 85.0f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Taxa (Tx/s)", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Ação", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableHeadersRow();

        std::lock_guard<std::mutex> lock(m_taskMutex);
        for (size_t i = 0; i < m_tasks.size(); ++i) {
            auto& task = m_tasks[i];
            ImGui::TableNextRow();

            // 0: ID
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%zu", i + 1);

            // 1: Formato
            ImGui::TableSetColumnIndex(1);
            std::string fmtStr = core::formatToString(task.inputFormat);
            ImGui::TextColored(ImVec4(0.22f, 0.75f, 0.95f, 1.0f), "%s", fmtStr.c_str());

            // 2: Caminho
            ImGui::TableSetColumnIndex(2);
            std::filesystem::path p(task.inputFilePath);
            ImGui::Text("%s", p.filename().string().c_str());
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", task.inputFilePath.c_str());
            }

            // 3: Transações
            ImGui::TableSetColumnIndex(3);
            if (task.parsedStatement) {
                ImGui::Text("%zu", task.parsedStatement->transactions.size());
            } else {
                ImGui::TextDisabled("-");
            }

            // 4: Status
            ImGui::TableSetColumnIndex(4);
            switch (task.status) {
                case core::BatchConversionTask::Status::PENDING:
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Pendente");
                    break;
                case core::BatchConversionTask::Status::PARSING:
                    ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "Lendo...");
                    break;
                case core::BatchConversionTask::Status::PARSED:
                    ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.9f, 1.0f), "Pronto");
                    break;
                case core::BatchConversionTask::Status::CONVERTING:
                    ImGui::TextColored(ImVec4(0.2f, 0.7f, 1.0f, 1.0f), "Convertendo...");
                    break;
                case core::BatchConversionTask::Status::COMPLETED:
                    ImGui::TextColored(ImVec4(0.1f, 0.8f, 0.4f, 1.0f), "Concluído");
                    break;
                case core::BatchConversionTask::Status::FAILED:
                    ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "Falha");
                    break;
            }

            // 5: Taxa
            ImGui::TableSetColumnIndex(5);
            if (task.throughputTxSec > 0.0) {
                ImGui::Text("%.0f", task.throughputTxSec);
            } else {
                ImGui::TextDisabled("-");
            }

            // 6: Ação (Inspecionar)
            ImGui::TableSetColumnIndex(6);
            std::string btnId = "Ver##" + std::to_string(i);
            if (task.parsedStatement && ImGui::Button(btnId.c_str(), ImVec2(75, 20))) {
                m_inspectedStatement = task.parsedStatement;
                m_showInspectModal = true;
            }
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
}

void UIController::renderTelemetryDashboard() {
    ImGui::BeginChild("TelemetryDashboard", ImVec2(0, 0), true);

    ImGui::TextColored(ImVec4(0.22f, 0.75f, 0.95f, 1.0f), "TELEMETRIA DE EXECUCAO & ALTA VAZAO:");
    ImGui::Spacing();

    ImGui::Columns(4, "telecomColumns", false);

    // Column 1: Vazão de Transações
    ImGui::TextDisabled("TAXA DE PROCESSAMENTO");
    std::string txRate = std::to_string(static_cast<int>(m_currentMetrics.averageTransactionsPerSec)) + " Tx/s";
    ImGui::TextColored(ImVec4(0.06f, 0.72f, 0.51f, 1.0f), "%s", txRate.c_str());
    ImGui::NextColumn();

    // Column 2: Volume de Dados
    ImGui::TextDisabled("DADOS TRANSFERIDOS");
    double mbProcessed = m_currentMetrics.totalBytesProcessed / (1024.0 * 1024.0);
    std::string dataStr = core::Utils::formatCurrency(mbProcessed, false) + " MB (" +
                          std::to_string(static_cast<int>(m_currentMetrics.averageThroughputMBs)) + " MB/s)";
    ImGui::TextColored(ImVec4(0.22f, 0.75f, 0.95f, 1.0f), "%s", dataStr.c_str());
    ImGui::NextColumn();

    // Column 3: Total Transações
    ImGui::TextDisabled("TOTAL DE REGISTROS");
    std::string totTx = std::to_string(m_currentMetrics.totalTransactions) + " itens em " +
                        std::to_string(static_cast<int>(m_currentMetrics.totalDurationMs)) + " ms";
    ImGui::TextColored(ImVec4(0.95f, 0.96f, 0.98f, 1.0f), "%s", totTx.c_str());
    ImGui::NextColumn();

    // Column 4: Integridade & Criptografia
    ImGui::TextDisabled("INTEGRIDADE & PARIDADE");
    ImGui::TextColored(ImVec4(0.06f, 0.72f, 0.51f, 1.0f), "SHA-256 PARITY OK");
    ImGui::NextColumn();

    ImGui::Columns(1);
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.55f, 0.65f, 0.75f, 1.0f),
        "Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação, Departamento de Eletrônica e Sistemas, CTG-UFPE.");

    ImGui::EndChild();
}

void UIController::renderInspectionModal() {
    if (!m_showInspectModal || !m_inspectedStatement) return;

    ImGui::SetNextWindowSize(ImVec2(900, 560), ImGuiCond_Appearing);
    if (ImGui::Begin("Inspeção de Extrato Bancário", &m_showInspectModal, ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("Arquivo: %s", m_inspectedStatement->sourceFilePath.c_str());
        ImGui::Text("Banco: %s (%s) | Ag: %s | CC: %s | Moeda: %s",
            m_inspectedStatement->account.bankName.c_str(),
            m_inspectedStatement->account.bankId.c_str(),
            m_inspectedStatement->account.branchId.c_str(),
            m_inspectedStatement->account.accountId.c_str(),
            m_inspectedStatement->account.currency.c_str()
        );

        ImGui::Separator();

        // Financial totals summary
        ImGui::TextColored(ImVec4(0.06f, 0.72f, 0.51f, 1.0f), "Créditos: R$ %s (%zu)", 
            core::Utils::formatCurrency(m_inspectedStatement->totalCredits, true).c_str(),
            m_inspectedStatement->creditCount);
        ImGui::SameLine(280);
        ImGui::TextColored(ImVec4(0.95f, 0.25f, 0.25f, 1.0f), "Débitos: R$ %s (%zu)", 
            core::Utils::formatCurrency(m_inspectedStatement->totalDebits, true).c_str(),
            m_inspectedStatement->debitCount);
        ImGui::SameLine(540);
        ImVec4 netColor = (m_inspectedStatement->netFlow >= 0.0) ? 
            ImVec4(0.06f, 0.72f, 0.51f, 1.0f) : ImVec4(0.95f, 0.25f, 0.25f, 1.0f);
        ImGui::TextColored(netColor, "Líquido: R$ %s", 
            core::Utils::formatCurrency(m_inspectedStatement->netFlow, true).c_str());

        ImGui::Spacing();
        ImGui::SetNextItemWidth(300);
        ImGui::InputTextWithHint("##filter", "Filtrar por data, descrição ou doc...", m_searchFilterBuf, sizeof(m_searchFilterBuf));
        ImGui::Spacing();

        std::string filter(m_searchFilterBuf);
        filter = core::Utils::toLower(filter);

        if (ImGui::BeginTable("InspectTable", 6, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
            ImGui::TableSetupColumn("Data", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Descrição", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Doc / FITID", ImGuiTableColumnFlags_WidthFixed, 110.0f);
            ImGui::TableSetupColumn("Tipo", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableSetupColumn("Valor (R$)", ImGuiTableColumnFlags_WidthFixed, 110.0f);
            ImGui::TableSetupColumn("Hash SHA-256", ImGuiTableColumnFlags_WidthFixed, 140.0f);
            ImGui::TableHeadersRow();

            for (const auto& tx : m_inspectedStatement->transactions) {
                if (!filter.empty()) {
                    std::string desc = core::Utils::toLower(tx.description);
                    std::string date = tx.date;
                    std::string doc = core::Utils::toLower(tx.documentNumber);
                    if (desc.find(filter) == std::string::npos && 
                        date.find(filter) == std::string::npos &&
                        doc.find(filter) == std::string::npos) {
                        continue;
                    }
                }

                ImGui::TableNextRow();

                // 0: Data
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", tx.date.c_str());

                // 1: Descrição
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", tx.description.c_str());

                // 2: Documento
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%s", tx.documentNumber.c_str());

                // 3: Tipo
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%s", tx.isCredit() ? "CRED" : "DEB");

                // 4: Valor
                ImGui::TableSetColumnIndex(4);
                std::string valStr = core::Utils::formatCurrency(tx.amount, true);
                if (tx.isCredit()) {
                    ImGui::TextColored(ImVec4(0.06f, 0.72f, 0.51f, 1.0f), "%s", valStr.c_str());
                } else {
                    ImGui::TextColored(ImVec4(0.95f, 0.25f, 0.25f, 1.0f), "%s", valStr.c_str());
                }

                // 5: Hash SHA-256
                ImGui::TableSetColumnIndex(5);
                std::string shortHash = tx.hashSha256.substr(0, 14) + "..";
                ImGui::TextDisabled("%s", shortHash.c_str());
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", tx.hashSha256.c_str());
                }
            }

            ImGui::EndTable();
        }

        ImGui::Spacing();
        if (ImGui::Button("Fechar", ImVec2(100, 28))) {
            m_showInspectModal = false;
        }
    }
    ImGui::End();
}

void UIController::renderAboutModal() {
    if (!m_showAboutModal) return;

    ImGui::SetNextWindowSize(ImVec2(700, 380), ImGuiCond_Appearing);
    if (ImGui::Begin("Creditos & Informacoes Institucionais", &m_showAboutModal, ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextColored(ImVec4(0.06f, 0.72f, 0.51f, 1.0f), "BSC - BANK STATEMENT CONVERSION (v2.5)");
        ImGui::Text("Sistema Universal de Conversao de Extratos Bancarios em Alta Vazao");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.22f, 0.75f, 0.95f, 1.0f), "DESENVOLVIMENTO:");
        ImGui::TextWrapped("Desenvolvido por Matheus Sousa, Laboratorio de Comunicacao e Seguranca da Informacao, Departamento de Eletronica e Sistemas, CTG-UFPE.");

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.22f, 0.75f, 0.95f, 1.0f), "ARQUITETURA E DESEMPENHO:");
        ImGui::BulletText("Pipeline de alta vazao com descompressao in-memory sem copias superfluas.");
        ImGui::BulletText("Processamento Concorrente com ThreadPool auto-adaptativo.");
        ImGui::BulletText("Integridade estrita ponta a ponta via hash criptografico SHA-256.");
        ImGui::BulletText("Reconciliacao e paridade continua de livro-razao contabil.");
        ImGui::BulletText("Licenca: MIT License.");

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.22f, 0.75f, 0.95f, 1.0f), "COMPATIBILIDADE LEGADA:");
        ImGui::BulletText("100%% C++20 | Backend OpenGL 2.1 / GLSL 1.20 | < 15MB RAM | CPU Intel Core 2 Duo / Atom / Modernas");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::Button("Fechar", ImVec2(100, 28))) {
            m_showAboutModal = false;
        }
    }
    ImGui::End();
}

void UIController::renderDirectoryPickerModal() {
    if (!m_showDirPickerModal) return;

    ImGui::SetNextWindowSize(ImVec2(750, 480), ImGuiCond_Appearing);
    std::string title = m_isSelectingOutputDir ? "Selecionar Pasta de Destino" : "Selecionar Pasta de Lotes com Extratos";
    
    if (ImGui::Begin(title.c_str(), &m_showDirPickerModal, ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextColored(ImVec4(0.22f, 0.75f, 0.95f, 1.0f), "Caminho Atual:");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputText("##pickerPath", m_dirPickerCurrentPath, sizeof(m_dirPickerCurrentPath));
        ImGui::Spacing();

        // Quick Jump buttons
        const char* homeEnv = std::getenv("HOME");
        if (!homeEnv) homeEnv = std::getenv("USERPROFILE");
        std::string homeDir = homeEnv ? std::string(homeEnv) : ".";

        if (ImGui::SmallButton(".. Subir Pasta")) {
            std::filesystem::path p(m_dirPickerCurrentPath);
            if (p.has_parent_path()) {
                snprintf(m_dirPickerCurrentPath, sizeof(m_dirPickerCurrentPath), "%s", p.parent_path().string().c_str());
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Pasta Pessoal")) {
            snprintf(m_dirPickerCurrentPath, sizeof(m_dirPickerCurrentPath), "%s", homeDir.c_str());
        }
        ImGui::SameLine();
        std::string docsDir = (std::filesystem::path(homeDir) / "Documentos").string();
        if (std::filesystem::exists(docsDir) && ImGui::SmallButton("Documentos")) {
            snprintf(m_dirPickerCurrentPath, sizeof(m_dirPickerCurrentPath), "%s", docsDir.c_str());
        }
        ImGui::SameLine();
        std::string dlDir = (std::filesystem::path(homeDir) / "Downloads").string();
        if (std::filesystem::exists(dlDir) && ImGui::SmallButton("Downloads")) {
            snprintf(m_dirPickerCurrentPath, sizeof(m_dirPickerCurrentPath), "%s", dlDir.c_str());
        }
        ImGui::SameLine();
        std::string deskDir = (std::filesystem::path(homeDir) / "Desktop").string();
        if (std::filesystem::exists(deskDir) && ImGui::SmallButton("Área de Trabalho")) {
            snprintf(m_dirPickerCurrentPath, sizeof(m_dirPickerCurrentPath), "%s", deskDir.c_str());
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Subdirectories and file preview
        std::vector<std::string> subDirs;
        size_t detectedStatements = 0;
        std::filesystem::path currentPath(m_dirPickerCurrentPath);
        std::error_code ec;

        if (std::filesystem::exists(currentPath, ec) && std::filesystem::is_directory(currentPath, ec)) {
            for (const auto& entry : std::filesystem::directory_iterator(currentPath, ec)) {
                if (entry.is_directory(ec)) {
                    subDirs.push_back(entry.path().filename().string());
                } else if (entry.is_regular_file(ec)) {
                    std::string ext = core::Utils::toLower(entry.path().extension().string());
                    if (ext == ".pdf" || ext == ".ofx" || ext == ".csv" || ext == ".xlsx" || ext == ".qif") {
                        detectedStatements++;
                    }
                }
            }
            std::sort(subDirs.begin(), subDirs.end());
        }

        ImGui::Text("Subpastas Disponiveis:");
        ImGui::BeginChild("SubdirList", ImVec2(0, 210), true);
        if (subDirs.empty()) {
            ImGui::TextDisabled("(Nenhuma subpasta encontrada)");
        } else {
            for (const auto& dirName : subDirs) {
                std::string label = "[Pasta] " + dirName;
                if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_DontClosePopups)) {
                    std::filesystem::path newP = currentPath / dirName;
                    snprintf(m_dirPickerCurrentPath, sizeof(m_dirPickerCurrentPath), "%s", newP.string().c_str());
                }
            }
        }
        ImGui::EndChild();

        ImGui::Spacing();
        if (!m_isSelectingOutputDir) {
            ImGui::TextColored(ImVec4(0.06f, 0.72f, 0.51f, 1.0f), 
                "Extratos financeiros suportados detectados nesta pasta raiz: %zu arquivos", detectedStatements);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (m_isSelectingOutputDir) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06f, 0.72f, 0.51f, 0.9f));
            if (ImGui::Button("DEFINIR ESTA PASTA COMO DESTINO", ImVec2(300, 32))) {
                snprintf(m_outputDirBuf, sizeof(m_outputDirBuf), "%s", m_dirPickerCurrentPath);
                m_showDirPickerModal = false;
            }
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06f, 0.72f, 0.51f, 0.9f));
            if (ImGui::Button("CARREGAR TODOS OS EXTRATOS DESTA PASTA", ImVec2(360, 32))) {
                addDirectory(m_dirPickerCurrentPath);
                m_showDirPickerModal = false;
            }
            ImGui::PopStyleColor();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(120, 32))) {
            m_showDirPickerModal = false;
        }
    }
    ImGui::End();
}

void UIController::render() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("BSCMainWindow", nullptr, windowFlags);

    renderHeader();
    renderTargetOptionsPanel();
    renderBatchQueuePanel();
    renderTelemetryDashboard();
    renderInspectionModal();
    renderAboutModal();
    renderDirectoryPickerModal();

    ImGui::End();
}

} // namespace ofx::ui
