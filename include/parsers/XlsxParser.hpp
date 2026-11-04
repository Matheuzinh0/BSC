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

class XlsxParser : public IParser {
public:
    core::FileFormat getFormat() const override { return core::FileFormat::XLSX; }
    bool canParse(const std::string& filePath, const uint8_t* headerData, size_t headerSize) const override;
    std::shared_ptr<core::Statement> parse(const std::string& filePath) const override;

private:
    std::vector<std::string> parseSharedStrings(const std::string& xml) const;
    std::vector<std::vector<std::string>> parseSheetRows(const std::string& xml, const std::vector<std::string>& sharedStrings) const;
};

} // namespace ofx::parsers
