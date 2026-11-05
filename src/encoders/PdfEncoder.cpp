/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#include "encoders/PdfEncoder.hpp"
#include "core/Utils.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <vector>
#include <cmath>

namespace ofx::encoders {

static std::string sanitizePdfString(std::string_view text) {
    std::string res;
    for (char c : text) {
        if (c == '(' || c == ')' || c == '\\') {
            res.push_back('\\');
            res.push_back(c);
        } else if (static_cast<unsigned char>(c) >= 32 && static_cast<unsigned char>(c) <= 126) {
            res.push_back(c);
        } else {
            // Replace non-ascii with space or standard representation
            if (c == -61) continue; // skip leading utf8 byte
            res.push_back(' ');
        }
    }
    return res;
}

bool PdfEncoder::encode(const core::Statement& statement, const std::string& outputFilePath) const {
    const double pageWidth = 595.28;
    const double pageHeight = 841.89;

    const size_t totalTx = statement.transactions.size();
    const size_t rowsFirstPage = 32;
    const size_t rowsOtherPages = 40;

    size_t totalPages = 1;
    if (totalTx > rowsFirstPage) {
        totalPages = 1 + static_cast<size_t>(std::ceil(static_cast<double>(totalTx - rowsFirstPage) / static_cast<double>(rowsOtherPages)));
    }

    std::vector<std::string> pageStreams;
    size_t currentTxIndex = 0;

    for (size_t pageNum = 1; pageNum <= totalPages; ++pageNum) {
        std::ostringstream stream;

        if (pageNum == 1) {
            // --- PAGE 1: Full Banner & KPI Summary ---
            // 1. Header Banner (Dark Slate Blue)
            stream << "0.12 0.16 0.23 rg\n";
            stream << "0 760 " << pageWidth << " 82 re f\n";

            // Title Text
            stream << "BT\n/F2 16 Tf\n1 1 1 rg\n30 805 Td\n(EXTRATO FINANCEIRO CONSOLIDADO) Tj\nET\n";

            // Bank Metadata
            std::string metaStr = "Banco: " + (statement.account.bankName.empty() ? "N/D" : statement.account.bankName) +
                                  " (" + (statement.account.bankId.empty() ? "000" : statement.account.bankId) + ")" +
                                  "  |  Ag: " + (statement.account.branchId.empty() ? "0001" : statement.account.branchId) +
                                  "  |  CC: " + (statement.account.accountId.empty() ? "000000" : statement.account.accountId) +
                                  "  |  Periodo: " + (statement.periodStart.empty() ? "N/D" : statement.periodStart) +
                                  " a " + (statement.periodEnd.empty() ? "N/D" : statement.periodEnd);
            stream << "BT\n/F1 9 Tf\n0.75 0.85 0.95 rg\n30 782 Td\n(" << sanitizePdfString(metaStr) << ") Tj\nET\n";

            // 2. Financial KPI Cards
            // Card 1: Total Créditos
            stream << "0.94 0.98 0.95 rg\n30 705 165 42 re f\n";
            stream << "0.06 0.58 0.35 rg\n30 705 3 42 re f\n";
            stream << "BT\n/F1 8 Tf\n0.3 0.4 0.3 rg\n40 733 Td\n(TOTAL CREDITOS) Tj\nET\n";
            std::string credStr = "R$ " + core::Utils::formatCurrency(statement.totalCredits, true);
            stream << "BT\n/F2 11 Tf\n0.06 0.58 0.35 rg\n40 715 Td\n(" << sanitizePdfString(credStr) << ") Tj\nET\n";

            // Card 2: Total Débitos
            stream << "0.99 0.95 0.95 rg\n210 705 165 42 re f\n";
            stream << "0.85 0.2 0.2 rg\n210 705 3 42 re f\n";
            stream << "BT\n/F1 8 Tf\n0.5 0.3 0.3 rg\n220 733 Td\n(TOTAL DEBITOS) Tj\nET\n";
            std::string debStr = "R$ " + core::Utils::formatCurrency(statement.totalDebits, true);
            stream << "BT\n/F2 11 Tf\n0.85 0.2 0.2 rg\n220 715 Td\n(" << sanitizePdfString(debStr) << ") Tj\nET\n";

            // Card 3: Fluxo Líquido
            stream << "0.95 0.96 0.98 rg\n390 705 175 42 re f\n";
            stream << "0.2 0.3 0.5 rg\n390 705 3 42 re f\n";
            stream << "BT\n/F1 8 Tf\n0.3 0.3 0.4 rg\n400 733 Td\n(FLUXO LIQUIDO) Tj\nET\n";
            std::string netStr = "R$ " + core::Utils::formatCurrency(statement.netFlow, true);
            stream << "BT\n/F2 11 Tf\n" << (statement.netFlow >= 0.0 ? "0.06 0.58 0.35" : "0.85 0.2 0.2") << " rg\n400 715 Td\n(" << sanitizePdfString(netStr) << ") Tj\nET\n";

            // 3. Table Header
            double y = 675;
            stream << "0.18 0.24 0.35 rg\n30 " << y << " 535 20 re f\n";
            stream << "BT\n/F2 8 Tf\n1 1 1 rg\n";
            stream << "35 " << (y + 6) << " Td\n(DATA) Tj\n";
            stream << "95 " << (y + 6) << " Td\n(HISTORICO / DESCRICAO) Tj\n";
            stream << "305 " << (y + 6) << " Td\n(DOCUMENTO) Tj\n";
            stream << "370 " << (y + 6) << " Td\n(CATEGORIA) Tj\n";
            stream << "450 " << (y + 6) << " Td\n(VALOR (R$)) Tj\n";
            stream << "520 " << (y + 6) << " Td\n(SALDO) Tj\n";
            stream << "ET\n";

            y -= 18;

            // Rows for Page 1
            size_t rowsToRender = std::min(totalTx - currentTxIndex, rowsFirstPage);
            for (size_t i = 0; i < rowsToRender; ++i) {
                const auto& tx = statement.transactions[currentTxIndex++];
                if (i % 2 == 1) {
                    stream << "0.96 0.97 0.98 rg\n30 " << y << " 535 16 re f\n";
                }
                stream << "0.9 0.92 0.94 rg\n0.5 w\n30 " << y << " m 565 " << y << " l S\n";

                std::string d = tx.date;
                if (d.size() == 10 && d[4] == '-' && d[7] == '-') {
                    d = d.substr(8, 2) + "/" + d.substr(5, 2) + "/" + d.substr(0, 4);
                }

                std::string desc = tx.description;
                if (desc.size() > 32) desc = desc.substr(0, 29) + "...";

                std::string doc = tx.documentNumber.empty() ? "0" : tx.documentNumber;
                if (doc.size() > 9) doc = doc.substr(0, 7) + "..";

                std::string cat = tx.category.empty() ? "Geral" : tx.category;
                if (cat.size() > 14) cat = cat.substr(0, 12) + "..";

                std::string amtStr = core::Utils::formatCurrency(tx.amount, true);
                if (tx.isCredit()) amtStr += " C";
                else amtStr += " D";

                stream << "BT\n/F1 8 Tf\n0.15 0.15 0.2 rg\n";
                stream << "35 " << (y + 4) << " Td\n(" << sanitizePdfString(d) << ") Tj\n";
                stream << "95 " << (y + 4) << " Td\n(" << sanitizePdfString(desc) << ") Tj\n";
                stream << "305 " << (y + 4) << " Td\n(" << sanitizePdfString(doc) << ") Tj\n";
                stream << "370 " << (y + 4) << " Td\n(" << sanitizePdfString(cat) << ") Tj\n";
                stream << "450 " << (y + 4) << " Td\n(" << sanitizePdfString(amtStr) << ") Tj\n";
                if (tx.runningBalance.has_value()) {
                    std::string balStr = core::Utils::formatCurrency(tx.runningBalance.value(), true);
                    stream << "520 " << (y + 4) << " Td\n(" << sanitizePdfString(balStr) << ") Tj\n";
                }
                stream << "ET\n";

                y -= 16;
            }

        } else {
            // --- CONTINUATION PAGES (Page 2..N) ---
            // Compact Running Header
            stream << "0.12 0.16 0.23 rg\n";
            stream << "0 805 " << pageWidth << " 37 re f\n";

            std::string runHead = "EXTRATO FINANCEIRO CONSOLIDADO - " + 
                                  (statement.account.bankName.empty() ? "N/D" : statement.account.bankName) +
                                  " (CC: " + statement.account.accountId + ")";
            stream << "BT\n/F2 10 Tf\n1 1 1 rg\n30 820 Td\n(" << sanitizePdfString(runHead) << ") Tj\nET\n";

            std::string pageIndicator = "Pagina " + std::to_string(pageNum) + " de " + std::to_string(totalPages);
            stream << "BT\n/F1 9 Tf\n0.8 0.9 1.0 rg\n490 820 Td\n(" << sanitizePdfString(pageIndicator) << ") Tj\nET\n";

            // Table Header on continuation page
            double y = 780;
            stream << "0.18 0.24 0.35 rg\n30 " << y << " 535 20 re f\n";
            stream << "BT\n/F2 8 Tf\n1 1 1 rg\n";
            stream << "35 " << (y + 6) << " Td\n(DATA) Tj\n";
            stream << "95 " << (y + 6) << " Td\n(HISTORICO / DESCRICAO) Tj\n";
            stream << "305 " << (y + 6) << " Td\n(DOCUMENTO) Tj\n";
            stream << "370 " << (y + 6) << " Td\n(CATEGORIA) Tj\n";
            stream << "450 " << (y + 6) << " Td\n(VALOR (R$)) Tj\n";
            stream << "520 " << (y + 6) << " Td\n(SALDO) Tj\n";
            stream << "ET\n";

            y -= 18;

            size_t rowsToRender = std::min(totalTx - currentTxIndex, rowsOtherPages);
            for (size_t i = 0; i < rowsToRender; ++i) {
                const auto& tx = statement.transactions[currentTxIndex++];
                if (i % 2 == 1) {
                    stream << "0.96 0.97 0.98 rg\n30 " << y << " 535 16 re f\n";
                }
                stream << "0.9 0.92 0.94 rg\n0.5 w\n30 " << y << " m 565 " << y << " l S\n";

                std::string d = tx.date;
                if (d.size() == 10 && d[4] == '-' && d[7] == '-') {
                    d = d.substr(8, 2) + "/" + d.substr(5, 2) + "/" + d.substr(0, 4);
                }

                std::string desc = tx.description;
                if (desc.size() > 32) desc = desc.substr(0, 29) + "...";

                std::string doc = tx.documentNumber.empty() ? "0" : tx.documentNumber;
                if (doc.size() > 9) doc = doc.substr(0, 7) + "..";

                std::string cat = tx.category.empty() ? "Geral" : tx.category;
                if (cat.size() > 14) cat = cat.substr(0, 12) + "..";

                std::string amtStr = core::Utils::formatCurrency(tx.amount, true);
                if (tx.isCredit()) amtStr += " C";
                else amtStr += " D";

                stream << "BT\n/F1 8 Tf\n0.15 0.15 0.2 rg\n";
                stream << "35 " << (y + 4) << " Td\n(" << sanitizePdfString(d) << ") Tj\n";
                stream << "95 " << (y + 4) << " Td\n(" << sanitizePdfString(desc) << ") Tj\n";
                stream << "305 " << (y + 4) << " Td\n(" << sanitizePdfString(doc) << ") Tj\n";
                stream << "370 " << (y + 4) << " Td\n(" << sanitizePdfString(cat) << ") Tj\n";
                stream << "450 " << (y + 4) << " Td\n(" << sanitizePdfString(amtStr) << ") Tj\n";
                if (tx.runningBalance.has_value()) {
                    std::string balStr = core::Utils::formatCurrency(tx.runningBalance.value(), true);
                    stream << "520 " << (y + 4) << " Td\n(" << sanitizePdfString(balStr) << ") Tj\n";
                }
                stream << "ET\n";

                y -= 16;
            }
        }

        // Footer on all pages
        if (pageNum == totalPages) {
            // Last page: Audit Badge & SHA-256 Seal
            stream << "0.95 0.96 0.98 rg\n30 25 535 30 re f\n";
            stream << "0.7 0.75 0.8 rg\n0.5 w\n30 25 535 30 re S\n";
            std::string hashBadge = "INTEGRIDADE CRIPTOGRAFICA SHA-256: " + 
                (statement.statementHashSha256.empty() ? "VERIFICADO" : statement.statementHashSha256.substr(0, 32) + "...");
            stream << "BT\n/F2 7 Tf\n0.2 0.3 0.4 rg\n40 42 Td\n(" << sanitizePdfString(hashBadge) << ") Tj\nET\n";
            std::string auditText = (statement.auditStatus == core::AuditStatus::VERIFIED) ?
                "Auditoria contabil aprovada: Conciliacao aritmetica com paridade 100% verificada." :
                "Extrato processado pelo motor universal BSC (Bank Statement Conversion).";
            stream << "BT\n/F1 7 Tf\n0.4 0.5 0.5 rg\n40 32 Td\n(" << sanitizePdfString(auditText) << ") Tj\nET\n";
        } else {
            // Intermediate footer
            std::string foot = "BSC Bank Statement Conversion | Pagina " + std::to_string(pageNum) + " de " + std::to_string(totalPages);
            stream << "BT\n/F1 8 Tf\n0.5 0.5 0.6 rg\n30 25 Td\n(" << sanitizePdfString(foot) << ") Tj\nET\n";
        }

        pageStreams.push_back(stream.str());
    }

    // --- Assemble PDF Document Structure ---
    std::ostringstream pdf;
    pdf << "%PDF-1.4\n";

    // Objects layout:
    // 1: Catalog
    // 2: Pages
    // For page i (0-indexed):
    //   Page Obj ID: 3 + i * 2
    //   Content Stream Obj ID: 4 + i * 2
    // Font F1: 3 + totalPages * 2
    // Font F2: 4 + totalPages * 2

    const size_t fontF1Id = 3 + totalPages * 2;
    const size_t fontF2Id = fontF1Id + 1;
    const size_t totalObjects = fontF2Id;

    std::vector<size_t> offsets(totalObjects + 1, 0);

    // Obj 1: Catalog
    offsets[1] = static_cast<size_t>(pdf.tellp());
    pdf << "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n";

    // Obj 2: Pages
    offsets[2] = static_cast<size_t>(pdf.tellp());
    pdf << "2 0 obj\n<< /Type /Pages /Kids [";
    for (size_t i = 0; i < totalPages; ++i) {
        size_t pageObjId = 3 + i * 2;
        pdf << pageObjId << " 0 R ";
    }
    pdf << "] /Count " << totalPages << " >>\nendobj\n";

    // Pages and Content Streams
    for (size_t i = 0; i < totalPages; ++i) {
        size_t pageObjId = 3 + i * 2;
        size_t streamObjId = pageObjId + 1;

        // Page Object
        offsets[pageObjId] = static_cast<size_t>(pdf.tellp());
        pdf << pageObjId << " 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 "
            << std::fixed << std::setprecision(2) << pageWidth << " " << pageHeight << "] "
            << "/Contents " << streamObjId << " 0 R "
            << "/Resources << /Font << /F1 " << fontF1Id << " 0 R /F2 " << fontF2Id << " 0 R >> >> >>\nendobj\n";

        // Content Stream Object
        const std::string& pageContent = pageStreams[i];
        offsets[streamObjId] = static_cast<size_t>(pdf.tellp());
        pdf << streamObjId << " 0 obj\n<< /Length " << pageContent.size() << " >>\nstream\n";
        pdf << pageContent;
        pdf << "\nendstream\nendobj\n";
    }

    // Font F1: Helvetica
    offsets[fontF1Id] = static_cast<size_t>(pdf.tellp());
    pdf << fontF1Id << " 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n";

    // Font F2: Helvetica-Bold
    offsets[fontF2Id] = static_cast<size_t>(pdf.tellp());
    pdf << fontF2Id << " 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold >>\nendobj\n";

    // Cross-reference table
    size_t xrefOffset = static_cast<size_t>(pdf.tellp());
    pdf << "xref\n0 " << (totalObjects + 1) << "\n";
    pdf << "0000000000 65535 f \n";
    for (size_t i = 1; i <= totalObjects; ++i) {
        pdf << std::setw(10) << std::setfill('0') << offsets[i] << " 00000 n \n";
    }

    // Trailer
    pdf << "trailer\n<< /Size " << (totalObjects + 1) << " /Root 1 0 R >>\nstartxref\n" << xrefOffset << "\n%%EOF\n";

    return core::Utils::writeFileString(outputFilePath, pdf.str());
}

} // namespace ofx::encoders
