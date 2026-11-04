/**
 * =================================================================================================
 * Projeto: OmniStatement Studio
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#pragma once

#include "parsers/IParser.hpp"

namespace ofx::parsers {

class OfxParser : public IParser {
public:
    core::FileFormat getFormat() const override { return core::FileFormat::OFX; }
    bool canParse(const std::string& filePath, const uint8_t* headerData, size_t headerSize) const override;
    std::shared_ptr<core::Statement> parse(const std::string& filePath) const override;

private:
    std::string extractTagValue(std::string_view content, std::string_view tag) const;
    void parseTransactions(std::string_view ofxContent, core::Statement& statement) const;
};

} // namespace ofx::parsers
