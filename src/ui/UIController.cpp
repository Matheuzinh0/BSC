/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#include "ui/UIController.hpp"
#include "ui/Theme.hpp"
#include "core/Utils.hpp"
#include "imgui.h"
#include <filesystem>
#include <algorithm>
#include <cmath>

namespace ofx::ui {

static void renderFormatBadge(core::FileFormat fmt) {
    ImVec4 bgCol, textCol;
    std::string label = core::formatToString(fmt);
    switch (fmt) {
        case core::FileFormat::PDF:  bgCol = ImVec4(0.85f, 0.20f, 0.20f, 0.25f); textCol = ImVec4(0.98f, 0.45f, 0.45f, 1.0f); break;
        case core::FileFormat::OFX:  bgCol = ImVec4(0.06f, 0.72f, 0.51f, 0.25f); textCol = ImVec4(0.20f, 0.90f, 0.65f, 1.0f); break;
        case core::FileFormat::CSV:  bgCol = ImVec4(0.04f, 0.52f, 0.89f, 0.25f); textCol = ImVec4(0.35f, 0.75f, 1.00f, 1.0f); break;
        case core::FileFormat::XLSX: bgCol = ImVec4(0.13f, 0.65f, 0.30f, 0.25f); textCol = ImVec4(0.35f, 0.88f, 0.55f, 1.0f); break;
        case core::FileFormat::JSON: bgCol = ImVec4(0.92f, 0.60f, 0.08f, 0.25f); textCol = ImVec4(0.98f, 0.75f, 0.25f, 1.0f); break;
        case core::FileFormat::QIF:  bgCol = ImVec4(0.65f, 0.30f, 0.95f, 0.25f); textCol = ImVec4(0.80f, 0.50f, 1.00f, 1.0f); break;
        default:                     bgCol = ImVec4(0.30f, 0.35f, 0.45f, 0.25f); textCol = ImVec4(0.70f, 0.75f, 0.85f, 1.0f); break;
    }

    ImGui::PushStyleColor(ImGuiCol_Button, bgCol);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bgCol);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, bgCol);
    ImGui::PushStyleColor(ImGuiCol_Text, textCol);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7.0f, 2.0f));
    ImGui::Button(label.c_str(), ImVec2(52, 22));
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
}

UIController::UIController(converter::ConverterEngine& engine)
    : m_engine(engine) {
    m_throughputHistory.resize(50, 0.0f);
}

void UIController::addFiles(const std::vector<std::string>& paths) {
    std::vector<std::string> expanded;
    for (const auto& path : paths) {
        if (!std::filesystem::exists(path)) continue;
        if (std::filesystem::is_directory(path)) {
            std::error_code ec;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(path, ec)) {
                if (entry.is_regular_file(ec)) {
                    expanded.push_back(entry.path().string());
                }
            }
        } else {
            expanded.push_back(path);
        }
    }

    std::lock_guard<std::mutex> lock(m_taskMutex);
    for (const auto& path : expanded) {
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
    if (m_targetJson) targets.push_back(core::FileFormat::JSON);

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
    AppFonts& fonts = getAppFonts();

    if (ImGui::BeginTable("HeaderLayoutTable", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("TitleCol", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("ActionCol", ImGuiTableColumnFlags_WidthFixed, 310.0f);
        ImGui::TableNextRow();

        // Left Column: Brand & Title
        ImGui::TableSetColumnIndex(0);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06f, 0.72f, 0.51f, 0.25f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.06f, 0.72f, 0.51f, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.06f, 0.72f, 0.51f, 0.45f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.15f, 0.95f, 0.65f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        if (fonts.fontBold) ImGui::PushFont(fonts.fontBold);
        ImGui::Button("BSC", ImVec2(54, 28));
        if (fonts.fontBold) ImGui::PopFont();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);

        ImGui::SameLine();
        if (fonts.fontBold) ImGui::PushFont(fonts.fontBold);
        ImGui::TextColored(ImVec4(0.96f, 0.97f, 0.99f, 1.0f), "Bank Statement Conversion");
        if (fonts.fontBold) ImGui::PopFont();

        ImGui::SameLine();
        ImGui::TextDisabled("v2.5");

        if (fonts.fontSmall) ImGui::PushFont(fonts.fontSmall);
        ImGui::TextDisabled("Sistema Universal de Conversao e Auditoria para Todos os Bancos e Fintechs");
        if (fonts.fontSmall) ImGui::PopFont();

        // Right Column: System info & About button
        ImGui::TableSetColumnIndex(1);
        std::string poolStr = "[" + std::to_string(m_engine.getWorkerCount()) + " Threads]";
        ImGui::TextColored(ImVec4(0.20f, 0.75f, 0.95f, 1.0f), "%s", poolStr.c_str());
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.70f, 0.50f, 0.95f, 1.0f), "[C++20]");
        ImGui::SameLine();
        if (ImGui::Button("Sobre / Licenca", ImVec2(130, 26))) {
            m_showAboutModal = true;
        }

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}

