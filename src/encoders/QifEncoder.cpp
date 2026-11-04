/**
 * =================================================================================================
 * Projeto: OmniStatement Studio
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#include "encoders/QifEncoder.hpp"
#include "core/Utils.hpp"
#include <sstream>
#include <iomanip>

namespace ofx::encoders {

bool QifEncoder::encode(const core::Statement& statement, const std::string& outputFilePath) const {
    std::ostringstream ss;

    ss << "!Type:Bank\r\n";

    for (const auto& tx : statement.transactions) {
        // Format date as DD/MM/YYYY
        std::string d = tx.date;
        if (d.size() == 10 && d[4] == '-' && d[7] == '-') {
            d = d.substr(8, 2) + "/" + d.substr(5, 2) + "/" + d.substr(0, 4);
        }

        ss << "D" << d << "\r\n";
        ss << "T" << std::fixed << std::setprecision(2) << tx.amount << "\r\n";

        if (!tx.documentNumber.empty()) {
            ss << "N" << tx.documentNumber << "\r\n";
        }

        if (!tx.description.empty()) {
            ss << "P" << tx.description << "\r\n";
            ss << "M" << tx.description << "\r\n";
        }

        if (!tx.category.empty()) {
            ss << "L" << tx.category << "\r\n";
        }

        ss << "^\r\n";
    }

    return core::Utils::writeFileString(outputFilePath, ss.str());
}

} // namespace ofx::encoders
