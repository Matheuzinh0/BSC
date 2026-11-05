/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#include "core/Types.hpp"
#include "core/Hasher.hpp"
#include "core/Utils.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <regex>
#include <unordered_set>
#include <cmath>

namespace ofx::core {

void Statement::computeStatistics() {
    totalCredits = 0.0;
    totalDebits = 0.0;
    creditCount = 0;
    debitCount = 0;

    std::string minDate = "9999-99-99";
    std::string maxDate = "0000-00-00";

    Sha256 stmtHasher;
    stmtHasher.update(account.bankId);
    stmtHasher.update(account.accountId);

    for (auto& tx : transactions) {
        // Compute transaction hash if not already set
        if (tx.hashSha256.empty()) {
            std::ostringstream ss;
            ss << tx.date << "|" << std::fixed << std::setprecision(2) << tx.amount 
               << "|" << tx.documentNumber << "|" << tx.description;
            tx.hashSha256 = Sha256::hashString(ss.str());
        }

        stmtHasher.update(tx.hashSha256);

        if (tx.amount > 0.0) {
            totalCredits += tx.amount;
            creditCount++;
            if (tx.type == TransactionType::OTHER) tx.type = TransactionType::CREDIT;
        } else if (tx.amount < 0.0) {
            totalDebits += std::abs(tx.amount);
            debitCount++;
            if (tx.type == TransactionType::OTHER) tx.type = TransactionType::DEBIT;
        }

        if (!tx.date.empty()) {
            if (tx.date < minDate) minDate = tx.date;
            if (tx.date > maxDate) maxDate = tx.date;
        }
    }

    netFlow = totalCredits - totalDebits;

    if (periodStart.empty() && minDate != "9999-99-99") {
        periodStart = minDate;
    }
    if (periodEnd.empty() && maxDate != "0000-00-00") {
        periodEnd = maxDate;
    }

    statementHashSha256 = stmtHasher.finalize();

    // Run automatic categorization and accounting audit
    autoCategorize();
    runAuditReconciliation();
}

void Statement::autoCategorize() {
    static const std::regex pixE2eRegex(R"(\b(E\d{31}[A-Za-z0-9]*)\b)");
    static const std::regex cpfCnpjRegex(R"(\b(?:\d{3}\.\d{3}\.\d{3}-\d{2}|\d{2}\.\d{3}\.\d{3}\/\d{4}-\d{2}|\d{11}|\d{14})\b)");
    static const std::regex emailRegex(R"(\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}\b)");
    static const std::regex evpRegex(R"(\b[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}\b)");

    for (auto& tx : transactions) {
        std::string upDesc = Utils::toUpper(tx.description);

        // Pix Key and E2E extraction
        if (upDesc.find("PIX") != std::string::npos) {
            std::smatch m;
            if (std::regex_search(tx.description, m, pixE2eRegex)) {
                tx.pixE2eId = m[1].str();
            }
            if (std::regex_search(tx.description, m, evpRegex)) {
                tx.pixKey = m[0].str();
            } else if (std::regex_search(tx.description, m, emailRegex)) {
                tx.pixKey = m[0].str();
            } else if (std::regex_search(tx.description, m, cpfCnpjRegex)) {
                tx.pixKey = m[0].str();
            }
        }

        if (!tx.category.empty()) continue; // Preserve manually set category

        // Rule-based classification
        if (upDesc.find("SALARIO") != std::string::npos || upDesc.find("FOLHA") != std::string::npos ||
            upDesc.find("PROVENTOS") != std::string::npos || upDesc.find("REMUNERACAO") != std::string::npos ||
            upDesc.find("REMUNERAÇÃO") != std::string::npos || upDesc.find("PRO-LABORE") != std::string::npos ||
            upDesc.find("INSS") != std::string::npos || upDesc.find("APOSENTADORIA") != std::string::npos) {
            tx.category = "Salario / Proventos";
        } else if (upDesc.find("IFOOD") != std::string::npos || upDesc.find("RAPPI") != std::string::npos ||
                   upDesc.find("RESTAURANTE") != std::string::npos || upDesc.find("LANCHONETE") != std::string::npos ||
                   upDesc.find("PADARIA") != std::string::npos || upDesc.find("MERCADO") != std::string::npos ||
                   upDesc.find("SUPERMERCADO") != std::string::npos || upDesc.find("ASSAI") != std::string::npos ||
                   upDesc.find("ATACADAO") != std::string::npos || upDesc.find("CARREFOUR") != std::string::npos ||
                   upDesc.find("MCDONALD") != std::string::npos || upDesc.find("BURGER KING") != std::string::npos) {
            tx.category = "Alimentacao / Mercado";
        } else if (upDesc.find("UBER") != std::string::npos || upDesc.find("99APP") != std::string::npos ||
                   upDesc.find("POSTO") != std::string::npos || upDesc.find("COMBUSTIVEL") != std::string::npos ||
                   upDesc.find("GASOLINA") != std::string::npos || upDesc.find("SHELL") != std::string::npos ||
                   upDesc.find("IPIRANGA") != std::string::npos || upDesc.find("PETROBRAS") != std::string::npos ||
                   upDesc.find("PEDAGIO") != std::string::npos || upDesc.find("ESTACIONAMENTO") != std::string::npos ||
                   upDesc.find("SEM PARAR") != std::string::npos || upDesc.find("VELOE") != std::string::npos) {
            tx.category = "Transporte / Combustivel";
        } else if (upDesc.find("ENEL") != std::string::npos || upDesc.find("CPFL") != std::string::npos ||
                   upDesc.find("LIGHT") != std::string::npos || upDesc.find("CEMIG") != std::string::npos ||
                   upDesc.find("NEOENERGIA") != std::string::npos || upDesc.find("SABESP") != std::string::npos ||
                   upDesc.find("COPASA") != std::string::npos || upDesc.find("COMPESA") != std::string::npos ||
                   upDesc.find("CLARO") != std::string::npos || upDesc.find("VIVO") != std::string::npos ||
                   upDesc.find("TIM") != std::string::npos || upDesc.find("INTERNET") != std::string::npos ||
                   upDesc.find("ENERGIA") != std::string::npos || upDesc.find("TELEFONE") != std::string::npos) {
            tx.category = "Utilidades / Contas";
        } else if (upDesc.find("DARF") != std::string::npos || upDesc.find("GPS") != std::string::npos ||
                   upDesc.find("FGTS") != std::string::npos || upDesc.find("DAS ") != std::string::npos ||
                   upDesc.find("SIMPLES NACIONAL") != std::string::npos || upDesc.find("IPVA") != std::string::npos ||
                   upDesc.find("IPTU") != std::string::npos || upDesc.find("IMPOSTO") != std::string::npos ||
                   upDesc.find("RECEITA FEDERAL") != std::string::npos || upDesc.find("TRIBUTO") != std::string::npos) {
            tx.category = "Tributos / Impostos";
        } else if (upDesc.find("TARIFA") != std::string::npos || upDesc.find("PACOTE") != std::string::npos ||
                   upDesc.find("ANUIDADE") != std::string::npos || upDesc.find("IOF") != std::string::npos ||
                   upDesc.find("MANUTENCAO") != std::string::npos || upDesc.find("TAXA") != std::string::npos ||
                   upDesc.find("JUROS") != std::string::npos || upDesc.find("ENCARGOS") != std::string::npos) {
            tx.category = "Tarifas / Bancarios";
            tx.type = TransactionType::FEE;
        } else if (upDesc.find("CDB") != std::string::npos || upDesc.find("LCI") != std::string::npos ||
                   upDesc.find("LCA") != std::string::npos || upDesc.find("TESOURO") != std::string::npos ||
                   upDesc.find("APLICACAO") != std::string::npos || upDesc.find("APLICAÇÃO") != std::string::npos ||
                   upDesc.find("RESGATE") != std::string::npos || upDesc.find("RENDIMENTO") != std::string::npos ||
                   upDesc.find("DIVIDENDO") != std::string::npos || upDesc.find("FUNDO") != std::string::npos) {
            tx.category = "Investimentos";
        } else if (upDesc.find("ALUGUEL") != std::string::npos || upDesc.find("CONDOMINIO") != std::string::npos ||
                   upDesc.find("CONDOMÍNIO") != std::string::npos || upDesc.find("IMOBILIARIA") != std::string::npos) {
            tx.category = "Habitacao / Aluguel";
        } else if (upDesc.find("FARMACIA") != std::string::npos || upDesc.find("FARMÁCIA") != std::string::npos ||
                   upDesc.find("DROGARIA") != std::string::npos || upDesc.find("DROGASIL") != std::string::npos ||
                   upDesc.find("HOSPITAL") != std::string::npos || upDesc.find("CLINICA") != std::string::npos ||
                   upDesc.find("LABORATORIO") != std::string::npos || upDesc.find("MEDICO") != std::string::npos) {
            tx.category = "Saude";
        } else if (upDesc.find("NETFLIX") != std::string::npos || upDesc.find("SPOTIFY") != std::string::npos ||
                   upDesc.find("AMAZON") != std::string::npos || upDesc.find("DISNEY") != std::string::npos ||
                   upDesc.find("CINEMA") != std::string::npos || upDesc.find("STEAM") != std::string::npos) {
            tx.category = "Lazer / Assinaturas";
        } else if (upDesc.find("VENDA") != std::string::npos || upDesc.find("RECEBIMENTO") != std::string::npos) {
            tx.category = "Vendas / Faturamento";
        } else if (upDesc.find("PIX") != std::string::npos) {
            tx.category = (tx.amount > 0.0) ? "Pix Recebido" : "Pix Enviado";
        } else if (upDesc.find("TED") != std::string::npos || upDesc.find("DOC") != std::string::npos ||
                   upDesc.find("TRANSF") != std::string::npos) {
            tx.category = "Transferencias";
            tx.type = TransactionType::TRANSFER;
        } else {
            tx.category = (tx.amount > 0.0) ? "Credito Avulso" : "Debito Avulso";
        }
    }
}

void Statement::runAuditReconciliation() {
    auditWarnings.clear();
    auditDiscrepancy = 0.0;

    // Check balance parity: Initial + Credits - Debits == Final
    if (hasInitialBalance && hasFinalBalance) {
        double expectedFinal = initialBalance + totalCredits - totalDebits;
        auditDiscrepancy = finalBalance - expectedFinal;
        if (std::abs(auditDiscrepancy) < 0.01) {
            auditStatus = AuditStatus::VERIFIED;
        } else {
            auditStatus = AuditStatus::DISCREPANCY;
            std::ostringstream ss;
            ss << "Discrepancia contabil: R$ " << std::fixed << std::setprecision(2) << auditDiscrepancy
               << " (Calculado: " << expectedFinal << " vs Declarado: " << finalBalance << ")";
            auditWarnings.push_back(ss.str());
        }
    } else {
        auditStatus = AuditStatus::INCOMPLETE_BALANCES;
    }

    // Duplicate transaction detection via SHA-256
    std::unordered_set<std::string> seenHashes;
    for (const auto& tx : transactions) {
        if (!tx.hashSha256.empty()) {
            if (seenHashes.find(tx.hashSha256) != seenHashes.end()) {
                auditWarnings.push_back("Possivel transacao duplicada: " + tx.date + " | R$ " + 
                                       std::to_string(tx.amount) + " | " + tx.description);
            }
            seenHashes.insert(tx.hashSha256);
        }
    }
}

} // namespace ofx::core