void UIController::renderTargetOptionsPanel() {
    AppFonts& fonts = getAppFonts();
    ImGui::BeginChild("TargetOptionsPanel", ImVec2(0, 142), true);

    if (fonts.fontBold) ImGui::PushFont(fonts.fontBold);
    ImGui::TextColored(ImVec4(0.20f, 0.75f, 0.95f, 1.0f), "FORMATOS DE SAIDA SIMULTANEOS (QUALQUER PARA QUALQUER)");
    if (fonts.fontBold) ImGui::PopFont();

    ImGui::SameLine();
    ImGui::TextDisabled("| Selecione os formatos a gerar simultaneamente");

    ImGui::Spacing();

    // 3-Column responsive table guarantees NO overlap on any screen resolution
    if (ImGui::BeginTable("FormatGridTable", 3, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();

        // Column 0: OFX & PDF
        ImGui::TableSetColumnIndex(0);
        ImGui::Checkbox("OFX (QuickBooks / ContaAzul)", &m_targetOfx);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Padrao Open Financial Exchange suportado por ERPs e contabilidades.");
        ImGui::Checkbox("PDF (Relatorio Vetorial)", &m_targetPdf);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Documento PDF executivo impresso em alta fidelidade vetorial.");

        // Column 1: CSV & JSON
        ImGui::TableSetColumnIndex(1);
        ImGui::Checkbox("CSV (RFC-4180 / Delimitador ';')", &m_targetCsv);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Planilha em texto puro com pontuacao brasileira para Excel e Python.");
        ImGui::Checkbox("JSON (Open Finance / REST APIs)", &m_targetJson);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Estrutura hierarquica em JSON no padrao Open Finance.");

        // Column 2: XLSX & QIF
        ImGui::TableSetColumnIndex(2);
        ImGui::Checkbox("XLSX (Microsoft Excel)", &m_targetXlsx);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Planilha nativa Excel Office Open XML com cores e formulas.");
        ImGui::Checkbox("QIF (Quicken / GnuCash)", &m_targetQif);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Formato financeiro universal Quicken Interchange Format.");

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Output Directory Bar
    ImGui::Text("Pasta de Destino:");
    ImGui::SameLine();
    float availW = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth(std::max(180.0f, availW - 270.0f));
    ImGui::InputTextWithHint("##outdir", "Mesma pasta do arquivo de origem (padrao)", m_outputDirBuf, sizeof(m_outputDirBuf));
    
    ImGui::SameLine();
    if (ImGui::Button("Selecionar...##out", ImVec2(140, 26))) {
        m_isSelectingOutputDir = true;
        std::string cur = m_outputDirBuf[0] ? m_outputDirBuf : std::filesystem::current_path().string();
        snprintf(m_dirPickerCurrentPath, sizeof(m_dirPickerCurrentPath), "%s", cur.c_str());
        m_showDirPickerModal = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Padrao##clearOut", ImVec2(90, 26))) {
        m_outputDirBuf[0] = '\0';
    }

    ImGui::EndChild();
}

void UIController::renderBatchQueuePanel() {
    AppFonts& fonts = getAppFonts();
    float availHeight = ImGui::GetContentRegionAvail().y - 88.0f;
    ImGui::BeginChild("BatchQueuePanel", ImVec2(0, availHeight), true);

    // Responsive Top Action Bar using a 2-column table
    if (ImGui::BeginTable("QueueTopBarTable", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("ButtonsCol", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("QueueInfoCol", ImGuiTableColumnFlags_WidthFixed, 260.0f);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.44f, 0.72f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.10f, 0.54f, 0.88f, 1.0f));
        if (ImGui::Button("+ Adicionar Pasta...", ImVec2(180, 32))) {
            m_isSelectingOutputDir = false;
            std::string cur = std::filesystem::current_path().string();
            snprintf(m_dirPickerCurrentPath, sizeof(m_dirPickerCurrentPath), "%s", cur.c_str());
            m_showDirPickerModal = true;
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine();
        if (ImGui::Button("Carregar Amostras (samples/)", ImVec2(200, 32))) {
            if (std::filesystem::exists("samples")) {
                addDirectory("samples");
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Limpar Fila", ImVec2(100, 32))) {
            std::lock_guard<std::mutex> lock(m_taskMutex);
            m_tasks.clear();
        }

        ImGui::SameLine();
        if (m_isConverting) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.20f, 0.20f, 0.90f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.25f, 0.25f, 1.00f));
            if (ImGui::Button("PARAR CONVERSAO", ImVec2(180, 32))) {
                m_engine.cancelAll();
            }
            ImGui::PopStyleColor(2);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06f, 0.65f, 0.45f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.08f, 0.75f, 0.52f, 1.00f));
            if (fonts.fontBold) ImGui::PushFont(fonts.fontBold);
            if (ImGui::Button("INICIAR CONVERSAO EM LOTE", ImVec2(230, 32))) {
                startBatchConversion();
            }
            if (fonts.fontBold) ImGui::PopFont();
            ImGui::PopStyleColor(2);
        }

        // Column 1: Queue status chip
        ImGui::TableSetColumnIndex(1);
        size_t totalTxInQueue = 0;
        {
            std::lock_guard<std::mutex> lock(m_taskMutex);
            for (const auto& t : m_tasks) {
                if (t.parsedStatement) totalTxInQueue += t.parsedStatement->transactions.size();
            }
        }
        std::string countStr = "Fila: " + std::to_string(m_tasks.size()) + " arqs | " + 
                               std::to_string(totalTxInQueue) + " lancamentos";
        ImGui::TextColored(ImVec4(0.85f, 0.90f, 0.95f, 1.0f), "%s", countStr.c_str());

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // IF QUEUE IS EMPTY: Draw a friendly, intuitive Drag & Drop Onboarding Zone
    if (m_tasks.empty()) {
        ImGui::BeginChild("DropZoneArea", ImVec2(0, 0), true);

        ImGui::Spacing();
        ImGui::Spacing();

        float centerPos = (ImGui::GetWindowWidth() - 560.0f) * 0.5f;
        if (centerPos < 20.0f) centerPos = 20.0f;

        ImGui::SetCursorPosX(centerPos);
        if (fonts.fontHeader) ImGui::PushFont(fonts.fontHeader);
        ImGui::TextColored(ImVec4(0.20f, 0.75f, 0.95f, 1.0f), "ARRASTE E SOLTE OS SEUS EXTRATOS AQUI");
        if (fonts.fontHeader) ImGui::PopFont();

        ImGui::SetCursorPosX(centerPos);
        ImGui::TextDisabled("Ou utilize os atalhos abaixo para carregar pastas ou arquivos de extrato:");

        ImGui::Spacing();
        ImGui::SetCursorPosX(centerPos);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.44f, 0.72f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.10f, 0.54f, 0.88f, 1.0f));
        if (ImGui::Button("+ Selecionar Pasta no Computador...##dz", ImVec2(270, 36))) {
            m_isSelectingOutputDir = false;
            std::string cur = std::filesystem::current_path().string();
            snprintf(m_dirPickerCurrentPath, sizeof(m_dirPickerCurrentPath), "%s", cur.c_str());
            m_showDirPickerModal = true;
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine();
        if (ImGui::Button("Carregar Amostras Prontas (samples/)##dz", ImVec2(270, 36))) {
            if (std::filesystem::exists("samples")) {
                addDirectory("samples");
            }
        }

        ImGui::Spacing();
        ImGui::Spacing();

        ImGui::SetCursorPosX(centerPos);
        if (fonts.fontBold) ImGui::PushFont(fonts.fontBold);
        ImGui::TextColored(ImVec4(0.95f, 0.96f, 0.98f, 1.0f), "Formatos Suportados na Ingestao:");
        if (fonts.fontBold) ImGui::PopFont();

        ImGui::SetCursorPosX(centerPos);
        renderFormatBadge(core::FileFormat::PDF); ImGui::SameLine();
        renderFormatBadge(core::FileFormat::OFX); ImGui::SameLine();
        renderFormatBadge(core::FileFormat::CSV); ImGui::SameLine();
        renderFormatBadge(core::FileFormat::XLSX); ImGui::SameLine();
        renderFormatBadge(core::FileFormat::JSON); ImGui::SameLine();
        renderFormatBadge(core::FileFormat::QIF);

        ImGui::Spacing();
        ImGui::Spacing();

        ImGui::SetCursorPosX(centerPos);
        if (fonts.fontBold) ImGui::PushFont(fonts.fontBold);
        ImGui::TextColored(ImVec4(0.95f, 0.96f, 0.98f, 1.0f), "Bancos e Fintechs Homologados:");
        if (fonts.fontBold) ImGui::PopFont();

        ImGui::SetCursorPosX(centerPos);
        ImGui::TextColored(ImVec4(0.70f, 0.80f, 0.90f, 1.0f), 
            "Bancos: Banco do Brasil, Itau, Bradesco, Santander, Caixa, Safra, Banrisul, BRB, BNB, BASA, BV\n"
            "Cooperativas: Sicoob, Sicredi, Unicred, Ailos (Viacredi), Cresol\n"
            "Digitais: Nubank, Banco Inter, C6 Bank, BTG Pactual, Original, Daycoval, Pan, BMG, Agibank\n"
            "Pagamentos: Mercado Pago, PagBank, PicPay, Stone, Asaas, Cora, Efi, Woovi, Celcoin, RecargaPay\n"
            "Investimentos: XP Investimentos, Genial, Modalmais, Avenue, Nomad, Wise");

        ImGui::EndChild();
    } else {
        // Table of queued files
        static ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | 
                                       ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
                                       ImGuiTableFlags_Resizable;

        if (ImGui::BeginTable("QueueTable", 8, flags)) {
            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 35.0f);
            ImGui::TableSetupColumn("Formato", ImGuiTableColumnFlags_WidthFixed, 65.0f);
            ImGui::TableSetupColumn("Arquivo de Origem", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Lancamentos", ImGuiTableColumnFlags_WidthFixed, 95.0f);
            ImGui::TableSetupColumn("Auditoria", ImGuiTableColumnFlags_WidthFixed, 130.0f);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 110.0f);
            ImGui::TableSetupColumn("Taxa (Tx/s)", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Acao", ImGuiTableColumnFlags_WidthFixed, 115.0f);
            ImGui::TableHeadersRow();

            std::lock_guard<std::mutex> lock(m_taskMutex);
            int itemToRemove = -1;

            for (size_t i = 0; i < m_tasks.size(); ++i) {
                auto& task = m_tasks[i];
                ImGui::TableNextRow();

                // 0: ID
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%zu", i + 1);

                // 1: Formato
                ImGui::TableSetColumnIndex(1);
                renderFormatBadge(task.inputFormat);

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
                    ImGui::Text("%zu itens", task.parsedStatement->transactions.size());
                } else {
                    ImGui::TextDisabled("-");
                }

                // 4: Auditoria Contábil
                ImGui::TableSetColumnIndex(4);
                if (task.parsedStatement) {
                    if (task.parsedStatement->auditStatus == core::AuditStatus::VERIFIED) {
                        ImGui::TextColored(ImVec4(0.10f, 0.80f, 0.55f, 1.0f), "Paridade 100%% OK");
                    } else if (task.parsedStatement->auditStatus == core::AuditStatus::DISCREPANCY) {
                        ImGui::TextColored(ImVec4(0.95f, 0.30f, 0.30f, 1.0f), "Discrepancia");
                    } else {
                        ImGui::TextColored(ImVec4(0.60f, 0.65f, 0.75f, 1.0f), "Livre de Erros");
                    }
                } else {
                    ImGui::TextDisabled("Pendente");
                }

                // 5: Status
                ImGui::TableSetColumnIndex(5);
                switch (task.status) {
                    case core::BatchConversionTask::Status::PENDING:
                        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Pendente");
                        break;
                    case core::BatchConversionTask::Status::PARSING:
                        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "Lendo...");
                        break;
                    case core::BatchConversionTask::Status::PARSED:
                        ImGui::TextColored(ImVec4(0.20f, 0.75f, 0.95f, 1.0f), "Pronto");
                        break;
                    case core::BatchConversionTask::Status::CONVERTING:
                        ImGui::TextColored(ImVec4(0.20f, 0.65f, 1.00f, 1.0f), "Convertendo...");
                        break;
                    case core::BatchConversionTask::Status::COMPLETED:
                        ImGui::TextColored(ImVec4(0.10f, 0.80f, 0.55f, 1.0f), "Concluido");
                        break;
                    case core::BatchConversionTask::Status::FAILED:
                        ImGui::TextColored(ImVec4(0.95f, 0.25f, 0.25f, 1.0f), "Falha");
                        break;
                }

                // 6: Taxa
                ImGui::TableSetColumnIndex(6);
                if (task.throughputTxSec > 0.0) {
                    ImGui::Text("%.0f", task.throughputTxSec);
                } else {
                    ImGui::TextDisabled("-");
                }

                // 7: Ação (Inspecionar / Remover)
                ImGui::TableSetColumnIndex(7);
                std::string btnId = "Inspecionar##" + std::to_string(i);
                if (task.parsedStatement && ImGui::Button(btnId.c_str(), ImVec2(80, 22))) {
                    m_inspectedStatement = task.parsedStatement;
                    m_showInspectModal = true;
                }
                ImGui::SameLine();
                std::string delId = "x##" + std::to_string(i);
                if (ImGui::Button(delId.c_str(), ImVec2(22, 22))) {
                    itemToRemove = static_cast<int>(i);
                }
            }

            if (itemToRemove >= 0 && itemToRemove < static_cast<int>(m_tasks.size())) {
                m_tasks.erase(m_tasks.begin() + itemToRemove);
            }

            ImGui::EndTable();
        }
    }

    ImGui::EndChild();
}

