/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#pragma once

#include "parsers/IParser.hpp"

namespace ofx::parsers {

class PdfParser : public IParser {
public:
    core::FileFormat getFormat() const override { return core::FileFormat::PDF; }
    bool canParse(const std::string& filePath, const uint8_t* headerData, size_t headerSize) const override;
    std::shared_ptr<core::Statement> parse(const std::string& filePath) const override;
    std::shared_ptr<core::Statement> parseFromText(const std::string& text) const;

    void parseBankMetadata(const std::string& text, core::Statement& statement) const;
    void parseTransactionsFromText(const std::string& text, core::Statement& statement) const;
    std::string extractRawTextFromPdf(const std::vector<uint8_t>& pdfBytes) const;
};

} // namespace ofx::parsers
