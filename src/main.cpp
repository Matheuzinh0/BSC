/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#include "converter/ConverterEngine.hpp"
#include "ui/AppWindow.hpp"
#include "ui/UIController.hpp"
#include "core/Utils.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <csignal>

int main(int argc, char* argv[]) {
    std::ios_base::sync_with_stdio(false);

    bool headless = false;
    bool runAudit = false;
    std::vector<std::string> inputFiles;
    std::vector<ofx::core::FileFormat> cliTargets;
    std::string cliOutDir = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--headless" || arg == "--cli" || arg == "-c" || arg == "--batch") {
            headless = true;
        } else if (arg == "--audit" || arg == "--validate") {
            runAudit = true;
        } else if (arg == "--version" || arg == "-v") {
            std::cout << "BSC (Bank Statement Conversion) versao 2.5.0\n";
            std::cout << "Desenvolvido por Matheus Sousa, CTG-UFPE.\n";
            return 0;
        } else if (arg == "--to" && i + 1 < argc) {
            std::string toList = argv[++i];
            auto parts = ofx::core::Utils::split(toList, ',');
            for (const auto& p : parts) {
                auto fmt = ofx::core::stringToFormat(p);
                if (fmt != ofx::core::FileFormat::UNKNOWN) {
                    cliTargets.push_back(fmt);
                }
            }
        } else if (arg == "--out" && i + 1 < argc) {
            cliOutDir = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "===================================================================\n";
            std::cout << " BSC - Bank Statement Conversion (v2.5 - C++20)\n";
            std::cout << " Desenvolvido por Matheus Sousa, Laboratorio de Comunicacao e\n";
            std::cout << " Seguranca da Informacao, Departamento de Eletronica e Sistemas, CTG-UFPE.\n";
            std::cout << "===================================================================\n";
            std::cout << "Uso no Modo Grafico:  ./BSC [arquivos ou pastas...]\n";
            std::cout << "Uso no Modo Linha de Comando (CLI Batch Engine):\n";
            std::cout << "  ./BSC --batch <arquivo1> <arquivo2...> --to ofx,csv,xlsx,qif,pdf,json [--out ./saida] [--audit]\n\n";
            std::cout << "Formatos Suportados (Qualquer para Qualquer):\n";
            std::cout << "  PDF  -> OFX, CSV, XLSX, QIF, PDF, JSON\n";
            std::cout << "  OFX  -> CSV, XLSX, QIF, PDF, OFX, JSON\n";
            std::cout << "  CSV  -> OFX, XLSX, QIF, PDF, CSV, JSON\n";
            std::cout << "  XLSX -> OFX, CSV, QIF, PDF, XLSX, JSON\n";
            std::cout << "  QIF  -> OFX, CSV, XLSX, PDF, QIF, JSON\n";
            std::cout << "  JSON -> OFX, CSV, XLSX, QIF, PDF, JSON\n\n";
            std::cout << "Opcoes:\n";
            std::cout << "  --batch, -c       Ativa execucao em lote em modo linha de comando\n";
            std::cout << "  --to <formatos>   Formatos de destino separados por virgula (ofx,csv,xlsx,pdf,qif,json)\n";
            std::cout << "  --out <pasta>     Diretorio de gravacao dos arquivos gerados\n";
            std::cout << "  --audit           Executa auditoria e reconciliacao aritmetica de saldo e duplicatas\n";
            std::cout << "  --version, -v     Exibe versao e creditos\n";
            std::cout << "  --help, -h        Exibe esta mensagem de ajuda\n";
            return 0;
        } else {
            inputFiles.push_back(arg);
        }
    }

    ofx::converter::ConverterEngine engine;

    if (headless) {
        std::cout << "[BSC] Modo Headless Batch Ativado.\n";
        std::cout << "[BSC] Threads ativas no pool: " << engine.getWorkerCount() << "\n";

        if (cliTargets.empty()) {
            cliTargets = {ofx::core::FileFormat::OFX, ofx::core::FileFormat::CSV, ofx::core::FileFormat::XLSX};
        }

        std::vector<ofx::core::BatchConversionTask> tasks;
        for (const auto& in : inputFiles) {
            if (std::filesystem::is_directory(in)) {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(in)) {
                    if (entry.is_regular_file()) {
                        ofx::core::BatchConversionTask task;
                        task.inputFilePath = entry.path().string();
                        task.inputFormat = engine.detectFormat(task.inputFilePath);
                        if (task.inputFormat != ofx::core::FileFormat::UNKNOWN) {
                            task.targetFormats = cliTargets;
                            task.outputDirectory = cliOutDir;
                            tasks.push_back(std::move(task));
                        }
                    }
                }
            } else if (std::filesystem::exists(in)) {
                ofx::core::BatchConversionTask task;
                task.inputFilePath = in;
                task.inputFormat = engine.detectFormat(in);
                if (task.inputFormat != ofx::core::FileFormat::UNKNOWN) {
                    task.targetFormats = cliTargets;
                    task.outputDirectory = cliOutDir;
                    tasks.push_back(std::move(task));
                }
            }
        }

        if (tasks.empty()) {
            std::cerr << "[ERRO] Nenhum arquivo valido encontrado para conversao.\n";
            return 1;
        }

        std::cout << "[BSC] Processando " << tasks.size() << " arquivo(s) em lote...\n";

        std::atomic<bool> done{false};
        engine.processBatchAsync(
            tasks,
            [runAudit](const ofx::core::BatchConversionTask& task) {
                if (task.status == ofx::core::BatchConversionTask::Status::COMPLETED) {
                    std::cout << "  [OK] " << task.inputFilePath << " -> " 
                              << task.parsedStatement->transactions.size() << " transacoes ("
                              << task.totalTimeMs << " ms, " << task.throughputTxSec << " Tx/s)\n";

                    if (runAudit && task.parsedStatement) {
                        std::cout << "       Auditoria: " 
                                  << ofx::core::auditStatusToString(task.parsedStatement->auditStatus) << "\n";
                        for (const auto& w : task.parsedStatement->auditWarnings) {
                            std::cout << "       * ALERTA: " << w << "\n";
                        }
                    }
                } else if (task.status == ofx::core::BatchConversionTask::Status::FAILED) {
                    std::cerr << "  [FALHA] " << task.inputFilePath << ": " << task.errorMessage << "\n";
                }
            },
            [&done](const ofx::core::PipelineMetrics& metrics) {
                std::cout << "\n================ RESUMO DA VAZAO ================\n";
                std::cout << "Total de Arquivos: " << metrics.totalFiles << "\n";
                std::cout << "Sucesso: " << metrics.successfulFiles << " | Falhas: " << metrics.failedFiles << "\n";
                std::cout << "Total de Transacoes: " << metrics.totalTransactions << "\n";
                std::cout << "Tempo Total: " << metrics.totalDurationMs << " ms\n";
                std::cout << "Vazao Media: " << metrics.averageTransactionsPerSec << " Tx/s ("
                          << metrics.averageThroughputMBs << " MB/s)\n";
                std::cout << "=================================================\n";
                done = true;
            }
        );

        while (!done) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        return 0;
    }

    // Launch Modern Graphical User Interface
    ofx::ui::UIController controller(engine);

    // If files were passed on command line, queue them immediately
    if (!inputFiles.empty()) {
        controller.addFiles(inputFiles);
    }

    ofx::ui::AppWindow window(1180, 780, "BSC v2.5 - Bank Statement Conversion", controller);
    if (!window.init()) {
        std::cerr << "Falha ao inicializar janela grafica. Execute com --batch para o modo CLI.\n";
        return 1;
    }

    window.run();
    return 0;
}