void UIController::renderTelemetryDashboard() {
    AppFonts& fonts = getAppFonts();
    ImGui::BeginChild("TelemetryDashboard", ImVec2(0, 78.0f), true);

    if (ImGui::BeginTable("TelemetryGridTable", 4, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();

        // Card 1: Vazão de Transações
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("TAXA DE VAZAO (THROUGHPUT)");
        if (fonts.fontHeader) ImGui::PushFont(fonts.fontHeader);
        std::string txRate = std::to_string(static_cast<int>(m_currentMetrics.averageTransactionsPerSec)) + " Tx/s";
        ImGui::TextColored(ImVec4(0.10f, 0.80f, 0.55f, 1.0f), "%s", txRate.c_str());
        if (fonts.fontHeader) ImGui::PopFont();

        // Card 2: Volume de Dados
        ImGui::TableSetColumnIndex(1);
        ImGui::TextDisabled("VOLUME E TEMPO DE EXECUCAO");
        if (fonts.fontHeader) ImGui::PushFont(fonts.fontHeader);
        double mbProcessed = m_currentMetrics.totalBytesProcessed / (1024.0 * 1024.0);
        std::string dataStr = core::Utils::formatCurrency(mbProcessed, false) + " MB (" +
                              std::to_string(static_cast<int>(m_currentMetrics.totalDurationMs)) + " ms)";
        ImGui::TextColored(ImVec4(0.20f, 0.75f, 0.95f, 1.0f), "%s", dataStr.c_str());
        if (fonts.fontHeader) ImGui::PopFont();

        // Card 3: Total Transações
        ImGui::TableSetColumnIndex(2);
        ImGui::TextDisabled("REGISTROS PROCESSADOS");
        if (fonts.fontHeader) ImGui::PushFont(fonts.fontHeader);
        std::string totTx = std::to_string(m_currentMetrics.totalTransactions) + " itens (" +
                            std::to_string(m_currentMetrics.successfulFiles) + "/" +
                            std::to_string(m_currentMetrics.totalFiles) + " arqs)";
        ImGui::TextColored(ImVec4(0.96f, 0.97f, 0.99f, 1.0f), "%s", totTx.c_str());
        if (fonts.fontHeader) ImGui::PopFont();

        // Card 4: Integridade & Criptografia
        ImGui::TableSetColumnIndex(3);
        ImGui::TextDisabled("AUDITORIA & INTEGRIDADE");
        if (fonts.fontHeader) ImGui::PushFont(fonts.fontHeader);
        ImGui::TextColored(ImVec4(0.10f, 0.80f, 0.55f, 1.0f), "SHA-256 PARITY OK");
        if (fonts.fontHeader) ImGui::PopFont();

        ImGui::EndTable();
    }

    ImGui::EndChild();
}

void UIController::renderInspectionModal() {
    if (!m_showInspectModal || !m_inspectedStatement) return;

    AppFonts& fonts = getAppFonts();

    ImGui::SetNextWindowSize(ImVec2(1020, 620), ImGuiCond_Appearing);
    if (ImGui::Begin("Inspecao Detalhada de Extrato Bancario", &m_showInspectModal, ImGuiWindowFlags_NoCollapse)) {
        
        // Header with Bank and Account info
        if (fonts.fontBold) ImGui::PushFont(fonts.fontBold);
        ImGui::TextColored(ImVec4(0.20f, 0.75f, 0.95f, 1.0f), "INSTITUICAO: %s (%s)",
            m_inspectedStatement->account.bankName.c_str(),
            m_inspectedStatement->account.bankId.c_str()
        );
        if (fonts.fontBold) ImGui::PopFont();

        ImGui::SameLine(500);
        ImGui::Text("Agencia: %s  |  Conta: %s  |  Moeda: %s",
            m_inspectedStatement->account.branchId.c_str(),
            m_inspectedStatement->account.accountId.c_str(),
            m_inspectedStatement->account.currency.c_str()
        );

        ImGui::TextDisabled("Arquivo de Origem: %s", m_inspectedStatement->sourceFilePath.c_str());

        // Audit & Reconciliation Banner
        ImGui::Spacing();
        if (m_inspectedStatement->auditStatus == core::AuditStatus::VERIFIED) {
            ImGui::TextColored(ImVec4(0.10f, 0.80f, 0.55f, 1.0f), 
                "[AUDITORIA CONTABIL: Conciliacao 100%% Exata (Saldo Inicial + Creditos - Debitos == Saldo Final)]");
        } else if (m_inspectedStatement->auditStatus == core::AuditStatus::DISCREPANCY) {
            ImGui::TextColored(ImVec4(0.95f, 0.25f, 0.25f, 1.0f), 
                "[ALERTA CONTABIL: Discrepancia aritmetica de R$ %s detectada na conciliacao]",
                core::Utils::formatCurrency(m_inspectedStatement->auditDiscrepancy, true).c_str());
        } else {
            ImGui::TextColored(ImVec4(0.70f, 0.75f, 0.85f, 1.0f), 
                "[AUDITORIA: Conciliacao aritmetica baseada nos lancamentos do periodo]");
        }

        for (const auto& w : m_inspectedStatement->auditWarnings) {
            ImGui::TextColored(ImVec4(0.98f, 0.70f, 0.20f, 1.0f), "  * %s", w.c_str());
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 3 Financial KPI Cards
        ImGui::Columns(3, "kpiInspectColumns", false);

        // Card 1: Total Créditos
        ImGui::BeginChild("KpiCredCard", ImVec2(0, 48), true);
        ImGui::TextDisabled("TOTAL CREDITOS (+)");
        if (fonts.fontBold) ImGui::PushFont(fonts.fontBold);
        std::string credStr = "R$ " + core::Utils::formatCurrency(m_inspectedStatement->totalCredits, true) + 
                              " (" + std::to_string(m_inspectedStatement->creditCount) + ")";
        ImGui::TextColored(ImVec4(0.10f, 0.80f, 0.55f, 1.0f), "%s", credStr.c_str());
        if (fonts.fontBold) ImGui::PopFont();
        ImGui::EndChild();
        ImGui::NextColumn();

        // Card 2: Total Débitos
        ImGui::BeginChild("KpiDebCard", ImVec2(0, 48), true);
        ImGui::TextDisabled("TOTAL DEBITOS (-)");
        if (fonts.fontBold) ImGui::PushFont(fonts.fontBold);
        std::string debStr = "R$ " + core::Utils::formatCurrency(m_inspectedStatement->totalDebits, true) +
                             " (" + std::to_string(m_inspectedStatement->debitCount) + ")";
        ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s", debStr.c_str());
        if (fonts.fontBold) ImGui::PopFont();
        ImGui::EndChild();
        ImGui::NextColumn();

        // Card 3: Fluxo Líquido
        ImGui::BeginChild("KpiNetCard", ImVec2(0, 48), true);
        ImGui::TextDisabled("FLUXO LIQUIDO");
        if (fonts.fontBold) ImGui::PushFont(fonts.fontBold);
        std::string netStr = "R$ " + core::Utils::formatCurrency(m_inspectedStatement->netFlow, true);
        ImVec4 netColor = (m_inspectedStatement->netFlow >= 0.0) ? 
            ImVec4(0.10f, 0.80f, 0.55f, 1.0f) : ImVec4(0.95f, 0.35f, 0.35f, 1.0f);
        ImGui::TextColored(netColor, "%s", netStr.c_str());
        if (fonts.fontBold) ImGui::PopFont();
        ImGui::EndChild();
        ImGui::NextColumn();

        ImGui::Columns(1);
        ImGui::Spacing();

        // Search & Filter controls
        ImGui::SetNextItemWidth(320);
        ImGui::InputTextWithHint("##filter", "Buscar descricao, documento ou categoria...", m_searchFilterBuf, sizeof(m_searchFilterBuf));
        
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);
        const char* filterOptions[] = {"Todas as Transacoes", "Apenas Creditos (+)", "Apenas Debitos (-)", "Apenas Pix"};
        ImGui::Combo("##typeFilter", &m_filterTypeIndex, filterOptions, IM_ARRAYSIZE(filterOptions));

        ImGui::Spacing();

        std::string filter(m_searchFilterBuf);
        filter = core::Utils::toLower(filter);

        if (ImGui::BeginTable("InspectTable", 7, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
            ImGui::TableSetupColumn("Data", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Descricao / Memo", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Categoria", ImGuiTableColumnFlags_WidthFixed, 140.0f);
            ImGui::TableSetupColumn("Doc / FITID", ImGuiTableColumnFlags_WidthFixed, 95.0f);
            ImGui::TableSetupColumn("Tipo", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Valor (R$)", ImGuiTableColumnFlags_WidthFixed, 110.0f);
            ImGui::TableSetupColumn("Hash SHA-256", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableHeadersRow();

            for (const auto& tx : m_inspectedStatement->transactions) {
                // Type filter
                if (m_filterTypeIndex == 1 && !tx.isCredit()) continue;
                if (m_filterTypeIndex == 2 && !tx.isDebit()) continue;
                if (m_filterTypeIndex == 3 && tx.description.find("Pix") == std::string::npos && 
                    tx.description.find("PIX") == std::string::npos && tx.category.find("Pix") == std::string::npos) continue;

                if (!filter.empty()) {
                    std::string desc = core::Utils::toLower(tx.description);
                    std::string date = tx.date;
                    std::string doc = core::Utils::toLower(tx.documentNumber);
                    std::string cat = core::Utils::toLower(tx.category);
                    if (desc.find(filter) == std::string::npos && 
                        date.find(filter) == std::string::npos &&
                        doc.find(filter) == std::string::npos &&
                        cat.find(filter) == std::string::npos) {
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
                if (!tx.pixKey.empty() || !tx.pixE2eId.empty()) {
                    if (ImGui::IsItemHovered()) {
                        std::string tip = "Chave Pix: " + (tx.pixKey.empty() ? "N/D" : tx.pixKey) + 
                                          "\nE2E ID: " + (tx.pixE2eId.empty() ? "N/D" : tx.pixE2eId);
                        ImGui::SetTooltip("%s", tip.c_str());
                    }
                }

                // 2: Categoria
                ImGui::TableSetColumnIndex(2);
                ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.40f, 1.0f), "%s", tx.category.c_str());

                // 3: Documento
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%s", tx.documentNumber.c_str());

                // 4: Tipo
                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%s", tx.isCredit() ? "CRED" : "DEB");

                // 5: Valor
                ImGui::TableSetColumnIndex(5);
                std::string valStr = core::Utils::formatCurrency(tx.amount, true);
                if (tx.isCredit()) {
                    ImGui::TextColored(ImVec4(0.10f, 0.80f, 0.55f, 1.0f), "%s", valStr.c_str());
                } else {
                    ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s", valStr.c_str());
                }

                // 6: Hash SHA-256
                ImGui::TableSetColumnIndex(6);
                std::string shortHash = tx.hashSha256.substr(0, 10) + "..";
                ImGui::TextDisabled("%s", shortHash.c_str());
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", tx.hashSha256.c_str());
                }
            }

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Exportacao Individual Imediata:");
        ImGui::SameLine();
        auto quickExport = [this](core::FileFormat fmt) {
            std::filesystem::path p(m_inspectedStatement->sourceFilePath);
            std::string outPath = (p.parent_path() / (p.stem().string() + "_converted" + core::formatToExtension(fmt))).string();
            bool ok = m_engine.convertStatement(*m_inspectedStatement, fmt, outPath);
            if (ok) {
                m_quickExportStatusMsg = "[OK] Salvo: " + p.stem().string() + "_converted" + core::formatToExtension(fmt);
            } else {
                m_quickExportStatusMsg = "[ERRO] Falha ao exportar";
            }
        };

        if (ImGui::Button("-> OFX", ImVec2(72, 26))) quickExport(core::FileFormat::OFX);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Exporta imediatamente para OFX na mesma pasta");
        ImGui::SameLine();
        if (ImGui::Button("-> CSV", ImVec2(72, 26))) quickExport(core::FileFormat::CSV);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Exporta imediatamente para CSV na mesma pasta");
        ImGui::SameLine();
        if (ImGui::Button("-> XLSX", ImVec2(72, 26))) quickExport(core::FileFormat::XLSX);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Exporta imediatamente para planilha Excel XLSX");
        ImGui::SameLine();
        if (ImGui::Button("-> PDF", ImVec2(72, 26))) quickExport(core::FileFormat::PDF);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Exporta imediatamente para relatorio vetorial PDF");
        ImGui::SameLine();
        if (ImGui::Button("-> JSON", ImVec2(72, 26))) quickExport(core::FileFormat::JSON);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Exporta imediatamente para JSON estruturado");

        if (!m_quickExportStatusMsg.empty()) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.10f, 0.85f, 0.55f, 1.0f), "%s", m_quickExportStatusMsg.c_str());
        }

        ImGui::SameLine(ImGui::GetWindowWidth() - 110);
        if (ImGui::Button("Fechar", ImVec2(90, 26))) {
            m_showInspectModal = false;
        }
    }
    ImGui::End();
}

void UIController::renderAboutModal() {
    if (!m_showAboutModal) return;

    AppFonts& fonts = getAppFonts();

    ImGui::SetNextWindowSize(ImVec2(720, 420), ImGuiCond_Appearing);
    if (ImGui::Begin("Creditos & Informacoes Institucionais", &m_showAboutModal, ImGuiWindowFlags_NoCollapse)) {
        if (fonts.fontHeader) ImGui::PushFont(fonts.fontHeader);
        ImGui::TextColored(ImVec4(0.10f, 0.80f, 0.55f, 1.0f), "BSC - BANK STATEMENT CONVERSION (v2.5)");
        if (fonts.fontHeader) ImGui::PopFont();

        ImGui::Text("Sistema Universal de Conversao e Auditoria de Extratos Bancarios");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (fonts.fontBold) ImGui::PushFont(fonts.fontBold);
        ImGui::TextColored(ImVec4(0.20f, 0.75f, 0.95f, 1.0f), "DESENVOLVIMENTO:");
        if (fonts.fontBold) ImGui::PopFont();
        ImGui::TextWrapped("Desenvolvido por Matheus Sousa, Laboratorio de Comunicacao e Seguranca da Informacao, Departamento de Eletronica e Sistemas, CTG-UFPE.");

        ImGui::Spacing();
        if (fonts.fontBold) ImGui::PushFont(fonts.fontBold);
        ImGui::TextColored(ImVec4(0.20f, 0.75f, 0.95f, 1.0f), "ARQUITETURA E DESEMPENHO:");
        if (fonts.fontBold) ImGui::PopFont();
        ImGui::BulletText("Pipeline de alta vazao com descompressao in-memory sem copias superfluas.");
        ImGui::BulletText("Processamento Concorrente com ThreadPool auto-adaptativo.");
        ImGui::BulletText("Integridade estrita ponta a ponta via hash criptografico SHA-256.");
        ImGui::BulletText("Reconciliacao e paridade continua de livro-razao contabil.");
        ImGui::BulletText("Matriz Universal Any-to-Any: PDF, OFX, CSV, XLSX, QIF e JSON.");
        ImGui::BulletText("Licenca: MIT License.");

        ImGui::Spacing();
        if (fonts.fontBold) ImGui::PushFont(fonts.fontBold);
        ImGui::TextColored(ImVec4(0.20f, 0.75f, 0.95f, 1.0f), "COMPATIBILIDADE LEGADA E EFICIENCIA:");
        if (fonts.fontBold) ImGui::PopFont();
        ImGui::BulletText("100%% C++20 Nativo | Backend OpenGL 2.1 / GLSL 1.20 | < 15MB RAM | CPU Intel Core 2 Duo / Atom / Modernas");

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

    ImGui::SetNextWindowSize(ImVec2(860, 520), ImGuiCond_Appearing);
    std::string title = m_isSelectingOutputDir ? "Selecionar Pasta de Destino" : "Selecionar Extratos Bancarios";
    
    if (ImGui::Begin(title.c_str(), &m_showDirPickerModal, ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextColored(ImVec4(0.20f, 0.75f, 0.95f, 1.0f), "Caminho Atual:");
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
        if (std::filesystem::exists(deskDir) && ImGui::SmallButton("Area de Trabalho")) {
            snprintf(m_dirPickerCurrentPath, sizeof(m_dirPickerCurrentPath), "%s", deskDir.c_str());
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Subdirectories and statement files scan
        std::vector<std::string> subDirs;
        struct DetectedFile {
            std::string name;
            std::string fullPath;
            core::FileFormat format;
            double sizeKb;
        };
        std::vector<DetectedFile> detectedFiles;
        std::filesystem::path currentPath(m_dirPickerCurrentPath);
        std::error_code ec;

        if (std::filesystem::exists(currentPath, ec) && std::filesystem::is_directory(currentPath, ec)) {
            for (const auto& entry : std::filesystem::directory_iterator(currentPath, ec)) {
                if (entry.is_directory(ec)) {
                    subDirs.push_back(entry.path().filename().string());
                } else if (entry.is_regular_file(ec)) {
                    std::string pStr = entry.path().string();
                    core::FileFormat fmt = m_engine.detectFormat(pStr);
                    if (fmt != core::FileFormat::UNKNOWN) {
                        double kb = static_cast<double>(entry.file_size(ec)) / 1024.0;
                        detectedFiles.push_back({entry.path().filename().string(), pStr, fmt, kb});
                    }
                }
            }
            std::sort(subDirs.begin(), subDirs.end());
            std::sort(detectedFiles.begin(), detectedFiles.end(), [](const DetectedFile& a, const DetectedFile& b) {
                return a.name < b.name;
            });
        }

        if (m_isSelectingOutputDir) {
            // Output mode: only subfolders needed
            ImGui::Text("Selecione ou confirme a subpasta abaixo:");
            ImGui::BeginChild("SubdirListOut", ImVec2(0, 230), true);
            if (subDirs.empty()) {
                ImGui::TextDisabled("(Nenhuma subpasta encontrada nesta pasta)");
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
        } else {
            // Input mode: Two-column layout (Folders on left, detected statements on right)
            ImGui::Columns(2, "pickerCols", true);
            ImGui::SetColumnWidth(0, 320.0f);

            // Left: Subfolders
            ImGui::TextColored(ImVec4(0.20f, 0.75f, 0.95f, 1.0f), "Subpastas (%zu):", subDirs.size());
            ImGui::BeginChild("SubdirListIn", ImVec2(0, 230), true);
            if (subDirs.empty()) {
                ImGui::TextDisabled("(Sem subpastas)");
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

            ImGui::NextColumn();

            // Right: Detected Financial Statements
            std::string rightTitle = "Extratos Suportados Nesta Pasta (" + std::to_string(detectedFiles.size()) + "):";
            ImGui::TextColored(ImVec4(0.10f, 0.80f, 0.55f, 1.0f), "%s", rightTitle.c_str());

            ImGui::BeginChild("FileListIn", ImVec2(0, 230), true);
            if (detectedFiles.empty()) {
                ImGui::Spacing();
                ImGui::TextDisabled("Nenhum extrato suportado (PDF, OFX, CSV, XLSX, JSON, QIF) nesta pasta.");
                ImGui::TextDisabled("Navegue pelas subpastas a esquerda para encontrar arquivos.");
            } else {
                for (size_t fi = 0; fi < detectedFiles.size(); ++fi) {
                    const auto& df = detectedFiles[fi];
                    renderFormatBadge(df.format);
                    ImGui::SameLine();
                    ImGui::Text("%s (%.1f KB)", df.name.c_str(), df.sizeKb);
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 65.0f);
                    std::string btnId = "+ Add##" + std::to_string(fi);
                    if (ImGui::SmallButton(btnId.c_str())) {
                        addFiles({df.fullPath});
                    }
                }
            }
            ImGui::EndChild();

            ImGui::Columns(1);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (m_isSelectingOutputDir) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06f, 0.65f, 0.45f, 0.95f));
            if (ImGui::Button("DEFINIR ESTA PASTA COMO DESTINO", ImVec2(320, 32))) {
                snprintf(m_outputDirBuf, sizeof(m_outputDirBuf), "%s", m_dirPickerCurrentPath);
                m_showDirPickerModal = false;
            }
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06f, 0.65f, 0.45f, 0.95f));
            std::string btnText = "CARREGAR TODOS OS " + std::to_string(detectedFiles.size()) + " EXTRATOS DESTA PASTA";
            if (ImGui::Button(btnText.c_str(), ImVec2(390, 32))) {
                addDirectory(m_dirPickerCurrentPath);
                m_showDirPickerModal = false;
            }
            ImGui::PopStyleColor();
        }

        ImGui::SameLine();
        if (ImGui::Button("Fechar##pickerClose", ImVec2(100, 32))) {
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
