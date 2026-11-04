/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#include "encoders/CsvEncoder.hpp"
#include "core/Utils.hpp"
#include <sstream>
#include <iomanip>

namespace ofx::encoders {

std::string CsvEncoder::escapeCell(std::string_view cell) const {
    bool needsQuotes = false;
    for (char c : cell) {
        if (c == m_delimiter || c == '"' || c == '\n' || c == '\r') {
            needsQuotes = true;
            break;
        }
    }

    if (!needsQuotes) return std::string(cell);

    std::string res;
    res.reserve(cell.size() + 8);
    res.push_back('"');
    for (char c : cell) {
        if (c == '"') res.push_back('"'); // escape double quote
        res.push_back(c);
    }
    res.push_back('"');
    return res;
}

bool CsvEncoder::encode(const core::Statement& statement, const std::string& outputFilePath) const {
    std::ostringstream ss;
    char d = m_delimiter;

    // Header row
    ss << "Data" << d 
       << "Descricao" << d 
       << "Documento" << d 
       << "Tipo" << d 
       << "Valor" << d 
       << "Saldo" << d 
       << "HashSha256\r\n";

    for (const auto& tx : statement.transactions) {
        ss << escapeCell(tx.date) << d
           << escapeCell(tx.description) << d
           << escapeCell(tx.documentNumber) << d
           << (tx.isCredit() ? "CREDITO" : "DEBITO") << d
           << std::fixed << std::setprecision(2) << tx.amount << d;

        if (tx.runningBalance.has_value()) {
            ss << std::fixed << std::setprecision(2) << tx.runningBalance.value();
        }
        ss << d;
        ss << escapeCell(tx.hashSha256) << "\r\n";
    }

    return core::Utils::writeFileString(outputFilePath, ss.str());
}

} // namespace ofx::encoders
