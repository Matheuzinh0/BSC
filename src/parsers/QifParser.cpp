/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#include "parsers/QifParser.hpp"
#include "core/Utils.hpp"

namespace ofx::parsers {

bool QifParser::canParse(const std::string& filePath, const uint8_t* headerData, size_t headerSize) const {
    std::string_view header(reinterpret_cast<const char*>(headerData), std::min(headerSize, size_t(256)));
    if (header.find("!Type:") != std::string_view::npos) return true;
    return filePath.size() >= 4 && core::Utils::toLower(filePath.substr(filePath.size() - 4)) == ".qif";
}

std::shared_ptr<core::Statement> QifParser::parse(const std::string& filePath) const {
    auto statement = std::make_shared<core::Statement>();
    statement->sourceFilePath = filePath;
    statement->sourceFormat = core::FileFormat::QIF;

    std::string content = core::Utils::readFileString(filePath);
    if (content.empty()) return statement;

    std::vector<std::string> lines = core::Utils::split(content, '\n');

    core::Transaction currentTx;
    bool inTransaction = false;

    for (const auto& rawLine : lines) {
        std::string line = core::Utils::trim(rawLine);
        if (line.empty()) continue;

        char code = line[0];
        std::string val = (line.size() > 1) ? core::Utils::trim(line.substr(1)) : "";

        if (code == '!') {
            // Header e.g. !Type:Bank
            statement->account.accountType = val;
            continue;
        }

        if (code == '^') {
            // End of transaction
            if (inTransaction && !currentTx.date.empty() && currentTx.amount != 0.0) {
                if (currentTx.amount > 0.0) currentTx.type = core::TransactionType::CREDIT;
                else if (currentTx.amount < 0.0) currentTx.type = core::TransactionType::DEBIT;
                statement->transactions.push_back(std::move(currentTx));
            }
            currentTx = core::Transaction();
            inTransaction = false;
            continue;
        }

        inTransaction = true;
        switch (code) {
            case 'D': // Date
                currentTx.date = core::Utils::normalizeDate(val);
                break;
            case 'T': // Amount
                currentTx.amount = core::Utils::parseAmount(val);
                break;
            case 'N': // Check/Doc number
                currentTx.documentNumber = val;
                break;
            case 'P': // Payee
                if (!currentTx.description.empty()) currentTx.description = val + " - " + currentTx.description;
                else currentTx.description = val;
                break;
            case 'M': // Memo
                if (!currentTx.description.empty()) currentTx.description += " (" + val + ")";
                else currentTx.description = val;
                break;
            case 'L': // Category
                currentTx.category = val;
                break;
            default:
                break;
        }
    }

    if (inTransaction && !currentTx.date.empty() && currentTx.amount != 0.0) {
        if (currentTx.amount > 0.0) currentTx.type = core::TransactionType::CREDIT;
        else if (currentTx.amount < 0.0) currentTx.type = core::TransactionType::DEBIT;
        statement->transactions.push_back(std::move(currentTx));
    }

    statement->computeStatistics();
    return statement;
}

} // namespace ofx::parsers
