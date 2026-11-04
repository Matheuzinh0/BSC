/**
 * =================================================================================================
 * Projeto: OmniStatement Studio
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#pragma once

#include "parsers/IParser.hpp"
#include <vector>
#include <string>

namespace ofx::parsers {

class CsvParser : public IParser {
public:
    core::FileFormat getFormat() const override { return core::FileFormat::CSV; }
    bool canParse(const std::string& filePath, const uint8_t* headerData, size_t headerSize) const override;
    std::shared_ptr<core::Statement> parse(const std::string& filePath) const override;

    static char detectDelimiter(const std::string& sample);
    static std::vector<std::vector<std::string>> parseCsvRows(const std::string& content, char delimiter);
};

} // namespace ofx::parsers
